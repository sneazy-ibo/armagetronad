/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  
***************************************************************************

*/

#include "gServerBrowser.h"
#include "gGame.h"
#include "gLogo.h"
#include "gServerFavorites.h"
#include "gFriends.h"

#include "nServerInfo.h"
#include "nNetwork.h"

#include "rSysdep.h"
#include "rScreen.h"
#include "rConsole.h"
#include "rRender.h"

#include "uMenu.h"
#include "uInputQueue.h"

#include "tMemManager.h"
#include "tSysTime.h"
#include "tToDo.h"

#include "tDirectories.h"
#include "tConfiguration.h"

int gServerBrowser::lowPort  = 4534;

int gServerBrowser::highPort = 4540;
static bool continuePoll = false;
static int sg_simultaneous = 20;
static tSettingItem< int > sg_simultaneousConf( "BROWSER_QUERIES_SIMULTANEOUS", sg_simultaneous );

static tOutput *sg_StartHelpText = NULL;

nServerInfo::QueryType sg_queryType = nServerInfo::QUERY_OPTOUT;
tCONFIG_ENUM( nServerInfo::QueryType );
static tSettingItem< nServerInfo::QueryType > sg_query_type( "BROWSER_QUERY_FILTER", sg_queryType );

class gServerMenuItem;

static nServerInfo::PrimaryKey sg_sortKey = nServerInfo::KEY_USERS;
tCONFIG_ENUM( nServerInfo::PrimaryKey );
static tConfItem< nServerInfo::PrimaryKey > sg_sortKeyConf( "BROWSER_SORT_KEY", sg_sortKey );

//  enabling this will cause friend's names to be case sensitive
static bool sg_enableFriendsCasing = true;
static tConfItem< bool > sg_enableFriendsCasingConf("ENABLE_FRIENDS_CASING", sg_enableFriendsCasing);

class gServerInfo: public nServerInfo
{
public:
    gServerMenuItem *menuItem;
    bool show; //for server browser hiding

    gServerInfo():menuItem(NULL), show(true)
    {
    }

    virtual ~gServerInfo();

    // during browsing, the whole server list consists of gServerInfos
    static gServerInfo * GetFirstServer()
    {
        return dynamic_cast< gServerInfo * >( nServerInfo::GetFirstServer() );
    }

    gServerInfo * Next()
    {
        return dynamic_cast< gServerInfo * >( nServerInfo::Next() );
    }
};

nServerInfo* CreateGServer()
{
    nServerInfo *ret = tNEW(gServerInfo);

    //if (!continuePoll)
    //{
    //    nServerInfo::StartQueryAll( sg_queryType );
    //    continuePoll = true;
    // }

    return ret;
}


class gServerMenu: public uMenu
{
public:
    virtual void OnRender();
    tString filter_string;

    void Update(); // sort the server view by score
    gServerMenu(const char *title);
    ~gServerMenu();

    //! Put the cursor on the filter field. Finds it by type, so it does not
    //! depend on the order the rows ended up in.
    void SelectFilter();

    //! Draws the shared list chrome: the background, the server count queries and
    //! the column headers. Every row that belongs to the browser runs this, so the
    //! headers and the ongoing polling do not stop when the filter has the cursor.
    void RenderListBackground();

    //! Forces the two fixed rows to the top of the list, host game above the
    //! filter, wherever ReverseItems() happened to shuffle them. Positions are
    //! assigned by identity, so they no longer depend on list length.
    void PinFixedItems();

    virtual void HandleEvent( SDL_Event event );

    void Render(REAL y,
                const tString &servername, const tOutput &score,
                const tOutput &users     , const tOutput &ping);

    void Render(REAL y,
                const tString &servername, const tString &score,
                const tString &users     , const tString &ping);
    
    void Render(REAL x, REAL y, tString &c, int center, int cursor, int cursorPos);
};

class gBrowserMenuItem: public uMenuItem
{
protected:
    bool displayHelp_;
    REAL helpAlpha_;
    
    gBrowserMenuItem(uMenu *M,const tOutput &help): uMenuItem( M, help )
      , displayHelp_{false}
      , helpAlpha_(0.0f)
    {
    }

    // handles a key press
    virtual bool Event( SDL_Event& event );

    virtual void RenderBackground();

    virtual bool DisplayHelp( bool display, REAL y, REAL alpha )
    {
        helpAlpha_ = alpha;
        displayHelp_ = display;
        return false;
    }
};

class gServerFilterMenuItem: public uMenuItemString
{
public:
    gServerFilterMenuItem(gServerMenu *M)
        :uMenuItemString(M,"0xffffffF0xccf5ffi0x99ebffl0x66e0fft0x33d6ffe0x00ccffr","",M->filter_string)
    {}
        
    virtual ~gServerFilterMenuItem(){}
    
    virtual void Render(REAL x,REAL y,REAL alpha=1, bool selected=0);
    virtual bool Event( SDL_Event& event );

    //! the filter row must keep drawing the shared list chrome (background,
    //! column headers, query polling) while it has the cursor, exactly like a
    //! server row does. Otherwise the headers vanish and the list stops loading.
    virtual void RenderBackground();
};

class gServerMenuItem: public gBrowserMenuItem
{
protected:
    gServerInfo *server;
    double      lastPing_; //!< the time of the last manual ping
    bool        favorite_; //!< flag indicating whether this is a favorite
public:
    void AddFavorite();
    void RemoveFavorite();
    void SetServer(nServerInfo *s);
    gServerInfo *GetServer();

    void Render(REAL x,REAL y,REAL alpha=1, bool selected=0) override;
    void RenderBackground() override;

    void Enter() override;
    void Select() override;

    // handles a key press
    bool Event( SDL_Event& event ) override;

    gServerMenuItem(gServerMenu *men);
    virtual ~gServerMenuItem();
};

class gServerStartMenuItem: public gBrowserMenuItem
{
public:
    virtual void Render(REAL x,REAL y,REAL alpha=1, bool selected=0);

    virtual void Enter();

    gServerStartMenuItem(gServerMenu *men);
    virtual ~gServerStartMenuItem();
};






static bool sg_RequestLANcontinuously = false;

void gServerBrowser::BrowseMaster()
{
    BrowseSpecialMaster(0,"");
}

// the currently active master
static nServerInfoBase * sg_currentMaster = 0;
nServerInfoBase * gServerBrowser::CurrentMaster()
{
    return sg_currentMaster;
}


void gServerBrowser::BrowseSpecialMaster( nServerInfoBase * master, char const * prefix )
{
    sg_currentMaster = master;

    sg_RequestLANcontinuously = false;

    sn_ServerInfoCreator *cback = nServerInfo::SetCreator(&CreateGServer);

    sr_con.autoDisplayAtNewline=true;
    sr_con.fullscreen=true;

#ifndef DEDICATED
    rSysDep::SwapGL();
    rSysDep::ClearGL();
    rSysDep::SwapGL();
    rSysDep::ClearGL();
#endif

    bool to=sr_textOut;
    sr_textOut=true;

    nServerInfo::DeleteAll();
    nServerInfo::GetFromMaster( master, prefix );
    nServerInfo::Save();

#ifdef SERVER_SURVEY
    // connect to all servers and log stats
    nServerInfo * info = nServerInfo::GetFirstServer();
    {
        std::ofstream o;
        if ( tDirectories::Var().Open(o, "serversurvey.txt", std::ios::app) )
        {
            o << "New survey\n";
        }
    }
    while( info )
    {
        ConnectToServer(info);
        info = info->Next();
    }
    return;
#endif

    //  gLogo::SetBig(true);
    //  gLogo::SetSpinning(false);

    sr_textOut = to;

    tOutput StartHelpTextInternet("$network_master_host_inet_help");
    sg_StartHelpText = &StartHelpTextInternet;
    sg_TalkToMaster = true;

    BrowseServers();

    nServerInfo::Save();

    sg_TalkToMaster = false;

    nServerInfo::SetCreator(cback);

    sg_currentMaster = master;
}

void gServerBrowser::BrowseLAN()
{
    // TODO: reacivate and see what happens. Done.
    sg_RequestLANcontinuously = true;
    //	sg_RequestLANcontinuously = false;

    sn_ServerInfoCreator *cback = nServerInfo::SetCreator(&CreateGServer);

    sr_con.autoDisplayAtNewline=true;
    sr_con.fullscreen=true;

#ifndef DEDICATED
    rSysDep::SwapGL();
    rSysDep::ClearGL();
    rSysDep::SwapGL();
    rSysDep::ClearGL();
#endif

    bool to=sr_textOut;
    sr_textOut=true;

    nServerInfo::DeleteAll();
    nServerInfo::GetFromLAN(lowPort, highPort);

    sr_textOut = to;

    tOutput StartHelpTextLAN("$network_master_host_lan_help");
    sg_StartHelpText = &StartHelpTextLAN;
    sg_TalkToMaster = false;

    BrowseServers();

    nServerInfo::SetCreator(cback);
}

void gServerBrowser::BrowseServers()
{
    if( nServerInfoAdmin::GetAdmin() )
    {
        nServerInfoAdmin::GetAdmin()->BeforeNewScan();
    }

    //nServerInfo::CalcScoreAll();
    //nServerInfo::Sort();
    nServerInfo::StartQueryAll( sg_queryType );
    continuePoll = true;

    gServerMenu browser("Server Browser");
    
    gServerStartMenuItem start(&browser);
    gServerFilterMenuItem filter(&browser);

    /*
      while (nServerInfo::DoQueryAll(sg_simultaneous));
      sn_SetNetState(nSTANDALONE);
      nServerInfo::Sort();

      if (nServerInfo::GetFirstServer())
      ConnectToServer(nServerInfo::GetFirstServer());
    */
    browser.Update();

    // Open with the cursor on the filter field, so typing a search works right
    // away. Done after Update(), which reverses the rows and would otherwise
    // move the field out from under the cursor.
    browser.SelectFilter();

    // eat excess input the user made while the list was fetched
    SDL_Event ignore;
    REAL time;
    while(su_GetSDLInput(ignore, time)) ;
    
    browser.Enter();

    nServerInfo::GetFromLANContinuouslyStop();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(true);
}

void gServerMenu::HandleEvent( SDL_Event event )
{
#ifndef DEDICATED
    // When the cursor is on the filter row, let it handle the keys itself: the
    // sort shortcuts below (left/right, home/end, M) would otherwise steal the
    // letters being typed into the search box.
    if ( selected >= 0 && selected < items.Len()
         && dynamic_cast<gServerFilterMenuItem*>( items(selected) ) )
    {
        return uMenu::HandleEvent( event );
    }
    
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case(SDLK_LEFT):
                        sg_sortKey = static_cast<nServerInfo::PrimaryKey>
                            ( ( sg_sortKey + nServerInfo::KEY_MAX-1 ) % nServerInfo::KEY_MAX );
            Update();
            return;
            break;
        case(SDLK_RIGHT):
                        sg_sortKey = static_cast<nServerInfo::PrimaryKey>
                            ( ( sg_sortKey + 1 ) % nServerInfo::KEY_MAX );
            Update();
            return;
            break;
        case(SDLK_m):
                        FriendsToggle();
            Update();
            return;
            break;
        case(SDLK_HOME):
            SetSelected(NumItems() - 1);
            Update();
            return;
        case(SDLK_END):
            SetSelected(0);
            Update();
            return;
        case(SDLK_f):
            // jump straight to the search box from anywhere in the list
            SelectFilter();
            return;
        default:
            break;
        }
    }
#endif

    uMenu::HandleEvent( event );
}

void gServerMenu::OnRender()
{
    uMenu::OnRender();

    // next time the server list is to be resorted
    static double sg_serverMenuRefreshTimeout=-1E+32f;

    if (sg_serverMenuRefreshTimeout < tSysTimeFloat())
    {
        Update();
        sg_serverMenuRefreshTimeout = tSysTimeFloat()+2.0f;
    }
}

// priority of bookmarks in sorting
static nServerInfo::SortHelperPriority sg_bookmarkPriority[nServerInfo::KEY_MAX]=
{
    nServerInfo::PRIORITY_NONE,
    nServerInfo::PRIORITY_PRIMARY,
    nServerInfo::PRIORITY_SECONDARY,
    nServerInfo::PRIORITY_PRIMARY
};

tCONFIG_ENUM( nServerInfo::SortHelperPriority );
static tSettingItem< nServerInfo::SortHelperPriority > sgc_bookmarkPriorityName( "BROWSER_BOOKMARK_PRIORITY_NAME", sg_bookmarkPriority[nServerInfo::KEY_NAME] );
static tSettingItem< nServerInfo::SortHelperPriority > sgc_bookmarkPriorityPing( "BROWSER_BOOKMARK_PRIORITY_PING", sg_bookmarkPriority[nServerInfo::KEY_PING] );
static tSettingItem< nServerInfo::SortHelperPriority > sgc_bookmarkPriorityUsers( "BROWSER_BOOKMARK_PRIORITY_USERS", sg_bookmarkPriority[nServerInfo::KEY_USERS] );
static tSettingItem< nServerInfo::SortHelperPriority > sgc_bookmarkPriorityScore( "BROWSER_BOOKMARK_PRIORITY_SCORE", sg_bookmarkPriority[nServerInfo::KEY_SCORE] );


void gServerMenu::Update()
{
    // get currently selected server
    gServerMenuItem *item = NULL;
    if ( selected < items.Len() )
    {
        item = dynamic_cast<gServerMenuItem*>(items(selected));
    }
    gServerInfo* info = NULL;
    if ( item )
    {
        info = item->GetServer();
    }

    // keep the cursor position relative to the top, if possible
    int selectedFromTop = items.Len() - selected;

    ReverseItems();

    nServerInfo::CalcScoreAll();
    nServerInfo::Sort( nServerInfo::PrimaryKey( sg_sortKey ), &gServerFavorites::IsFavorite, sg_bookmarkPriority[sg_sortKey] );

    int mi = 2;
    gServerInfo *run = gServerInfo::GetFirstServer();
    bool oneFound = false; //so we can display all if none were found
    
    tString filteredFriends[MAX_FRIENDS];
    tString* friends = getFriends();
    int i;
    for (i = MAX_FRIENDS-1; i>=0; i--)
    {
        filteredFriends[i] = sg_enableFriendsCasing ? friends[i] : friends[i].ToLower();
    }

    while (run)
    {
        if(filter_string.Len() > 1)
        {
            run->show = false;
            oneFound = true;
            
            tString name;
            name << tColoredString::RemoveColors( run->GetName(), false );
            
            if(name.ToLower().find(filter_string.ToLower()) != std::string::npos)
            {
                run->show = true;
            }
        }
        //check friend filter
        else if (getFriendsEnabled())
        {
            run->show = false;
            tString userNames = sg_enableFriendsCasing ? run->UserNames() : run->UserNames().ToLower();
            tString globalIds = sg_enableFriendsCasing ? run->UserGlobalIDs() : run->UserGlobalIDs().ToLower();
            for (i = MAX_FRIENDS-1; i>=0; i--)
            {
                if (run->Users() > 0 && friends[i].Len() > 1 && (userNames.StrPos(filteredFriends[i]) >= 0 || globalIds.StrPos(filteredFriends[i]) >= 0))
                {
                    oneFound = true;
                    run->show = true;
                }
            }
        }
        run = run->Next();
    }

    run = gServerInfo::GetFirstServer();
    {
        while (run)
        {
            if (run->show || oneFound == false)
            {
                if (mi >= items.Len())
                    tNEW(gServerMenuItem)(this);

                gServerMenuItem *item = dynamic_cast<gServerMenuItem*>(items(mi));
                item->SetServer(run);
                mi++;
            }
            run = run->Next();
        }
    }

    if (items.Len() == 1)
        selected = 1;

    while(mi < items.Len() && items.Len() > 2)
    {
        uMenuItem *it = items(items.Len()-1);
        delete it;
    }

    ReverseItems();

    // host game on top, filter below it, whatever the reversal above did
    PinFixedItems();

    // keep the cursor position relative to the top, if possible ( calling function will handle the clamping )
    selected = items.Len() - selectedFromTop;

    // set cursor to currently selected server, if possible
    if ( info && info->menuItem )
    {
        selected = info->menuItem->GetID();
    }

    if (sg_RequestLANcontinuously)
    {
        static REAL timeout=-1E+32f;

        if (timeout < tSysTimeFloat())
        {
            nServerInfo::GetFromLANContinuously();
            if (!continuePoll)
            {
                nServerInfo::StartQueryAll( sg_queryType );
                continuePoll = true;
            }
            timeout = tSysTimeFloat()+10;
        }
    }
}

void gServerMenu::SelectFilter()
{
    for ( int i = items.Len() - 1; i >= 0; --i )
    {
        if ( dynamic_cast<gServerFilterMenuItem*>( items(i) ) )
        {
            selected = i;
            return;
        }
    }
}

void gServerMenu::PinFixedItems()
{
    // find the two fixed rows by type
    gServerStartMenuItem  *startItem  = NULL;
    gServerFilterMenuItem *filterItem = NULL;
    for ( int i = 0; i < items.Len(); ++i )
    {
        if ( !startItem )
            startItem = dynamic_cast<gServerStartMenuItem*>( items(i) );
        if ( !filterItem )
            filterItem = dynamic_cast<gServerFilterMenuItem*>( items(i) );
    }

    if ( !startItem || !filterItem || items.Len() < 2 )
        return;

    // They are already the top two rows and in the right order: nothing to do.
    if ( items( items.Len() - 1 ) == startItem && items( items.Len() - 2 ) == filterItem )
        return;

    // otherwise pull both out and re-append them, filter first so the start row
    // ends up as the very top entry. RemoveItem/AddItem keep idnum in step, which
    // poking at the array directly would not.
    RemoveItem( startItem );
    RemoveItem( filterItem );
    AddItem( filterItem );
    AddItem( startItem );
}

gServerMenu::gServerMenu(const char *title)
        : uMenu(title, false)
{
    nServerInfo *run = nServerInfo::GetFirstServer();
    
    while (run)
    {
        gServerMenuItem *item = tNEW(gServerMenuItem)(this);
        item->SetServer(run);
        run = run->Next();
    }

    ReverseItems();

    // With no servers yet, keep a placeholder row so the menu is not empty.
    if (items.Len() <= 0)
        tNEW(gServerMenuItem)(this);
}

gServerMenu::~gServerMenu()
{
    for (int i=items.Len()-1; i>=0; i--)
        delete items(i);
}

#ifndef DEDICATED
static REAL text_height=.05;

static REAL shrink = .6f;
static REAL displace = .15;

void gServerMenu::Render(REAL y,
                         const tString &servername, const tString &score,
                         const tString &users     , const tString &ping)
{
    if (sr_glOut)
    {
        DisplayText(-.9f, y, text_height, servername.c_str(), sr_fontServerBrowser, -1);
        DisplayText(.6f, y, text_height, ping.c_str(), sr_fontServerBrowser, 1);
        DisplayText(.75f, y, text_height, users.c_str(), sr_fontServerBrowser, 1);
        DisplayText(.9f, y, text_height, score.c_str(), sr_fontServerBrowser, 1);
    }
}

void gServerMenu::Render(REAL x,REAL y, tString &c, int center = 0, int cursor = 0, int cursorPos = 0)
{
    if (sr_glOut)
    {
        DisplayText(x, y, text_height, c.c_str(), sr_fontServerBrowser, -1, cursor, cursorPos);
    }
}

void gServerMenu::Render(REAL y,
                         const tString &servername, const tOutput &score,
                         const tOutput &users     , const tOutput &ping)
{
    tColoredString highlight, normal;
    highlight << tColoredString::ColorString( 1,.7,.7 );
    normal << tColoredString::ColorString( .7,.3,.3 );

    tString sn, s, u, p;

    sn << normal;
    s << normal;
    u << normal;
    p << normal;

    switch ( sg_sortKey )
    {
    case nServerInfo::KEY_NAME:
        sn = highlight;
        break;
    case nServerInfo::KEY_PING:
        p = highlight;
        break;
    case nServerInfo::KEY_USERS:
        u = highlight;
        break;
    case nServerInfo::KEY_SCORE:
        s = highlight;
        break;
    case nServerInfo::KEY_MAX:
        break;
    }

    sn << servername;// tColoredString::RemoveColors( servername );
    s  << score;
    u  << users;
    p  << ping;

    Render(y, sn, s, u, p);
}

#endif /* DEDICATED */
static bool sg_filterServernameColorStrings = true;
static tSettingItem< bool > removeServerNameColors("FILTER_COLOR_SERVER_NAMES", sg_filterServernameColorStrings);
static bool sg_filterServernameDarkColorStrings = true;
static tSettingItem< bool > removeServerNameDarkColors("FILTER_DARK_COLOR_SERVER_NAMES", sg_filterServernameDarkColorStrings);

void gServerMenuItem::Render(REAL x,REAL y,REAL alpha, bool selected)
{
#ifndef DEDICATED
    // REAL time=tSysTimeFloat()*10;

    SetColor( selected, alpha );

    gServerMenu *serverMenu = static_cast<gServerMenu*>(menu);

    if (server)
    {
        tColoredString name;
        tString score;
        tString users;
        tString ping;

        int p = static_cast<int>(server->Ping()*1000);
        if (p < 0)
            p = 0;
        if (p > 10000)
            p = 10000;

        int s = static_cast<int>(server->Score());
        if (server->Score() > 10000)
            s = 10000;
        if (server->Score() < -10000)
            s = -10000;

        if ( favorite_ )
        {
            score << "B ";
        }
        if (server->Polling())
        {
            score << tOutput("$network_master_polling");
        }
        else if (!server->Reachable())
        {
            score << tOutput("$network_master_unreachable");
        }
        else if ( nServerInfo::Compat_Ok != server->Compatibility() )
        {
            switch( server->Compatibility() )
            {
            case nServerInfo::Compat_Upgrade:
                score << tOutput( "$network_master_upgrage" );
                break;
            case nServerInfo::Compat_Downgrade:
                score << tOutput( "$network_master_downgrage" );
                break;
            default:
                score << tOutput( "$network_master_incompatible" );
                break;
            }
        }
        else if ( !favorite_ && server->GetClassification().noJoin_.Len() > 1 )
        {
            score << server->GetClassification().noJoin_;
        }
        else if ( server->Users() >= server->MaxUsers() )
        {
            score << tOutput( "$network_master_full" );
            score << " (" << server->Users() << "/" << server->MaxUsers() << ")";
        }
        else
        {
            score << s;
            users << server->Users() << "/" << server->MaxUsers();
            ping  << p;
        }

        if ( sg_filterServernameColorStrings )
            name << tColoredString::RemoveColors( server->GetName(), false );
	else if ( sg_filterServernameDarkColorStrings )
            name << tColoredString::RemoveColors( server->GetName(), true );
        else
        {
            name << server->GetName();
        }

        serverMenu->Render(y*shrink + displace,
                           name,
                           score, users, ping);
    }
    else
    {
        tOutput o("$network_master_noserver");
        tString s;
        s << o;
        serverMenu->Render(y*shrink + displace,
                           s,
                           tString(""), tString(""), tString(""));

    }
#endif
}

static REAL sg_menuBottom    = -.9;
static REAL sg_requestBottom = -.9;

void gServerMenuItem::RenderBackground()
{
#ifndef DEDICATED
    REAL helpTopReal = sg_requestBottom*shrink + displace - .05;;

    gBrowserMenuItem::RenderBackground();

    rTextField::SetDefaultColor( tColor(1,1,1) );

    rTextField players( -.9, helpTopReal, text_height, sr_fontServerDetails );
    players.EnableLineWrap();
    if ( server )
    {
        players << tOutput( "$network_master_players" );
        if ( server->UserNamesOneLine().Len() > 2 )
            players << server->UserNamesOneLine();
        else
            players << tOutput( "$network_master_players_empty" );
        players << "\n" << tColoredString::ColorString(1,1,1);
        tColoredString uri;
        uri << server->Url() << tColoredString::ColorString(1,1,1);
        tColoredString options;
        options << server->GetClassification().description_;
        options << server->Options();
        players << tOutput( "$network_master_serverinfo", server->Release(), uri, options );
    }

    {
        players << "\n";
        players.SetColor(tColor(1,1,1,displayHelp_ ? helpAlpha_ : 0));
        players << Help();
    }

    REAL helpSpace = players.GetTop() - players.GetBottom();
    REAL helpTop = -.85 + helpSpace;
    REAL helpTopScaled = ( helpTop - displace )/shrink;
    REAL helpTopMax = .25;
    REAL helpTopMin = -.9;
    if( helpTopScaled > helpTopMax )
    {
        helpTopScaled = helpTopMax;
    }
    if( helpTopScaled < helpTopMin )
    {
        helpTopScaled = helpTopMin;
    }
    sg_requestBottom = helpTopScaled;
#endif
}

#ifndef DEDICATED
static void Refresh()
{
    continuePoll = true;
    nServerInfo::StartQueryAll( sg_queryType );
}
#endif

bool gBrowserMenuItem::Event( SDL_Event& event )
{
#ifndef DEDICATED
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case SDLK_r:
            {
                static double lastRefresh = - 100; //!< the time of the last manual refresh
                if ( tSysTimeFloat() - lastRefresh > 2.0 )
                {
                    lastRefresh = tSysTimeFloat();
                    // trigger refresh
                    st_ToDo( Refresh );
                    return true;
                }
            }
            break;
        default:
            break;
        }
    }
#endif

    return uMenuItem::Event( event );
}

bool gServerMenuItem::Event( SDL_Event& event )
{
#ifndef DEDICATED
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case SDLK_p:
            continuePoll = true;
            if ( server && tSysTimeFloat() - lastPing_ > .5f )
            {
                lastPing_ = tSysTimeFloat();

                server->SetQueryType( nServerInfo::QUERY_ALL );
                server->QueryServer();
                server->ClearInfoFlags();
            }
            return true;
            break;
        default:
            break;
        }
        switch (event.key.keysym.sym)
        {
        case SDLK_KP_PLUS:
        case SDLK_PLUS:
            if ( server )
            {
                server->SetScoreBias( server->GetScoreBias() + 10 );
                server->CalcScore();
            }
            (static_cast<gServerMenu*>(menu))->Update();

            return true;
            break;
        case SDLK_KP_MINUS:
        case SDLK_MINUS:
            if ( server )
            {
                server->SetScoreBias( server->GetScoreBias() - 10 );
                server->CalcScore();
            }
            (static_cast<gServerMenu*>(menu))->Update();

            return true;
            break;
        case SDLK_b:
            if ( server )
            {
                if (favorite_ ) {
                    gServerFavorites::RemoveFavorite( server );
                    favorite_ = false;
                } else {
                    favorite_ = gServerFavorites::AddFavorite( server );
                }
            }
            (static_cast<gServerMenu*>(menu))->Update();

            return true;
            break;    
        default:
            break;
        }
    }
#endif

    return gBrowserMenuItem::Event( event );
}

void gServerMenu::RenderListBackground()
{
    {
        double now = tSysTimeFloat();
        static double lastTime = now;
        if( sg_menuBottom > sg_requestBottom )
        {
            sg_menuBottom -= now - lastTime;
        }
        lastTime = now;
        if( sg_menuBottom < sg_requestBottom )
        {
            sg_menuBottom = sg_requestBottom;
        }

        SetBot( sg_menuBottom );
        sg_requestBottom = -.9;
    }

    sn_Receive();
    sn_SendPlanned();

    GenericBackground();
    if (continuePoll)
    {
        // keep filling the list in while any browser row has the cursor: the
        // query loop used to live only in the server rows, so it stalled whenever
        // the cursor sat on the filter.
        continuePoll = nServerInfo::DoQueryAll(sg_simultaneous);
        sn_Receive();
        sn_SendPlanned();
    }

#ifndef DEDICATED
    rTextField::SetDefaultColor( tColor(.8,.3,.3,1) );

    tString sn2 = tString(tOutput("$network_master_servername"));
    if (getFriendsEnabled()) //display that friends filter is on
        sn2 << " - " << tOutput("$friends_enable");

    Render(.62,
           sn2,
           tOutput("$network_master_score"),
           tOutput("$network_master_users"),
           tOutput("$network_master_ping"));
#endif
}

void gBrowserMenuItem::RenderBackground()
{
    static_cast<gServerMenu*>(menu)->RenderListBackground();
}

void gServerMenuItem::Enter()
{
    nServerInfo::GetFromLANContinuouslyStop();

    menu->Exit();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(false);

    if (server)
        ConnectToServer(server);
}

void gServerMenuItem::Select()
{
    // reset help display state
    this->displayHelp_ = false;
    this->helpAlpha_ = 0.0f;
}

void gServerMenuItem::SetServer(nServerInfo *s)
{
    if (s == server)
        return;

    if (server)
        server->menuItem = NULL;

    server = dynamic_cast<gServerInfo*>(s);

    if (server)
    {
        if (server->menuItem)
            server->menuItem->SetServer(NULL);

        server->menuItem = this;
    }

    favorite_ = gServerFavorites::IsFavorite( server );
}

gServerInfo *gServerMenuItem::GetServer()
{
    return server;
}

static char const * sg_HelpText = "$network_master_browserhelp";

gServerMenuItem::gServerMenuItem(gServerMenu *men)
        :gBrowserMenuItem(men, sg_HelpText), server(NULL), lastPing_(-100), favorite_(false)
{}

gServerMenuItem::~gServerMenuItem()
{
    SetServer(NULL);

    // make sure the last entry in the array (the first menuitem)
    // stays the same
    uMenuItem* last = menu->Item(menu->NumItems()-1);
    menu->RemoveItem(last);
    menu->RemoveItem(this);
    menu->AddItem(last);
}


gServerInfo::~gServerInfo()
{
    if (menuItem)
        delete menuItem;
}


void gServerStartMenuItem::Render(REAL x,REAL y,REAL alpha, bool selected)
{
#ifndef DEDICATED
    // REAL time=tSysTimeFloat()*10;

    SetColor( selected, alpha );

    tString s;
    s << tOutput("$network_master_start");
    static_cast<gServerMenu*>(menu)->Render(y*shrink + displace,
                                            s,
                                            tString(), tString(), tString());
#endif
}



void gServerStartMenuItem::Enter()
{
    nServerInfo::GetFromLANContinuouslyStop();

    menu->Exit();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(false);

    sg_HostGameMenu();
}


void gServerFilterMenuItem::RenderBackground()
{
    static_cast<gServerMenu*>(menu)->RenderListBackground();
}

void gServerFilterMenuItem::Render(REAL x,REAL y,REAL alpha, bool selected)
{
#ifndef DEDICATED
    // The label carries its own gradient (white to blue), so use it as-is rather
    // than overriding it with a menu colour. Only the invisible part matters here.
    tColoredString label( description );

    // the colour codes must not count towards where the typed text starts
    tString visible( tColoredString::RemoveColors( description, false ) );

    x = -.9f;
    REAL x2 = visible.Len() * 0.018 + x;

    int cMode = selected ? 1 : 0;

    static_cast<gServerMenu*>(menu)->Render(x, y*shrink + displace, label);
    static_cast<gServerMenu*>(menu)->Render(x2, y*shrink + displace, *content, 1, cMode, realCursorPos);
#endif
}

bool gServerFilterMenuItem::Event( SDL_Event& event )
{
#ifndef DEDICATED
    auto prev_filter_string = *content; // store current content for later comparison
    bool update = false; // do we need to update the server list?
    bool ret = false; // have we handled the event?

    if (event.type==SDL_KEYDOWN)
    {
        switch (event.key.keysym.sym)
        {
        case(SDLK_ESCAPE):
            // escape clears the filter
            if(!content->empty())
            {
                *content = "";

                update = true;
                ret = true;

                break;
            }

            // fall through
        default:
            // let base handle it
            ret = uMenuItemString::Event( event );

            break;
        }
    }
    else
    {
        // let base handle it
        ret = uMenuItemString::Event( event );
    }

    // update on change
    if(prev_filter_string != *content)
    {
        update = true;
    }

    if(update)
    {
        // update menu, filter has changed
        (static_cast<gServerMenu*>(menu))->Update();
    }

    return ret;
#else
    return false;
#endif
}

gServerStartMenuItem::gServerStartMenuItem(gServerMenu *men)
        :gBrowserMenuItem(men, *sg_StartHelpText)
{}

gServerStartMenuItem::~gServerStartMenuItem()
{
}
