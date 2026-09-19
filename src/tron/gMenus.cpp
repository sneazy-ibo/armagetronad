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

#include "gMenus.h"
#include "ePlayer.h"
#include "rScreen.h"
#include "rTexture.h"
#include "nConfig.h"
#include "rConsole.h"
#include "tToDo.h"
#include "rGL.h"
#include "eTimer.h"
#include "eFloor.h"
#include "rRender.h"
#include "rModel.h"
#include "gGame.h"
#include "gCycle.h"
#include "cockpit/cCockpit.h"
#include "cockpit/cMap.h"
#include "rFont.h"
#include "tDirectories.h"
#include "tRecorder.h"
#include "rSysdep.h"
#include "uInput.h"

#include <sstream>
#include <set>
#include <ctime>
#include <algorithm>
#include <vector>
#include <array>
#include <memory>

#ifndef DEDICATED
static tConfItem<int>   tm0("TEXTURE_MODE_0",rTextureGroups::TextureMode[0]);
static tConfItem<int>   tm1("TEXTURE_MODE_1",rTextureGroups::TextureMode[1]);
static tConfItem<int>   tm2("TEXTURE_MODE_2",rTextureGroups::TextureMode[2]);
static tConfItem<int>   tm3("TEXTURE_MODE_3",rTextureGroups::TextureMode[3]);

uMenu sg_screenMenu("$display_settings_menu");

static uMenuItemFunction defaul
(&sg_screenMenu,"$graphics_load_defaults_text",
 "$graphics_load_defaults_help",
 &sr_LoadDefaultConfig);
uMenu screen_menu_detail("$detail_settings_menu");
uMenu screen_menu_tweaks("$performance_tweaks_menu");
uMenu screen_menu_prefs("$preferences_menu");

static void sg_ScreenModeMenu();

static uMenuItemSubmenu smt(&sg_screenMenu,&screen_menu_tweaks,
                            "$performance_tweaks_menu_help");
static uMenuItemSubmenu smd(&sg_screenMenu,&screen_menu_detail,
                            "$detail_settings_menu_help");
static uMenuItemSubmenu smp(&sg_screenMenu,&screen_menu_prefs,
                            "$preferences_menu_help");
static uMenuItemFunction smm(&sg_screenMenu,"$screen_mode_menu",
                             "$screen_mode_menu_help", sg_ScreenModeMenu );

// Modded Settings: HUD and look options added by this build.

uMenu sg_moddedMenu("Modded Settings");

//! The cockpit in use: what the menu was opened with, or what was last confirmed
//! with enter. Hovering only previews, so this is both what the list marks and what
//! a cancelled menu returns to.
static tString sg_cockpitCommitted;

static void sg_ModdedSettingsMenu()
{
    // Same as pressing escape: tell the chat system, and pause a local game.
    se_ChatState( ePlayerNetID::ChatFlags_Menu, true );
    if ( sn_GetNetState() == nSTANDALONE )
        se_PauseGameTimer( true, eTIMER_PAUSE_MENU );

    // ... but keep the HUD drawn while we are in here. The in-game menu lets it
    // disappear, this one should not.
    sg_hudVisibleInMenu = true;

    // what was in use when we came in: hovering only previews, so a menu left
    // without pressing enter goes back to this
    sg_cockpitCommitted = cCockpit::GetFile();

    sg_moddedMenu.Enter();          // returns once the menu is left

    sg_hudVisibleInMenu = false;

    if ( sg_cockpitCommitted.Len() > 0 && cCockpit::GetFile() != sg_cockpitCommitted )
        cCockpit::SetFile( sg_cockpitCommitted );
    sg_cockpitCommitted = tString();

    if ( sn_GetNetState() == nSTANDALONE )
        se_PauseGameTimer( false, eTIMER_PAUSE_MENU );
    se_ChatState( ePlayerNetID::ChatFlags_Menu, false );
}

static uActionGlobal moddedSettingsAction( "MODDED_SETTINGS" );
static bool sg_moddedSettingsKey( REAL x )
{
    if ( x > 0 )
    {
        if ( uMenu::MenuActive() )
            sg_moddedMenu.Exit();          // back out and play on
        else
            st_ToDo( &sg_ModdedSettingsMenu );
    }

    return true;
}
static uActionGlobalFunc moddedSettingsActionFunc( &moddedSettingsAction, &sg_moddedSettingsKey, true );

//! Binding for the action above, in the menu it opens.
static uMenuItemInput moddedSettingsBinding( &sg_moddedMenu, &moddedSettingsAction, 0, "Open this menu" );

//! Binding for the in-game map toggle; H until the player changes it.
static uMenuItemInput moddedHudMapBinding( &sg_moddedMenu, &cCockpit::GetHudMapAction(), 0 );

//! re-read the current cockpit file, so layout edits apply without a restart
static void sg_reloadCockpit()
{
    cCockpit::SetFile( cCockpit::GetFile() );
    con << "Cockpit reloaded: " << cCockpit::GetFile() << "\n";
}

//! Horizontal stretch of all text; 1 is the engine default.
static uMenuItemReal moddedTextWidth( &sg_moddedMenu,
                                      "Text width",
                                      "Horizontal stretch of all text. 1 = normal",
                                      sr_fontStretch, 0.5, 2.5, 0.05 );

//! changing the font type reloads the fonts at once, so the effect is visible
static void sg_fontChosen( int const & )
{
    sr_ReloadFont();
}

//! font rendering type; values match the FONT_TYPE setting
class gFontMenuItem : public uMenuItemSelection<int>
{
public:
    gFontMenuItem( uMenu * menu )
        : uMenuItemSelection<int>( menu, "Font", "How text is rendered", sr_fontType, &sg_fontChosen )
    {
        NewChoice( tOutput( "Pixmap" ),   tOutput( "Fixed size, like the original 0.2.9 look" ), sr_fontPixmap );
        NewChoice( tOutput( "Bitmap" ),   tOutput( "Bitmap font" ),                              sr_fontBitmap );
        NewChoice( tOutput( "Texture" ),  tOutput( "Scaled texture font, the default" ),         sr_fontTexture );
        NewChoice( tOutput( "Polygon" ),  tOutput( "Polygon font" ),                             sr_fontPolygon );
        NewChoice( tOutput( "Outline" ),  tOutput( "Outline font" ),                             sr_fontOutline );
        NewChoice( tOutput( "Extruded" ), tOutput( "Extruded font, experimental" ),              sr_fontExtruded );
    }
};

static gFontMenuItem moddedFont( &sg_moddedMenu );

// Cockpits. The catalogue is the wiki's list, kept here in the source so it is
// always there, and it supplies the prose that a cockpit XML cannot carry.

struct gCockpitVersion
{
    char const * label;   // "0.0.7"
    char const * path;
    char const * url;     // empty when the default repository serves it
    char const * note;    // what is special about this version
};

struct gCockpitCatalogueEntry
{
    char const * title;
    char const * author;
    char const * description;
    gCockpitVersion const * versions;
    int versionCount;
    bool forked;          //!< this build's own version of someone else's cockpit
};

static gCockpitVersion const sg_versions_classic[] = {
    { "0.0.1", "wrtlprnft/classic-0.0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_classicMinimap[] = {
    { "0.0.2", "nelg/armaclassic-0.0.2.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_classicPlus[] = {
    { "0.0.1", "rxfreaks/cockpits/classicplus-0.0.1.aacockpit.xml",
      "http://rxtron.com/aa/resource/rxfreaks/cockpits/classicplus-0.0.1.aacockpit.xml",
      "its host is frequently unreachable" },
};
static gCockpitVersion const sg_versions_essential[] = {
    { "0.0.5", "Rain/essential-0.0.5.aacockpit.xml", "", "the current one" },
    { "0.0.1", "Rain/essential-0.0.1.aacockpit.xml", "", "the first release" },
};
static gCockpitVersion const sg_versions_cadillac[] = {
    { "0.1", "Cadillac/cockpits/full-0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_nivek[] = {
    { "1.1.0", "NIVEK/Cockpits/Niveks_vanplus-1.1.0.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_playroom[] = {
    { "0.0.3", "Lucifer/sick/Playroom-0.0.3.aacockpit.xml", "",
      "shipped with this build; the repository copy is empty" },
    { "0.0.1", "Lucifer/sick/Playroom-0.0.1.aacockpit.xml", "", "the version on the wiki" },
};
static gCockpitVersion const sg_versions_skypit[] = {
    { "0.0.1", "lukesky/skypit-0.0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_standard[] = {
    { "0.0.1", "Anonymous/standard-0.0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_vanoriginal[] = {
    { "0.0.1", "Vanhayes/cockpit/vanoriginal-0.0.1.aacockpit.xml", "",
      "its host is frequently unreachable" },
};
static gCockpitVersion const sg_versions_vanoriginalFix[] = {
    { "0.0.2", "vov/cockpit/vanoriginal_mapfix-0.0.2.aacockpit.xml", "", "map is transparent again" },
};
static gCockpitVersion const sg_versions_vov[] = {
    { "0.6", "vov/cockpit/new_notextures-0.6.aacockpit.xml", "", "no textures" },
};
static gCockpitVersion const sg_versions_wordsview[] = {
    { "0.0.1", "Word/cockpit/WordsView-0.0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_wrtl[] = {
    { "0.0.3", "wrtlprnft/testfile-0.0.3.aacockpit.xml", "", "needs 0.3.1 or later" },
};
static gCockpitVersion const sg_versions_xevi[] = {
    { "0.0.7", "xevi/xevi-0.0.7.aacockpit.xml", "", "the version the repository serves" },
};
static gCockpitVersion const sg_versions_abyss[] = {
    { "0.0.1", "spidey/abyss-0.0.1.aacockpit.xml", "", "" },
};
static gCockpitVersion const sg_versions_pink[] = {
    { "0.0.1", "spidey/pink-0.0.1.aacockpit.xml", "", "" },
};

//! our fork of Pink, kept under its own name so the original is untouched
static gCockpitVersion const sg_versions_pinkplus[] = {
    { "0.0.1", "spidey/pinkplus-0.0.1.aacockpit.xml", "", "ping fixed, brake gauge added" },
};

static gCockpitCatalogueEntry const sg_cockpitCatalogue[] =
{
    { "Classic", "wrtlprnft",
      "The 0.2.9 HUD recreated: needle gauges for speed, rubber and brakes, scores, fastest player, enemies and friends, ping and the frame rate, in the original colours and positions. This build's default.",
      sg_versions_classic, 1, true },
    { "Classic Minimap", "nelg", "The classic cockpit with a minimap added.",
      sg_versions_classicMinimap, 1 },
    { "Classic Plus", "dukevin",
      "Classic plus health gauges above cycles, a small transparent minimap in the corner, a speed-versus-fastest meter, and a brake meter that changes colour with the brake left.",
      sg_versions_classicPlus, 1 },
    { "Essential", "rain",
      "Simple and lightweight. Cockpit key 1 hides the brake and speed labels, key 2 shows the debug tools.",
      sg_versions_essential, 2 },
    { "Cadillac's Cockpit", "Cadillac",
      "A clean fortress layout meant to be read in peripheral vision: impact meters, enemy rubber and brakes, team balance and a minimap. Cockpit keys 1 to 5 toggle the extras.",
      sg_versions_cadillac, 1 },
    { "NIVEK's vanplus", "NIVEK",
      "Vanoriginal and Classic Plus mixed: a three mode map, a front proximity gauge, enemy rubber and a last-alive bar. Cockpit keys 1 to 5 cycle the map views and toggle the meters.",
      sg_versions_nivek, 1 },
    { "Playroom", "Lucifer",
      "A fortress cockpit with every gauge close to where your eyes already are: speed, brake and rubber meters, a colour coded enemies gauge and a map.",
      sg_versions_playroom, 2 },
    { "Skypit", "LukeSky", "A compact HUD. Cockpit key 1 toggles the map, key 2 the time and FPS display.",
      sg_versions_skypit, 1 },
    { "Standard", "wrtlprnft and Lucifer",
      "The stock 0.3 and 0.4 HUD: HUD map and bar gauges, showing about the same data as 0.2.8.",
      sg_versions_standard, 1 },
    { "Vanoriginal", "Vanhayes",
      "All gauges are vertical bars, the top right only shows time and FPS, and the enemies and friends meter from incam is included. Cockpit keys 1, 2 and 5 toggle the map, the impact meters and the top right labels.",
      sg_versions_vanoriginal, 1 },
    { "Vanoriginal (map fix)", "vov", "vov's fixed version of Vanoriginal.",
      sg_versions_vanoriginalFix, 1 },
    { "vov's Cockpit", "Ninja Potato",
      "A well documented cockpit, without textures. Cockpit key 5 toggles the side bars.",
      sg_versions_vov, 1 },
    { "WordsView", "Word",
      "Vertical gauges, a minimap, and rubber meters drawn above enemies and teammates.",
      sg_versions_wordsview, 1 },
    { "Wrtl's Cockpit", "wrtlprnft",
      "A big rubber gauge on the left, player information top right, and a gauge showing the approximate time to the next impact. Cockpit key 1 hides the big map, key 2 dims the 3D rendering.",
      sg_versions_wrtl, 1 },
    { "Xevi's Cockpit", "Xevi",
      "An evolution of the real HUD with warning bars for front, left and right impacts. Cockpit key 3 switches to the full screen map.",
      sg_versions_xevi, 1 },
    { "Abyss", "Spidey", "A dark, minimal cockpit.", sg_versions_abyss, 1 },
    { "Pink", "Spidey", "A pink themed cockpit.", sg_versions_pink, 1 },
    { "Pink Plus", "Spidey", "spidey's Pink with the ping working and a brake bar gauge added. Speed, rubber and brakes as bars, enemies and friends, scores, time, FPS, ping and a map.",
      sg_versions_pinkplus, 1, true },
};

static int const sg_cockpitCatalogueSize = sizeof( sg_cockpitCatalogue ) / sizeof( sg_cockpitCatalogue[0] );

//! the submenu the browser lives in
uMenu sg_cockpitMenu( "Cockpits", false );

//! cockpit files found in the resource directories, as resource paths
static tArray< tString > sg_cockpitFiles;

//! those of them that contain a Map widget. Detected from the file itself, so it
//! also works for a cockpit that is not the one in use, and before a game starts.
static std::vector< tString > sg_cockpitsWithMap;

//! the path a version entry loads
static tString sg_cockpitPath( gCockpitVersion const & version )
{
    tString path( version.path );
    if ( version.url[0] != '\0' )
        path << "(" << version.url << ")";

    return path;
}

//! does this cockpit file hold a map? looks for the Map element in the XML
static bool sg_scanCockpitForMap( tString const & path )
{
    FILE * file = tResourceManager::openResource( path );
    if ( !file )
        return false;

    bool found = false;
    char buffer[4096];
    size_t got = 0;
    tString tail;

    while ( ( got = fread( buffer, 1, sizeof( buffer ), file ) ) > 0 )
    {
        tString chunk;
        chunk.append( buffer, got );
        tail << chunk;

        // "<Map", but not "<MapModes": the element name ends in space, > or /
        tString::size_type pos = tail.find( "<Map" );
        while ( pos != tString::npos )
        {
            char const next = pos + 4 < tail.Len() ? tail[pos + 4] : '\0';
            if ( next == '\0' || next == '>' || next == '/' || next == ' ' || next == '\t' || next == '\n' || next == '\r' )
            {
                found = true;
                break;
            }
            pos = tail.find( "<Map", pos + 4 );
        }

        if ( found )
            break;

        // keep enough of the tail to see an element split across reads
        if ( tail.Len() > 16 )
            tail = tail.substr( tail.Len() - 16 );
    }

    fclose( file );
    return found;
}

//! does this cockpit file hold a map?
static bool sg_cockpitHasMap( char const * path )
{
    for ( int i = 0; i < (int)sg_cockpitsWithMap.size(); ++i )
        if ( sg_cockpitsWithMap[i] == path )
            return true;

    return false;
}

//! does the resource directory hold this cockpit?
static bool sg_cockpitInstalled( char const * path )
{
    for ( int i = 0; i < sg_cockpitFiles.Len(); ++i )
        if ( sg_cockpitFiles( i ) == path )
            return true;

    return false;
}

//! the catalogue in the order it is shown: what is installed first, then the
//! rest, each group sorted by title
static std::vector< int > sg_cockpitOrder;

static int sg_lowerCase( int c )
{
    return ( c >= 'A' && c <= 'Z' ) ? c - 'A' + 'a' : c;
}

//! is any version of this cockpit installed?
static bool sg_cockpitEntryInstalled( gCockpitCatalogueEntry const & entry )
{
    for ( int v = 0; v < entry.versionCount; ++v )
        if ( sg_cockpitInstalled( entry.versions[v].path ) )
            return true;

    return false;
}

static bool sg_cockpitTitleLess( int a, int b )
{
    char const * x = sg_cockpitCatalogue[a].title;
    char const * y = sg_cockpitCatalogue[b].title;

    for ( ; *x != '\0' && *y != '\0'; ++x, ++y )
    {
        int const cx = sg_lowerCase( *x );
        int const cy = sg_lowerCase( *y );
        if ( cx != cy )
            return cx < cy;
    }

    return *x == '\0' && *y != '\0';
}

static void sg_buildCockpitOrder()
{
    std::vector< int > installed, rest;

    for ( int i = 0; i < sg_cockpitCatalogueSize; ++i )
    {
        if ( sg_cockpitEntryInstalled( sg_cockpitCatalogue[i] ) )
            installed.push_back( i );
        else
            rest.push_back( i );
    }

    std::sort( installed.begin(), installed.end(), sg_cockpitTitleLess );
    std::sort( rest.begin(), rest.end(), sg_cockpitTitleLess );

    sg_cockpitOrder = installed;
    sg_cockpitOrder.insert( sg_cockpitOrder.end(), rest.begin(), rest.end() );
}

//! where the list cursor is, and which version of the highlighted cockpit is chosen
static int sg_cockpitSelected = 0;
static int sg_cockpitVersionEntry = -1;
static int sg_cockpitSelectedVersion = 0;

//! the entry under the cursor
static gCockpitCatalogueEntry const & sg_selectedCockpit()
{
    return sg_cockpitCatalogue[ sg_cockpitOrder[ sg_cockpitSelected ] ];
}

//! Preview the cockpit under the cursor, but only one that is already here.
static void sg_previewCockpit()
{
    gCockpitCatalogueEntry const & entry = sg_selectedCockpit();

    // the chosen version if it is installed, else the newest installed one
    int pick = -1;
    if ( sg_cockpitSelectedVersion >= 0 && sg_cockpitSelectedVersion < entry.versionCount
         && sg_cockpitInstalled( entry.versions[sg_cockpitSelectedVersion].path ) )
        pick = sg_cockpitSelectedVersion;
    else
        for ( int v = 0; v < entry.versionCount; ++v )
            if ( sg_cockpitInstalled( entry.versions[v].path ) )
                pick = v;

    if ( pick >= 0 )
        cCockpit::SetFile( sg_cockpitPath( entry.versions[pick] ) );
    else if ( sg_cockpitCommitted.Len() > 0 )
        // nothing to show for this one: don't leave a different cockpit's HUD up
        cCockpit::SetFile( sg_cockpitCommitted );
}

//! make sure the chosen version belongs to the highlighted cockpit, defaulting to
//! a version that is already here
static void sg_syncCockpitVersion()
{
    if ( sg_cockpitVersionEntry == sg_cockpitSelected )
        return;

    sg_cockpitVersionEntry = sg_cockpitSelected;
    sg_cockpitSelectedVersion = 0;

    gCockpitCatalogueEntry const & entry = sg_selectedCockpit();
    for ( int v = 0; v < entry.versionCount; ++v )
        if ( sg_cockpitInstalled( entry.versions[v].path ) )
            sg_cockpitSelectedVersion = v;
}

//! Collect the cockpits in one directory, then look deeper: a resource path is
//! <author>/[<category>/]<name>, so they can sit three directories below a root.
static void sg_scanCockpitDir( tString const & root, tString const & prefix, int depth )
{
    tArray< tString > files;
    tDirectories::GetFiles( root + prefix, tString( "*.aacockpit.xml" ), files, tDirectories::eGetFilesFilesOnly );
    for ( int f = 0; f < files.Len(); ++f )
    {
        tString file = prefix + files( f );

        // the tutorial cockpits belong to the tutorial maps, they are not
        // something a player picks
        if ( file.find( "/tutorial/" ) != tString::npos )
            continue;

        // 'included/' and 'automatic/' name repositories, not part of the resource path.
        if ( file.substr( 0, 9 ) == "included/" )
            file = file.substr( 9 );
        else if ( file.substr( 0, 10 ) == "automatic/" )
            file = file.substr( 10 );

        bool known = false;
        for ( int i = 0; i < sg_cockpitFiles.Len(); ++i )
            if ( sg_cockpitFiles( i ) == file )
                known = true;

        if ( !known )
        {
            sg_cockpitFiles.push_back( file );
            if ( sg_scanCockpitForMap( file ) )
                sg_cockpitsWithMap.push_back( file );
        }
    }

    if ( depth <= 0 )
        return;

    tArray< tString > dirs;
    tDirectories::GetFiles( root + prefix, tString( "*" ), dirs, tDirectories::eGetFilesDirsOnly );
    for ( int d = 0; d < dirs.Len(); ++d )
        sg_scanCockpitDir( root, prefix + dirs( d ), depth - 1 );
}

static void sg_findCockpits()
{
    sg_cockpitFiles.SetLen( 0 );
    sg_cockpitsWithMap.clear();

    tArray< tString > roots;
    tDirectories::Resource().GetPaths( roots );

    for ( int r = 0; r < roots.Len(); ++r )
    {
        tString root = roots( r );
        if ( !root.EndsWith( "/" ) )
            root += "/";

        sg_scanCockpitDir( root, tString( "" ), 3 );
    }
}

//! is this the cockpit in use? the configured value may carry its repository
static bool sg_cockpitIsCurrent( gCockpitVersion const & version )
{
    return sg_cockpitCommitted == sg_cockpitPath( version ) || sg_cockpitCommitted == version.path;
}

//! the HUD parts of the cockpit in use (the map first, so the browser can find it)
static std::vector< cWidget::Base * > sg_hudParts;

static void sg_findHudParts()
{
    sg_hudParts.clear();

    for ( std::list< cCockpit * >::const_iterator i = cCockpit::Cockpits().begin();
          i != cCockpit::Cockpits().end() && sg_hudParts.empty(); ++i )
    {
        std::vector< cWidget::Base * > parts;
        ( *i )->GetToggleWidgets( parts );
        sg_hudParts = parts;
    }
}

static cWidget::Base * sg_hudPart( char const * typeName )
{
    for ( int i = 0; i < (int)sg_hudParts.size(); ++i )
        if ( sg_hudParts[i]->GetTypeName() == typeName )
            return sg_hudParts[i];

    return 0;
}

//! label of a toggleable HUD part: its caption if it has one, else its type
static tString sg_hudPartLabel( cWidget::Base * widget )
{
    if ( cWidget::WithCaption * caption = dynamic_cast< cWidget::WithCaption * >( widget ) )
        if ( caption->GetCaptionText().Len() > 0 )
            return caption->GetCaptionText();

    return widget->GetTypeName();
}

//! The state of a cockpit: in use, here, or still to fetch.
static char const * sg_cockpitStateIcon( bool inUse, bool installed )
{
    if ( inUse )
        return "[*]";
    return installed ? "[x]" : "[ ]";
}

//! One row of the list; also used for measuring it.
static tString sg_cockpitRowLabel( gCockpitCatalogueEntry const & entry, bool cursor, bool * installedOut )
{
    bool inUse = false;
    bool installed = false;
    for ( int v = 0; v < entry.versionCount; ++v )
    {
        if ( sg_cockpitInstalled( entry.versions[v].path ) )
            installed = true;
        if ( sg_cockpitIsCurrent( entry.versions[v] ) )
            inUse = true;
    }
    if ( installedOut )
        *installedOut = installed;

    tString label( cursor ? "> " : "  " );
    label << ( entry.forked ? "+ " : "  " );
    label << sg_cockpitStateIcon( inUse, installed );
    label << " " << entry.title;

    return label;
}

//! The height text can have without growing wider than maxWidth.
static REAL sg_fitHeight( tString const & text, REAL h, REAL maxWidth )
{
    REAL const w = rTextField::GetTextLength( text, h, false );
    return ( w > maxWidth && w > 0 ) ? h * maxWidth / w : h;
}

//! The browser: the cockpits on the left, everything about the highlighted one on
//! the right, including the controls that belong to it.
class uMenuItemCockpitBrowser : public uMenuItem
{
public:
    uMenuItemCockpitBrowser( uMenu * menu )
        : uMenuItem( menu, tOutput() ) {}

    //! reading the list again on entry keeps it correct after a download
    virtual void Select()
    {
        sg_findCockpits();
        sg_buildCockpitOrder();
        sg_cockpitSelected = 0;
        sg_findHudParts();
        sg_syncCockpitVersion();

        if ( sg_cockpitCommitted.Len() == 0 )
            sg_cockpitCommitted = cCockpit::GetFile();
    }

    //! the browser draws its own help, and it does not fit the menu's help box
    virtual bool DisplayHelp( bool, REAL, REAL ) { return false; }

    virtual void RenderBackground()
    {
        menu->GenericBackground();
    }

    virtual void Render( REAL, REAL, REAL alpha, bool selected );

    virtual bool Event( SDL_Event & event );

    virtual void Enter();

private:
};

void uMenuItemCockpitBrowser::Render( REAL, REAL, REAL alpha, bool )
{
#ifndef DEDICATED
    if ( !sr_glOut )
        return;

    REAL const hText = .055f;    // half the menu default (.11): readable, and
    REAL const hTitle = .075f;   // leaves room for two panes of text
    REAL const line = .075f;

    REAL const left = -.92f;
    REAL const right = .06f;
    REAL const top = .56f;    // below the key hint line, which the menu draws at .66

    if ( sg_cockpitOrder.empty() )
        sg_buildCockpitOrder();

    if ( sg_cockpitSelected < 0 )
        sg_cockpitSelected = 0;
    if ( sg_cockpitSelected >= (int)sg_cockpitOrder.size() )
        sg_cockpitSelected = (int)sg_cockpitOrder.size() - 1;

    // The key actually bound to HUD_MAP, or H when nothing is bound.
    tString const boundKey = su_GetBoundKeyName( &cCockpit::GetHudMapAction() );
    tString const mapKey = boundKey.Len() > 0 ? boundKey : tString( "H" );

    // the key hint sits right under the title the menu draws
    SetColor( false, alpha * .7f );
    {
        tString hint( "up/down choose   left/right version   enter use   " );
        hint << mapKey << " map   R reload   (+ our version)";
        ::DisplayText( left, .66f, sg_fitHeight( hint, hText, .94f - left ), hint, sr_fontMenu, -1 );
    }

    // Measure the widest row and shrink the list to fit, instead of guessing at widths.
    REAL const listWidth = ( right - .04f ) - left;
    REAL hRow = hText;
    {
        REAL widest = 0;
        for ( int i = 0; i < (int)sg_cockpitOrder.size(); ++i )
        {
            REAL const w = rTextField::GetTextLength(
                sg_cockpitRowLabel( sg_cockpitCatalogue[ sg_cockpitOrder[i] ], false, NULL ), hRow, false );
            if ( w > widest )
                widest = w;
        }
        if ( widest > listWidth )
            hRow *= listWidth / widest;
    }

    // the list, one line per cockpit
    int const maxRows = 15;
    int first = 0;
    if ( sg_cockpitSelected >= maxRows )
        first = sg_cockpitSelected - maxRows + 1;

    REAL const listTop = .60f;

    // say so when the list continues out of sight
    if ( first > 0 )
    {
        tString more;
        more << first << ( first == 1 ? " more cockpit above  ^" : " more cockpits above  ^" );
        SetColor( false, alpha * .55f );
        ::DisplayText( left, listTop, hText, more, sr_fontMenu, -1 );
    }

    for ( int row = 0; row < maxRows && first + row < (int)sg_cockpitOrder.size(); ++row )
    {
        int const index = sg_cockpitOrder[ first + row ];
        gCockpitCatalogueEntry const & entry = sg_cockpitCatalogue[index];

        bool installed = false;
        tString const label = sg_cockpitRowLabel( entry, ( first + row ) == sg_cockpitSelected, &installed );

        // the highlighted row is the one the cursor is on, so it is the row
        // position that matters here, not the catalogue index
        bool const highlighted = ( first + row ) == sg_cockpitSelected;
        SetColor( highlighted, alpha );
        if ( !installed )
            SetColor( highlighted, alpha * .55f );   // dim what is not here yet

        REAL const y = listTop - line * ( row + 1 );
        ::DisplayText( left, y, hRow, label, sr_fontMenu, -1 );
    }

    if ( first + maxRows < (int)sg_cockpitOrder.size() )
    {
        tString more;
        int const remaining = (int)sg_cockpitOrder.size() - ( first + maxRows );
        more << "v  " << remaining << ( remaining == 1 ? " more cockpit below" : " more cockpits below" );
        SetColor( false, alpha * .55f );
        ::DisplayText( left, listTop - line * ( maxRows + 1 ), hText, more, sr_fontMenu, -1 );
    }

    // the right pane: everything about the highlighted cockpit
    gCockpitCatalogueEntry const & entry = sg_selectedCockpit();

    bool inUse = false;
    int installedVersion = -1;
    for ( int v = 0; v < entry.versionCount; ++v )
    {
        if ( sg_cockpitInstalled( entry.versions[v].path ) )
            installedVersion = v;
        if ( sg_cockpitIsCurrent( entry.versions[v] ) )
            inUse = true;
    }

    // the version the chooser is on
    sg_syncCockpitVersion();
    if ( sg_cockpitSelectedVersion >= entry.versionCount )
        sg_cockpitSelectedVersion = 0;

    gCockpitVersion const & version = entry.versions[sg_cockpitSelectedVersion];

    REAL y = top;
    SetColor( true, alpha );
    ::DisplayText( right, y, hTitle, entry.title, sr_fontMenu, -1 );
    y -= line;

    tString sub;
    sub << sg_cockpitStateIcon( inUse, installedVersion >= 0 ) << "  " << entry.author << "  -  ";
    if ( inUse )
        sub << "in use";
    else if ( installedVersion >= 0 )
        sub << "installed";
    else
        sub << "not installed";
    SetColor( false, alpha * .8f );
    ::DisplayText( right, y, hText, sub, sr_fontMenu, -1 );
    y -= line;

    if ( entry.versionCount > 1 )
    {
        tString versionLine;
        versionLine << "< " << version.label << " >   version " << ( sg_cockpitSelectedVersion + 1 )
                    << " of " << entry.versionCount;
        SetColor( false, alpha );
        ::DisplayText( right, y, hText, versionLine, sr_fontMenu, -1 );
        y -= line * .9f;

        if ( version.note[0] != '\0' )
        {
            SetColor( false, alpha * .7f );
            ::DisplayText( right, y, hText, version.note, sr_fontMenu, -1 );
            y -= line * .9f;
        }
    }
    else
    {
        tString versionLine;
        versionLine << "version " << version.label;
        SetColor( false, alpha * .8f );
        ::DisplayText( right, y, hText, versionLine, sr_fontMenu, -1 );
        y -= line * .9f;
    }

    // The widgets a reload replaces, so ask for them again on every frame and
    // every key press: keeping the pointers around is what made M stop working.
    sg_findHudParts();
    cWidget::Base * map = sg_hudPart( "Map" );

    // The minimap has a line of its own and is left out of the parts list below,
    // otherwise it shows up twice.
    tString mapLine( "minimap: " );
    if ( inUse )
    {
        if ( map )
            mapLine << ( map->Active() ? "shown" : "hidden" ) << "   [" << mapKey << "]";
        else
            mapLine << "none in this cockpit";
    }
    else if ( !sg_cockpitInstalled( version.path ) )
        mapLine << "unknown until it is installed";
    else
        mapLine << ( sg_cockpitHasMap( version.path ) ? "yes" : "no" );

    SetColor( false, alpha * .8f );
    ::DisplayText( right, y, hText, mapLine, sr_fontMenu, -1 );
    y -= line * .9f;

    // the other parts of the cockpit in use
    if ( inUse )
    {
        tString partsHelp;
        int shownParts = 0;
        for ( int i = 0; i < (int)sg_hudParts.size() && shownParts < 4; ++i )
        {
            if ( sg_hudParts[i] == map )
                continue;

            tString part;
            part << sg_hudPartLabel( sg_hudParts[i] ) << ": "
                 << ( sg_hudParts[i]->Active() ? "shown" : "hidden" );

            if ( shownParts == 0 )
            {
                y -= line * .5f;
                SetColor( false, alpha * .8f );
                ::DisplayText( right, y, hText, "HUD parts", sr_fontMenu, -1 );
                y -= line;
            }

            SetColor( false, alpha * .75f );
            ::DisplayText( right, y, hText, part, sr_fontMenu, -1 );
            y -= line * .9f;
            ++shownParts;
        }
    }

    // the description, wrapped, so long text stays inside the pane
    y -= line * .5f;
    if ( sr_alphaBlend )
        glColor4f( .85f, .85f, .9f, alpha * .9f );
    else
        Color( alpha * .85f, alpha * .85f, alpha * .9f );

    rTextField c( right, y, hText, sr_fontMenu );
    c.SetWidth( .92f - right );
    c.EnableLineWrap();
    c << entry.description;
#endif
}

bool uMenuItemCockpitBrowser::Event( SDL_Event & event )
{
    if ( event.type != SDL_KEYDOWN )
        return false;

    sg_syncCockpitVersion();
    gCockpitCatalogueEntry const & entry = sg_selectedCockpit();

    switch ( event.key.keysym.sym )
    {
    case SDLK_UP:
    case SDLK_KP_8:
        // wrap around: the list is a loop, so leaving the top lands at the bottom
        if ( --sg_cockpitSelected < 0 )
            sg_cockpitSelected = (int)sg_cockpitOrder.size() - 1;
        sg_syncCockpitVersion();
        sg_previewCockpit();
        return true;

    case SDLK_DOWN:
    case SDLK_KP_2:
        if ( ++sg_cockpitSelected >= (int)sg_cockpitOrder.size() )
            sg_cockpitSelected = 0;
        sg_syncCockpitVersion();
        sg_previewCockpit();
        return true;

    case SDLK_LEFT:
    case SDLK_KP_4:
        if ( --sg_cockpitSelectedVersion < 0 )
            sg_cockpitSelectedVersion = entry.versionCount - 1;
        sg_previewCockpit();
        return true;

    case SDLK_RIGHT:
    case SDLK_KP_6:
        if ( ++sg_cockpitSelectedVersion >= entry.versionCount )
            sg_cockpitSelectedVersion = 0;
        sg_previewCockpit();
        return true;

    default:
        break;
    }

    if ( event.key.keysym.sym == SDLK_h || event.key.keysym.sym == SDLK_m )
    {
        // the same switch the bindable HUD_MAP action uses. H is the default
        // binding, M is what many players pick; accept both here.
        cCockpit::ToggleMap();
        return true;
    }

    if ( event.key.keysym.sym == SDLK_r )
    {
        sg_reloadCockpit();
        return true;
    }

    return false;
}

void uMenuItemCockpitBrowser::Enter()
{
    sg_syncCockpitVersion();

    gCockpitCatalogueEntry const & entry = sg_selectedCockpit();
    int const v = sg_cockpitSelectedVersion >= 0 && sg_cockpitSelectedVersion < entry.versionCount ? sg_cockpitSelectedVersion : 0;

    gCockpitVersion const & version = entry.versions[v];
    tString const path = sg_cockpitPath( version );

    if ( !sg_cockpitInstalled( version.path ) )
        con << "Fetching cockpit " << version.path << " ...\n";

    cCockpit::SetFile( path );
    sg_cockpitCommitted = path;      // this one is in use now, not just previewed

    sg_findCockpits();
    sg_buildCockpitOrder();
    sg_findHudParts();
}

static uMenuItemCockpitBrowser modded_cockpit( &sg_cockpitMenu );

//! the entry that puts the cockpit browser into Modded Settings. It only stores
//! the pointer to the menu, so the submenu's own name ("Cockpits") is the label.
static uMenuItemSubmenu modded_cockpitEntry( &sg_moddedMenu, &sg_cockpitMenu,
                                             "Choose the HUD cockpit, its version and which parts of it to draw" );

static tConfItemLine c_ext("GL_EXTENSIONS",gl_extensions);
static tConfItemLine c_ver("GL_VERSION",gl_version);
static tConfItemLine c_rEnd("GL_RENDERER",gl_renderer);
static tConfItemLine c_vEnd("GL_VENDOR",gl_vendor);
// static tConfItemLine a_ver("ARMAGETRON_VERSION",st_programVersion);

static uMenuItemStringWithHistory::history_t &sg_consoleHistory() {
    static uMenuItemStringWithHistory::history_t instance("console_history.txt");
    return instance;
}

static int sg_consoleHistoryMaxSize=100; // size of the console history
static tSettingItem< int > sg_consoleHistoryMaxSizeConf("HISTORY_SIZE_CONSOLE",sg_consoleHistoryMaxSize);

class ArmageTron_feature_menuitem: public uMenuItemSelection<int>{
    void NewChoice(uSelectItem<bool> *){}
    void NewChoice(char *,bool ){}
public:
    ArmageTron_feature_menuitem(uMenu *m,char const * tit,char const * help,int &targ)
            :uMenuItemSelection<int>(m,tit,help,targ){
        uMenuItemSelection<int>::NewChoice(
            "$feature_disabled_text",
            "$feature_disabled_help",
            rFEAT_OFF);
        uMenuItemSelection<int>::NewChoice(
            "$feature_default_text",
            "$feature_default_help",
            rFEAT_DEFAULT);
        uMenuItemSelection<int>::NewChoice(
            "$feature_enabled_text",
            "$feature_enabled_help",
            rFEAT_ON);
    }

    ~ArmageTron_feature_menuitem(){}
};


class ArmageTron_texmode_menuitem: public uMenuItemSelection<int>{
    void NewChoice(uSelectItem<bool> *){}
    void NewChoice(char *,bool ){}
public:
    ArmageTron_texmode_menuitem(uMenu *m,char const * tit,int &targ,
                                bool font=false)
            :uMenuItemSelection<int>
    (m,tit,"$texture_menuitem_help",targ){

        if(!font)
            uMenuItemSelection<int>::NewChoice
            ("$texture_off_text","$texture_off_help",-1);
#ifndef DEDICATED
        uMenuItemSelection<int>::NewChoice
        ("$texture_nearest_text","$texture_nearest_help",GL_NEAREST);

        uMenuItemSelection<int>::NewChoice
        ("$texture_bilinear_text","$texture_bilinear_help",GL_LINEAR);

        if(!font)
        {
            uMenuItemSelection<int>::NewChoice
            ("$texture_mipmap_nearest_text",
             "$texture_mipmap_nearest_help",
             GL_NEAREST_MIPMAP_NEAREST);
            uMenuItemSelection<int>::NewChoice
            ("$texture_mipmap_bilinear_text",
             "$texture_mipmap_bilinear_help",
             GL_LINEAR_MIPMAP_NEAREST);
            uMenuItemSelection<int>::NewChoice
            ("$texture_mipmap_trilinear_text",
             "$texture_mipmap_trilinear_help",
             GL_LINEAR_MIPMAP_LINEAR);
        }
    #endif
    }

    ~ArmageTron_texmode_menuitem(){}
};

static tConfItem<bool>    ab("ALPHA_BLEND",sr_alphaBlend);
static tConfItem<bool>    ss("SMOOTH_SHADING",sr_smoothShading);
static tConfItem<bool>    to("TEXT_OUT",sr_textOut);
static tConfItem<bool>    fps("SHOW_FPS",sr_FPSOut);
// tConfItem<> ("",&);
static tConfItem<int> fm("FLOOR_MIRROR",sr_floorMirror);
static tConfItem<int> fd("FLOOR_DETAIL",sr_floorDetail);
static tConfItem<bool> hr("HIGH_RIM",sr_highRim);
static tConfItem<bool> dt("DITHER",sr_dither);
static tConfItem<bool> us("UPPER_SKY",sr_upperSky);
static tConfItem<bool> ls("LOWER_SKY",sr_lowerSky);
static tConfItem<bool> wos("SKY_WOBBLE",sr_skyWobble);
static tConfItem<bool> ip("INFINITY_PLANE",sr_infinityPlane);

extern bool sg_axesIndicator;

static tConfItem<bool> lm("LAG_O_METER",sr_laggometer);
static tConfItem<bool> ai("AXES_INDICATOR",sg_axesIndicator);
static tConfItem<bool> po("PREDICT_OBJECTS",sr_predictObjects);
static tConfItem<bool> t32("TEXTURES_HI",sr_texturesTruecolor);

static tConfItem<bool> kwa("KEEP_WINDOW_ACTIVE",sr_keepWindowActive);
#ifdef USE_HEADLIGHT
static tConfItem<bool> chl("HEADLIGHT",headlights);
#endif

#ifndef SDL_OPENGL
#ifndef DIRTY
#define DIRTY
#endif
#endif

bool operator < ( rScreenSize const & a, rScreenSize const & b )
{
    return a.Compare(b) < 0;
}

#if SDL_VERSION_ATLEAST(2,0,0)
class gRefreshRateMenuItem: public uMenuItemSelection<int>
{
public:
    void Rescan()
    {
        Clear();

        // collect refresh rates
        std::set<int> refreshRates;
        int modes = SDL_GetNumDisplayModes(currentScreensetting.displayIndex);

        for(int run = 0; run <= 1; ++run)
        {
            int width = currentScreensetting.res.width;
            int height = currentScreensetting.res.height;
            
            if(width + height == 0)
            {
                // desktop mode, get real res
                SDL_DisplayMode mode;
                if(SDL_GetDesktopDisplayMode(currentScreensetting.displayIndex, &mode) == 0)
                {
                    width = mode.w;
                    height = mode.h;
                }
            }

            for(int i = modes-1; i >= 0; --i)
            {
                SDL_DisplayMode mode;
                if (0>SDL_GetDisplayMode(currentScreensetting.displayIndex, i, &mode)) continue;

                // first scan only matching modes, then all
                if(run == 0)
                {
                    if(width != mode.w)
                        continue;
                    if(height != mode.h)
                        continue;
                }
                if(refreshRates.find(mode.refresh_rate) == refreshRates.end())
                {
                    refreshRates.insert(mode.refresh_rate);
                }
            }

            if(refreshRates.size() > 0)
                break;
        }

        // emergenct default fallback
        if(refreshRates.size() == 0)
            refreshRates.insert(60);

        // add them to menu
        for(std::set<int>::iterator i = refreshRates.begin(); i != refreshRates.end(); ++i)
        {
            int displayRate = *i;

            // round up display rates ending in 9.
            if((displayRate % 10) == 9 && refreshRates.find(displayRate+1) == refreshRates.end())
                displayRate++;

            tOutput text("$screen_refreshrate_select", displayRate);
            NewChoice(text, text, *i);
        }

        // add default on top
        NewChoice("$screen_refreshrate_desk_text", "$screen_refreshrate_desk_help", 0);

        select = -1;
        PickFromValue();
        if(select < 0)
        {
            select = choices.Len()-1;
        }
        LeftRight(0);
    }

    gRefreshRateMenuItem( uMenu & screen_menu_mode, const tOutput& text, const tOutput& help )
            :uMenuItemSelection<int>
            (&screen_menu_mode,
             text,
             help,
             currentScreensetting.refreshRate)
    {
        Rescan();
    }
};

static gRefreshRateMenuItem * sg_refreshRateMenuItem = NULL;
#endif // SDL_VER

class gResolutionMenuItem: uMenuItemSelection<rScreenSize> 
{
    std::set< rScreenSize > sizes;           // set of already added modes
    bool addFixed; // true if fixed set of resolutions is to be added

    // adds a single custom screen resolution
    void NewSize( rScreenSize const & size )
    {
        if ( sizes.find( size ) == sizes.end() )
        {
            sizes.insert(size);
        }
    }

    // adds a single predefined resolution
    void NewSize( rResolution res )
    {
        rScreenSize size( res );
        NewSize( size );
    }

public:
    void Rescan()
    {
        sizes.clear();
        Clear();

#ifndef DEDICATED
        // fetch valid screen modes from SDL
        int i;
#if SDL_VERSION_ATLEAST(2,0,0)
        int modes = SDL_GetNumDisplayModes(currentScreensetting.displayIndex);

        // Check is there are any modes available
        if(modes < 0)
#else
        SDL_Rect **modes;
        modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_OPENGL);

        // Check is there are any modes available
        if(modes == 0 || modes == (SDL_Rect **)-1)
#endif

        {
            // add all fixed resolutions
            for ( i = ArmageTron_Custom; i>=0; --i )
            {
                NewSize( rResolution(i) );
            }
        }
        else
        {
            // add custom resolution
            NewSize( ArmageTron_Custom );

            // add desktop resolution
            if ( sr_DesktopScreensizeSupported() && !addFixed )
                NewSize( ArmageTron_Desktop );

            // the maximal allowed screen size
            rScreenSize maxSize(0,0);

            // fill in available modes (avoid duplicates)
#if SDL_VERSION_ATLEAST(2,0,0)
            for ( i = 0 ; i < modes ; i++ )
            {
                SDL_DisplayMode mode;
                // add mode (if it's new)
                if (0>SDL_GetDisplayMode(currentScreensetting.displayIndex, i, &mode)) continue;
                rScreenSize size(mode.w, mode.h);
 #else
            for(i=0;modes[i];++i)
            {
                // add mode (if it's new)
                rScreenSize size(modes[i]->w, modes[i]->h);
#endif
                NewSize( size );
                if ( maxSize.width < size.width )
                    maxSize.width = size.width;
                if ( maxSize.height < size.height )
                    maxSize.height = size.height;
            }

            // add fixed resolutions (as window sizes)
            if ( addFixed )
            {
                for ( i = ArmageTron_Custom; i>=ArmageTron_Min; --i )
                {
                    rScreenSize size( static_cast< rResolution >(i) );

                    // only add those that fit the maximal resolution
                    if ( maxSize.height >= size.height && maxSize.width >= size.width )
                        NewSize( size );
                }
            }
        }

        // insert sorted resolutions into menu
        for( std::set< rScreenSize >::iterator iter = sizes.begin(); iter != sizes.end(); ++iter )
            {
                rScreenSize const & size = *iter;

                std::stringstream s;
                if ( size.width + size.height > 0 )
                    s << size.width << " x " << size.height;
                else
                    s << tOutput("$screen_size_desktop");

                NewChoice( s.str().c_str(), "", size );
            }

#endif

        select = -1;
        PickFromValue();
        if(select < 0)
        {
            select = choices.Len()-1;
        }
        LeftRight(0);
    }

        
    virtual void LeftRight(int lr)
    {
        uMenuItemSelection<rScreenSize>::LeftRight(lr);

#if SDL_VERSION_ATLEAST(2,0,0)       
        if(sg_refreshRateMenuItem)
            sg_refreshRateMenuItem->Rescan();
#endif
    }

    gResolutionMenuItem( uMenu & screen_menu_mode, rScreenSize& res, const tOutput& text, const tOutput& help, bool addFixed )
            :uMenuItemSelection<rScreenSize>
            (&screen_menu_mode,
             text,
             help,
             res)
    {
        this->addFixed = addFixed;

        Rescan();
    }
};

static gResolutionMenuItem * sg_windowResMen = NULL, * sg_screenResMen = NULL;

class gDisplayMenuItem: public uMenuItemInt
{
public:
    gDisplayMenuItem( uMenu & screen_menu_mode, const tOutput& text, const tOutput& help, int max )
            :uMenuItemInt
            (&screen_menu_mode,
             text,
             help,
             currentScreensetting.displayIndex, 0, max)
    {
    }

    virtual void LeftRight(int lr)
    {
        uMenuItemInt::LeftRight(lr);
        
        if(sg_windowResMen)
        {
            sg_windowResMen->Rescan();
        }
        if(sg_screenResMen)
        {
            sg_screenResMen->Rescan();
        }
    }
};

static void sg_ScreenModeAdvanced()
{
    uMenu screen_menu_mode("$screen_mode_advanced");
    
    uMenuItemToggle gm(
        &screen_menu_mode,
        "$screen_grab_mouse_text",
        "$screen_grab_mouse_help",
        su_mouseGrab);

    uMenuItemToggle kwa_t(
        &screen_menu_mode,
        "$screen_keep_window_active_text",
        "$screen_keep_window_active_help",
        sr_keepWindowActive);

#if !SDL_VERSION_ATLEAST(2,0,0)
    uMenuItemToggle ie_t
    (&screen_menu_mode,
     "$screen_check_errors_text",
     "$screen_check_errors_help",
     currentScreensetting.checkErrors);
#endif

    // frame rate limit
    std::unique_ptr<uMenuItem> zfm_t;
    {
        // list of hand-picked MAX_FPS values
        std::array<int, 17> pickableLimits{0, 20, 30, 40, 60, 80, 100, 120, 180, 240, 300, 360, 480, 600, 720, 900, 1200};
        bool pickable = false;
        for (auto const limit : pickableLimits)
        {
            if (limit == sr_maxFPS)
                pickable = true;
        }

        if (pickable)
        {
            // current value is in pickable list. create selection menu item
            auto zmf_t_p = new uMenuItemSelection<int>(&screen_menu_mode,
                                                       "$screen_max_fps_text",
                                                       "$max_fps_help",
                                                       sr_maxFPS);
            zfm_t.reset(zmf_t_p);

            zmf_t_p->NewChoice("$screen_max_fps_off_text", "$screen_max_fps_off_help", 0);
            for (auto const limit : pickableLimits)
            {
                if (!limit)
                    continue;
                std::stringstream s;
                s << limit;
                zmf_t_p->NewChoice(s.str().c_str(), "", limit);
            }
        }
        else
        {
            // fallback to ranged item
            zfm_t.reset(new uMenuItemInt(&screen_menu_mode, "$screen_max_fps_text", "$max_fps_help", sr_maxFPS, 0, 9990, 10));
        }
    }

#ifdef SDL_OPENGL
#if SDL_VERSION_ATLEAST(1, 2, 10)
    uMenuItemSelection<rVSync> zvs_t
    (&screen_menu_mode,
     "$screen_vsync_text",
     "$screen_vsync_help",
     currentScreensetting.vSync);

    uSelectEntry<rVSync> zvs_on(zvs_t,"$screen_vsync_on_text","$screen_vsync_on_help",ArmageTron_VSync_On);
    uSelectEntry<rVSync> zvs_d(zvs_t,"$screen_vsync_default_text","$screen_vsync_default_help",ArmageTron_VSync_Default);
    uSelectEntry<rVSync> zvs_off(zvs_t,"$screen_vsync_off_text","$screen_vsync_off_help",ArmageTron_VSync_Off);
#ifdef HAVE_GLEW
    uSelectEntry<rVSync> zvs_blur(zvs_t,"$screen_vsync_motionblur_text","$screen_vsync_motionblur_help",ArmageTron_VSync_MotionBlur);
#endif // HAVE_GLEW
#endif // SDL_GL_SWAP_CONTROL
#endif // SDL_OPENGL

    uMenuItemSelection<rColorDepth> zd_t
    (&screen_menu_mode,
     "$screen_zdepth_text",
     "$screen_zdepth_help",
     currentScreensetting.zDepth);

    uSelectEntry<rColorDepth> zd_16(zd_t,"$screen_zdepth_16_text","$screen_zdepth_16_help",ArmageTron_ColorDepth_16);
    uSelectEntry<rColorDepth> zd_d(zd_t,"$screen_zdepth_desk_text","$screen_zdepth_desk_help",ArmageTron_ColorDepth_Desktop);
    uSelectEntry<rColorDepth> zd_32(zd_t,"$screen_zdepth_32_text","$screen_zdepth_32_help",ArmageTron_ColorDepth_32);

    uMenuItemSelection<rColorDepth> cd_t
    (&screen_menu_mode,
     "$screen_colordepth_text",
     "$screen_colordepth_help",
     currentScreensetting.colorDepth);

    uSelectEntry<rColorDepth> cd_16(cd_t,"$screen_colordepth_16_text","$screen_colordepth_16_help",ArmageTron_ColorDepth_16);
    uSelectEntry<rColorDepth> cd_d(cd_t,"$screen_colordepth_desk_text","$screen_colordepth_desk_help",ArmageTron_ColorDepth_Desktop);
    uSelectEntry<rColorDepth> cd_32(cd_t,"$screen_colordepth_32_text","$screen_colordepth_32_help",ArmageTron_ColorDepth_32);

    screen_menu_mode.Enter();
}

static void sg_ScreenModeMenu()
{
    uMenu screen_menu_mode("$screen_mode_menu");

    uMenuItemFunction appl
    (&screen_menu_mode,
     "$screen_apply_changes_text",
     "$screen_apply_changes_help",
     &sr_ReinitDisplay);

    uMenuItemFunction sma(&screen_menu_mode,"$screen_mode_advanced",
                          "$screen_mode_advanced_help", sg_ScreenModeAdvanced );

    gResolutionMenuItem winsize( screen_menu_mode, currentScreensetting.windowSize, "$window_size_text", "$window_size_help", true );

    uMenuItemToggle fs_t
    (&screen_menu_mode,
     "$screen_fullscreen_text",
     "$screen_fullscreen_help",
     currentScreensetting.fullscreen);

#ifdef MACOSX
    uMenuItemToggle lowdpi_t
    (&screen_menu_mode,
     "$screen_lowdpi_text",
     "$screen_lowdpi_help",
     currentScreensetting.lowDPIWindow);
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
    gRefreshRateMenuItem refreshRate
    (screen_menu_mode,
     "$screen_refreshrate_text",
     "$screen_refreshrate_help");

    sg_refreshRateMenuItem = &refreshRate;
#endif


    gResolutionMenuItem res( screen_menu_mode, currentScreensetting.res, "$screen_resolution_text", "$screen_resolution_help", false );

    sg_windowResMen = &winsize;
    sg_screenResMen = &res;

#if SDL_VERSION_ATLEAST(2,0,0)

    int numDisplays = SDL_GetNumVideoDisplays();
    std::unique_ptr<uMenuItemInt> pDisplayIndex;
    if(numDisplays > 1)
    {
        pDisplayIndex.reset( tNEW(gDisplayMenuItem)
        (screen_menu_mode,
         "$screen_displayindex_text",
         "$screen_displayindex_help",
         numDisplays-1) );
    }
#endif

    screen_menu_mode.Enter();

    sg_windowResMen = NULL;
    sg_screenResMen = NULL;

#if SDL_VERSION_ATLEAST(2,0,0)       
    sg_refreshRateMenuItem = NULL;
#endif
}


#define gZONE_STYLE_VERT 0
#define gZONE_STYLE_FLAT 1
#define gZONE_STYLE_COMB 2
#define gZONE_STYLE_CUST 3
static int sg_zoneStyle;

void zone_style_on_select(int const& val) {
    // default include files are executed at owner level
    tCurrentAccessLevel level( tAccessLevel_Owner, true );

    // hardcoded includes
    switch (val) {
        case gZONE_STYLE_VERT:
            st_Include(tString("zonestyle_vertical.cfg"));
            break;
        case gZONE_STYLE_FLAT:
            st_Include(tString("zonestyle_flat.cfg"));
            break;
        case gZONE_STYLE_COMB:
            st_Include(tString("zonestyle_combined.cfg"));
    }
}

extern int  sz_zoneSegments;
extern int  sz_zoneSegSteps;
extern REAL sz_zoneSegLength;
extern REAL sz_zoneBottom;
extern REAL sz_zoneHeight;
extern REAL sz_zoneFloorScalePercent;
extern REAL sz_zoneProximityDistance;
extern REAL sz_zoneProximityOffset;

void zone_style_on_enter(int const& val) {
    if (val!=gZONE_STYLE_CUST) return;
    uMenu customMenu("$zone_style_custom_menu");
    uMenuItemReal cm8(&customMenu,
                      "$zone_style_proximity_offset_text",
                      "$zone_style_proximity_offset_help",
                      sz_zoneProximityOffset, (REAL)0, (REAL)100, (REAL)1 );
    uMenuItemReal cm7(&customMenu,
                      "$zone_style_proximity_distance_text",
                      "$zone_style_proximity_distance_help",
                      sz_zoneProximityDistance, (REAL)-1, (REAL)100, (REAL)1 );
    uMenuItemReal cm6(&customMenu,
                      "$zone_style_floor_scale_pct_text",
                      "$zone_style_floor_scale_pct_help",
                      sz_zoneFloorScalePercent, (REAL)0, (REAL)1, (REAL).05 );
    uMenuItemReal cm5(&customMenu,
                      "$zone_style_height_text",
                      "$zone_style_height_help",
                      sz_zoneHeight, (REAL)0, (REAL)20, (REAL)1 );
    uMenuItemReal cm4(&customMenu,
                      "$zone_style_bottom_text",
                      "$zone_style_bottom_help",
                      sz_zoneBottom, (REAL)0, (REAL)10, (REAL)1 );
    uMenuItemReal cm3(&customMenu,
                      "$zone_style_segment_length_text",
                      "$zone_style_segment_length_help",
                      sz_zoneSegLength, (REAL)0, (REAL)1, (REAL).05 );
    uMenuItemInt  cm2(&customMenu,
                      "$zone_style_segment_steps_text",
                      "$zone_style_segment_steps_help",
                      sz_zoneSegSteps, 1, 16, 1);
    uMenuItemInt  cm1(&customMenu,
                      "$zone_style_segments_text",
                      "$zone_style_segments_help",
                      sz_zoneSegments, 2, 20, 1);
    customMenu.SetCenter(.3);
    customMenu.Enter();
}

static uMenuItemSelection<int> mzm
(&screen_menu_prefs,
 "$detail_zone_style_text",
 "$detail_zone_style_help",
 sg_zoneStyle, &zone_style_on_select, &zone_style_on_enter);
static uSelectEntry<int> mzm0(mzm,"$detail_zone_style_vertical_text",
                              "$detail_zone_style_vertical_help", gZONE_STYLE_VERT);
static uSelectEntry<int> mzm1(mzm,"$detail_zone_style_flat_text",
                              "$detail_zone_style_flat_help", gZONE_STYLE_FLAT);
static uSelectEntry<int> mzm2(mzm,"$detail_zone_style_combined_text",
                              "$detail_zone_style_combined_help", gZONE_STYLE_COMB);
static uSelectEntry<int> mzm3(mzm,"$detail_zone_style_custom_text",
                              "$detail_zone_style_custom_help", gZONE_STYLE_CUST);


static uMenuItemSelection<int> mfm
(&screen_menu_detail,
 "$detail_floor_mirror_text",
 "$detail_floor_mirror_help",
 sr_floorMirror);
static uSelectEntry<int> mfma(mfm,"$detail_floor_mirror_off_text","$detail_floor_mirror_off_help",rMIRROR_OFF);


static uSelectEntry<int> mfmb(mfm,"$detail_floor_mirror_obj_text",
                              "$detail_floor_mirror_obj_help",
                              rMIRROR_OBJECTS);

static uSelectEntry<int> mfmc(mfm,"$detail_floor_mirror_ow_text",
                              "$detail_floor_mirror_ow_help",
                              rMIRROR_WALLS);

static uSelectEntry<int> mfme(mfm,"$detail_floor_mirror_ev_text","$detail_floor_mirror_ev_help",rMIRROR_ALL);

static uMenuItemToggle fs_dither
(&screen_menu_detail,"$detail_dither_text",
 "$detail_dither_help",
 sr_dither);

// from gWall.cpp
extern bool sg_simpleTrail;
static uMenuItemToggle sgm_simpleTrail
(&screen_menu_detail,
 "$detail_simple_trail_text",
 "$detail_simple_trail_help",
 sg_simpleTrail);

static uMenuItemSelection<int> mfd
(&screen_menu_detail,
 "$detail_floor_text",
 "$detail_floor_help",
 sr_floorDetail);

static uSelectEntry<int> mfda(mfd,"$detail_floor_no_text",
                              "$detail_floor_no_help",
                              rFLOOR_OFF);
static uSelectEntry<int> mfdb(mfd,"$detail_floor_grid_text",
                              "$detail_floor_grid_help",
                              rFLOOR_GRID);
static uSelectEntry<int> mfdc(mfd,"$detail_floor_tex_text",
                              "$detail_floor_tex_help",
                              rFLOOR_TEXTURE);
static uSelectEntry<int> mfdd(mfd,"$detail_floor_2tex_text",
                              "$detail_floor_2tex_help",
                              rFLOOR_TWOTEXTURE);

static uMenuItemToggle  abm
(&screen_menu_detail,"$detail_alpha_text",
 "$detail_alpha_help",
 sr_alphaBlend);

static uMenuItemToggle  ssm
(&screen_menu_detail,"$detail_smooth_text",
 "$detail_smooth_help",
 sr_smoothShading);

extern bool crash_sparks;		// from gCycle.cpp
extern bool white_sparks;		// from gSparks.cpp
static tConfItem<bool> cs2("SPARKS",crash_sparks);
static tConfItem<bool> wsp("WHITE_SPARKS",white_sparks);

extern bool sg_crashExplosion;   // from gExplosion.cpp
static tConfItem<bool> crexp("EXPLOSION",sg_crashExplosion);
extern bool sg_crashExplosionHud;   // from gExplosion.cpp
static tConfItem<bool> crexph("EXPLOSION_HUD",sg_crashExplosionHud);

//extern bool png_screenshot;		// from rSysdep.cpp
//static tConfItem<bool> pns("PNG_SCREENSHOT",png_screenshot);

static uMenuItemToggle  t32b
(&screen_menu_detail,"$detail_text_truecolor_text",
 "$detail_text_truecolor_help"
 ,sr_texturesTruecolor);


static ArmageTron_texmode_menuitem tmm0(&screen_menu_detail,
                                        rTextureGroups::TextureGroupDescription[0],
                                        rTextureGroups::TextureMode[0]);

static ArmageTron_texmode_menuitem tmm1(&screen_menu_detail,
                                        rTextureGroups::TextureGroupDescription[1],
                                        rTextureGroups::TextureMode[1]);

static ArmageTron_texmode_menuitem tmm2(&screen_menu_detail,
                                        rTextureGroups::TextureGroupDescription[2],
                                        rTextureGroups::TextureMode[2]);

static ArmageTron_texmode_menuitem tmm3(&screen_menu_detail,
                                        rTextureGroups::TextureGroupDescription[3],
                                        rTextureGroups::TextureMode[3],true);


static uMenuItemToggle s2
(&screen_menu_prefs,"$pref_highrim_text",
 "$pref_highrim_help",sr_highRim);

static uMenuItemToggle us2
(&screen_menu_prefs,"$pref_uppersky_text",
 "$pref_uppersky_help",
 sr_upperSky);

static uMenuItemToggle ls2
(&screen_menu_prefs,"$pref_lowersky_text",
 "$pref_lowersky_help",
 sr_lowerSky);

uMenuItemToggle fps2
(&screen_menu_prefs,"$misc_fps_text",
 "$misc_fps_help",sr_FPSOut);

#ifdef USE_HEADLIGHT
static uMenuItemToggle uchl(&screen_menu_prefs,"$pref_headlight_text","$pref_headlight_help",
                            headlights);
#endif


static uMenuItemToggle ws2
(&screen_menu_prefs,"$pref_skymove_text",
 "$pref_skymove_help",
 sr_skyWobble);

static uMenuItemToggle crexph2
(&screen_menu_prefs,"$pref_explosion_hud_text",
 "$pref_explosion_hud_help",
 sg_crashExplosionHud);

static uMenuItemToggle crexp2
(&screen_menu_prefs,"$pref_explosion_text",
 "$pref_explosion_help",
 sg_crashExplosion);

static uMenuItemToggle cs
(&screen_menu_prefs,"$pref_sparks_text",
 "$pref_sparks_help",
 crash_sparks);

static uMenuItemSelection<rDisplayListUsage> dl
(&screen_menu_tweaks,"$tweaks_displaylists_text",
 "$tweaks_displaylists_help", sr_useDisplayLists);
static uSelectEntry<rDisplayListUsage> dl_off(dl,"$tweaks_displaylists_off_text","$tweaks_displaylists_off_help",rDisplayList_Off);
static uSelectEntry<rDisplayListUsage> dl_cac(dl,"$tweaks_displaylists_cac_text","$tweaks_displaylists_cac_help",rDisplayList_CAC);
static uSelectEntry<rDisplayListUsage> dl_cae(dl,"$tweaks_displaylists_cae_text","$tweaks_displaylists_cae_help",rDisplayList_CAE);

static uMenuItemToggle infp
(&screen_menu_tweaks,"$tweaks_infinity_text",
 "$tweaks_infinity_help"
 ,sr_infinityPlane);

uMenuItemSelection<rSysDep::rFramedropTolerance> framedropTolerance
(&screen_menu_tweaks,
 "$framedroptolerance_text",
 "$framedroptolerance_help",
 rSysDep::framedropTolerance_);

static uSelectEntry<rSysDep::rFramedropTolerance> framedropTolerance_Lenient(framedropTolerance,"$framedrop_tolerance_lenient_text","$framedrop_tolerance_lenient_help",rSysDep::rSwap_Lenient);
static uSelectEntry<rSysDep::rFramedropTolerance> framedropTolerance_Normal(framedropTolerance,"$framedrop_tolerance_normal_text","$framedrop_tolerance_normal_help",rSysDep::rSwap_Normal);
static uSelectEntry<rSysDep::rFramedropTolerance> framedropTolerance_Strict(framedropTolerance,"$framedrop_tolerance_strict_text","$framedrop_tolerance_strict_help",rSysDep::rSwap_Strict);
static uSelectEntry<rSysDep::rFramedropTolerance> framedropTolerance_Draconic(framedropTolerance,"$framedrop_tolerance_draconic_text","$framedrop_tolerance_draconic_help",rSysDep::rSwap_Draconic);

tCONFIG_ENUM( rSysDep::rFramedropTolerance );

static tConfItem< rSysDep::rFramedropTolerance > framedropToleranceCI("FRAMEDROP_TOLERANCE", rSysDep::framedropTolerance_ );

uMenuItemSelection<rSysDep::rSwapOptimize> swapOptimize
(&screen_menu_tweaks,
 "$swapoptimize_text",
 "$swapoptimize_help",
 rSysDep::swapOptimize_);

static uSelectEntry<rSysDep::rSwapOptimize> swapOptimize_Latency(swapOptimize,"$swapoptimize_latency_text","$swapoptimize_latency_help",rSysDep::rSwap_Latency);
static uSelectEntry<rSysDep::rSwapOptimize> swapOptimize_Auto(swapOptimize,"$swapoptimize_auto_text","$swapoptimize_auto_help",rSysDep::rSwap_Auto);
static uSelectEntry<rSysDep::rSwapOptimize> swapOptimize_Throughput(swapOptimize,"$swapoptimize_throughput_text","$swapoptimize_throughput_help",rSysDep::rSwap_Throughput);
static uSelectEntry<rSysDep::rSwapOptimize> swapOptimize_ThroughputFlush(swapOptimize,"$swapoptimize_throughput_flush_text","$swapoptimize_throughput_flush_help",rSysDep::rSwap_ThroughputFlush);
static uSelectEntry<rSysDep::rSwapOptimize> swapOptimize_ThroughputFastest(swapOptimize,"$swapoptimize_throughput_fastest_text","$swapoptimize_throughput_fastest_help",rSysDep::rSwap_ThroughputFastest);

tCONFIG_ENUM( rSysDep::rSwapOptimize );

static tConfItem< rSysDep::rSwapOptimize > swapOptimizeCI("SWAP_OPTIMIZE", rSysDep::swapOptimize_ );

static tConfItem<bool> WRAP("WRAP_MENU",uMenu::wrap);

class gAutoCompleterConsole : public uAutoCompleter {
public:
    int Complete(tString &string, unsigned pos) {
        if(string.find_first_of(' ') == pos - 1) {
            tConfItemBase *cfg = tConfItemBase::FindConfigItem(string.substr(0, pos-1));
            if(cfg != 0) {
                std::ostringstream toAdd("");
                cfg->WriteVal(toAdd);
                string << toAdd.str();
                return string.size();
            }
        }
        //delegate
        return uAutoCompleter::Complete(string, pos);
    }
    gAutoCompleterConsole(std::deque<tString> &words) : uAutoCompleter(words) {}
};

//! Handles the console prompt
class gMemuItemConsole: uMenuItemStringWithHistory{
public:
    gMemuItemConsole(uMenu *M,tString &c, uAutoCompleter *completer); //!< Constructor
    virtual ~gMemuItemConsole(){}
    bool Event(SDL_Event &e);
};

//! @param M         passed on to uMenuItemStringWithHistory
//! @param c         passed on to uMenuItemStringWithHistory
//! @param completer the completer to be used
gMemuItemConsole::gMemuItemConsole(uMenu *M,tString &c, uAutoCompleter *completer):
        uMenuItemStringWithHistory(M,"Con:","", c, 1024, sg_consoleHistory(), sg_consoleHistoryMaxSize, completer)
{}


bool gMemuItemConsole::Event(SDL_Event &e){
    if (e.type==SDL_KEYDOWN &&
            (e.key.keysym.sym==SDLK_KP_ENTER || e.key.keysym.sym==SDLK_RETURN)){

        con << tColoredString::ColorString(.5,.5,1) << " > " << *content << '\n';

       // direct commands are executed at owner level
       tCurrentAccessLevel level( tAccessLevel_Owner, true );

       // pass the console command to the configuration system
       std::stringstream s(&((*content)[0]));
       tConfItemBase::LoadAll( s, false );

        MyMenu()->Exit();
        return true;
    }
    else if (e.type==SDL_KEYDOWN &&
             uActionGlobal::IsBreakingGlobalBind(e.key.keysym.sym))
        return su_HandleEvent(e, true);
    else
        return uMenuItemStringWithHistory::Event(e);
}

void do_con(){
    su_ClearKeys();

    se_ChatState( ePlayerNetID::ChatFlags_Console, true );
    sr_con.SetHeight(20,false);
    se_SetShowScoresAuto(false);
    tString c;

    uMenu con_menu("",false);
    std::deque<tString> commands;
    commands = tConfItemBase::GetCommands();
    gAutoCompleterConsole completer(commands);
    gMemuItemConsole s(&con_menu,c,&completer);
    con_menu.SetCenter(-.75);
    con_menu.SetBot(-2);
    con_menu.SetTop(-.7);
    con_menu.Enter();

    se_ChatState( ePlayerNetID::ChatFlags_Console, false );

    se_SetShowScoresAuto(true);
    sr_con.SetHeight(7,false);
}
#endif

void sg_ConsoleInput(){
#ifndef DEDICATED
    st_ToDoOnce(&do_con);
#endif
}















class ArmageTron_viewport_menuitem:public uMenuItemInt{
public:
    ArmageTron_viewport_menuitem(uMenu *m):
            uMenuItemInt(m,"$viewport_menu_title",
                         "$viewport_menu_help",
                         rViewportConfiguration::next_conf_num,
                 0,rViewportConfiguration::s_viewportNumConfigurations-1){
        m->RequestSpaceBelow(.9);
    }

    virtual REAL SpaceRight(){return 1;}

    virtual void RenderBackground(){
        uMenuItem::RenderBackground();

        if (rViewportConfiguration::next_conf_num<0) rViewportConfiguration::next_conf_num=0;
        if (rViewportConfiguration::next_conf_num>=rViewportConfiguration::s_viewportNumConfigurations)
            rViewportConfiguration::next_conf_num=rViewportConfiguration::s_viewportNumConfigurations-1;

        tString  titles[MAX_VIEWPORTS];

        for(int i=MAX_VIEWPORTS-1;i>=0;i--)
            titles[i] << i+1;
#ifndef DEDICATED
        rViewportConfiguration::DemonstrateViewport(titles);
#endif
    }

    virtual void Render(REAL x,REAL y,REAL alpha=1,bool selected=0){
        if (rViewportConfiguration::next_conf_num<0) rViewportConfiguration::next_conf_num=0;
        if (rViewportConfiguration::next_conf_num>=rViewportConfiguration::s_viewportNumConfigurations)
            rViewportConfiguration::next_conf_num=rViewportConfiguration::s_viewportNumConfigurations-1;

        tOutput disp;

        disp << "$viewport_conf_text";
        disp.AddSpace();
        disp << rViewportConfiguration::s_viewportConfigurationNames[rViewportConfiguration::next_conf_num];
        DisplayText(x-.02,y,disp,selected,alpha);
    }

};



class ArmageTronPlayer_to_viewport_menuitem:public uMenuItemInt{
    int    vp;
public:
    ArmageTronPlayer_to_viewport_menuitem(uMenu *m,int v):
            uMenuItemInt(m,"",
                         "$viewport_assign_help",
                         s_newViewportBelongsToPlayer[v],
                 0,MAX_PLAYERS-1),vp(v){
        m->RequestSpaceBelow(.9);
    }

    virtual REAL SpaceRight(){return 1;}

    virtual void LeftRight(int x){
        rViewport::SetDirectionOfCorrection(vp,x);
        target=(target + MAX_PLAYERS + x) % MAX_PLAYERS;
    }

    virtual void RenderBackground(){
        rViewport::CorrectViewport(vp, MAX_PLAYERS);

        uMenuItem::RenderBackground();

        tString titles[MAX_VIEWPORTS];
        for(int i=MAX_VIEWPORTS-1;i>=0;i--){
            titles[i] << s_newViewportBelongsToPlayer[i]+1;
            titles[i] << " (";
            titles[i] << ePlayer::PlayerConfig(s_newViewportBelongsToPlayer[i])->Name();
            titles[i] << ")";
        }
#ifndef DEDICATED
        rViewportConfiguration::DemonstrateViewport(titles);
#endif
    }

    virtual void Render(REAL x,REAL y,REAL alpha=1,bool selected=0){

        tOutput disp;

        disp.SetTemplateParameter(1, vp +1);
        disp.SetTemplateParameter(2, s_newViewportBelongsToPlayer[vp]+1);
        disp.SetTemplateParameter(3, ePlayer::PlayerConfig(s_newViewportBelongsToPlayer[vp])->Name());
        disp << "$viewport_belongs_text";

        DisplayText(x-.02,y,disp,selected,alpha);
    }

};

#include "rSysdep.h"
extern void Render(int);


class ArmageTron_color_menuitem:public uMenuItemInt{
protected:
    int *rgb;
    unsigned short me;
public:
    // Colours can be set above the classic 0..15 with the COLOR_R/G/B_n console
    // commands, and opening this menu must not clamp what the player chose.
    ArmageTron_color_menuitem(uMenu *m,const char *tit,
                              const char *help, int *RGB,int Me)
            :uMenuItemInt(m,tit,help,RGB[Me],0,255,1,false),
    rgb(RGB),me(Me) {
        m->RequestSpaceBelow(.2);
    }

    ~ArmageTron_color_menuitem(){}

    virtual REAL SpaceRight(){return .2;}


    virtual void RenderBackground(){
        //    static int count=0;
        /*
        while(rgb[0]+rgb[1]+rgb[2]<13){
          if (rgb[count]<15)
        rgb[count]++;
          count++;
          if (count>2)
        count=0;
        }
        */
#ifndef DEDICATED
        uMenuItem::RenderBackground();
        if (!sr_glOut)
            return;
        REAL r = rgb[0]/15.0;
        REAL g = rgb[1]/15.0;
        REAL b = rgb[2]/15.0;
        se_MakeColorValid(r, g, b, 1.0f);
        RenderEnd();
        glColor3f(r, g, b);
        glRectf(.8,-.8,.98,-.98);
#endif
    }

};



void sg_PlayerMenu(int Player){
    tOutput name;
    name.SetTemplateParameter(1, Player+1);

    name << "$player_menu_text";


    uMenu playerMenu(name);

    uMenu camera_menu("$player_camera_text");
    uMenu chat_menu("$player_chat_text");
    //  name.Clear();
    chat_menu.SetCenter(-.5);

    ePlayer *p = ePlayer::PlayerConfig(Player);
    if (!p)
        return;

    std::vector< uMenuItemString * > instantChatSlots;
    std::vector< uMenuItemInput * > instantChatInputs;
    std::vector< uMenuItemDivider * > instantChatDividers;

    for ( int i = MAX_INSTANT_CHAT - 1 ; i >= 0; i-- )
    {
        instantChatSlots.push_back(
            new uMenuItemString(
                &chat_menu,
                tOutput( "$player_chat_chat" ),
                "$player_chat_chat_help",
                p->instantChatString[ i ],
                se_SpamMaxLen
            )
        );
        instantChatInputs.push_back(
            new uMenuItemInput( &chat_menu, p->se_instantChatAction[ i ], Player + 1 )
        );
        instantChatDividers.push_back( new uMenuItemDivider( &chat_menu ) );
    }

    uMenuItemToggle al
    (&playerMenu,"$player_autologin_text",
     "$player_autologin_help",
     p->autoLogin);

    uMenuItemString gid(&playerMenu,
                      "$player_global_id_text",
                      "$player_global_id_help",
                      p->globalID, 400);
    gid.SetColorMode( rTextField::COLOR_IGNORE );

    uMenuItemToggle st
    (&playerMenu,"$player_stealth_text",
     "$player_stealth_help",
     p->stealth);

    uMenuItemToggle sp
    (&playerMenu,"$player_spectator_text",
     "$player_spectator_help",
     p->spectate);

    uMenuItemToggle pnt
    (&playerMenu,"$player_name_team_text",
     "$player_name_team_help",
     p->nameTeamAfterMe);

    uMenuItemInt npt
    (&playerMenu,"$player_num_per_team_text",
     "$player_num_per_team_help",
     p->favoriteNumberOfPlayersPerTeam, 1, 16, 1);

    ArmageTron_color_menuitem B(&playerMenu,"$player_blue_text",
                                "$player_blue_help",
                                p->rgb,2);

    ArmageTron_color_menuitem G(&playerMenu,"$player_green_text",
                                "$player_green_help",
                                p->rgb,1);

    ArmageTron_color_menuitem R(&playerMenu,"$player_red_text",
                                "$player_red_help",
                                p->rgb,0);



    uMenuItemSubmenu chm(&playerMenu,&chat_menu,
                         "$player_chat_chat_help");

    uMenuItemSubmenu cm(&playerMenu,&camera_menu,
                        "$player_camera_help");

    uMenuItemFunctionInt icc(&playerMenu,"$player_camera_input_text",
                             "$player_camera_input_help",
                             &su_InputConfigCamera,Player);

    uMenuItemFunctionInt inc(&playerMenu,"$player_input_text",
                             "$player_input_help",
                             &su_InputConfig,Player);


    camera_menu.SetCenter(.3);

    uMenuItemToggle cam_glance
    (&camera_menu,
     "$camera_smart_glance_custom_text",
     "$camera_smart_glance_custom_help",
     p->smartCustomGlance);

    uMenuItemToggle cis(&camera_menu,
                        "$player_camera_autoin_text",
                        "$player_camera_autoin_help",
                        p->autoSwitchIncam);

    uMenuItemToggle cim(&camera_menu,
                        "$player_camera_wobble_text",
                        "$player_camera_wobble_help",
                        p->wobbleIncam);

    uMenuItemToggle cic(&camera_menu,
                        "$player_camera_center_int_text",
                        "$player_camera_center_int_help",
                        p->centerIncamOnTurn);

    uMenuItemToggle al_s
    (&camera_menu,
     "$player_camera_smartcam_text",
     "$player_camera_smartcam_help",
     p->allowCam[CAMERA_SMART]);

    uMenuItemToggle al_f
    (&camera_menu,
     "$player_camera_fixed_text",
     "$player_camera_fixed_help",
     p->allowCam[CAMERA_FOLLOW]);


    uMenuItemToggle al_fr
    (&camera_menu,
     "$player_camera_free_text",
     "$player_camera_free_help",
     p->allowCam[CAMERA_FREE]);

    uMenuItemToggle al_c
    (&camera_menu,
     "$player_camera_custom_text",
     "$player_camera_custom_help",
     p->allowCam[CAMERA_CUSTOM]);

    uMenuItemToggle al_sc
    (&camera_menu,
     "$player_camera_server_custom_text",
     "$player_camera_server_custom_help",
     p->allowCam[CAMERA_SERVER_CUSTOM]);

    uMenuItemToggle al_i
    (&camera_menu,
     "$player_camera_incam_text",
     "$player_camera_incam_help",
     p->allowCam[CAMERA_IN]);

    uMenuItemToggle al_m
    (&camera_menu,
     "$player_camera_mercam_text",
     "$player_camera_mercam_help",
     p->allowCam[CAMERA_MER]);

    uMenuItemInt cam_fov
    (&camera_menu,
     "$player_camera_fov_text",
     "$player_camera_fov_help",
     p->startFOV,30,160,5);

    uMenuItemSelection<eCamMode> cam_s
    (&camera_menu,
     "$player_camera_initial_text",
     "$player_camera_initial_help",
     p->startCamera);

    cam_s.NewChoice("$player_camera_initial_scust_text","$player_camera_initial_scust_help",CAMERA_SERVER_CUSTOM);
    cam_s.NewChoice("$player_camera_initial_cust_text","$player_camera_initial_cust_help",CAMERA_CUSTOM);
    cam_s.NewChoice("$player_camera_initial_int_text","$player_camera_initial_int_help",CAMERA_IN);
    cam_s.NewChoice("$player_camera_initial_smrt_text","$player_camera_initial_smrt_help",CAMERA_SMART);
    cam_s.NewChoice("$player_camera_initial_ext_text","$player_camera_initial_ext_help",CAMERA_FOLLOW);
    cam_s.NewChoice("$player_camera_initial_free_text","$player_camera_initial_free_help",CAMERA_FREE);
    cam_s.NewChoice("$player_camera_initial_mer_text","$player_camera_initial_mer_help",CAMERA_MER);

    uMenuItemString tn(&playerMenu,
                       "$player_teamname_text",
                       "$player_teamname_help",
                       p->teamname, 16);

    uMenuItemString n(&playerMenu,
                      "$player_name_text",
                      "$player_name_help",
                      p->name, ePlayerNetID::MAX_NAME_LENGTH);

    playerMenu.Enter();

    for ( std::vector< uMenuItemString * >::const_iterator it = instantChatSlots.begin(); it != instantChatSlots.end(); ++it )
        delete *it;
    for ( std::vector< uMenuItemInput * >::const_iterator it = instantChatInputs.begin(); it != instantChatInputs.end(); ++it )
        delete *it;
    for ( std::vector< uMenuItemDivider * >::const_iterator it = instantChatDividers.begin(); it != instantChatDividers.end(); ++it )
        delete *it;

    // request network synchronisation if the server can handle it
    static nVersionFeature inGameRenames( 5 );
    if ( inGameRenames.Supported() )
    {
        ePlayerNetID::Update();
        ePlayer::SendAuthNames();
    }

    /*
    for (i=MAX_PLAYERS-1; i>=0; i--)
    {
        if (ePlayer::PlayerIsInGame(i))
        {
            ePlayer* p = ePlayer::PlayerConfig(i);
            if (p->netPlayer)
                p->netPlayer->RequestSync();
        }
    }
    */
}


AA_VOIDFUNC viewport_menu_x;






// from gGame.C
//extern int pingCharity;


void sg_PlayerMenu(){
    uMenu Player_men("$player_mainmenu_text");


    uMenuItemFunction vp_selec(&Player_men,
                               "$viewport_assign_text",
                               "$viewport_assign_help",
                               &viewport_menu_x);

    ArmageTron_viewport_menuitem vp(&Player_men);
    uMenuItemFunctionInt  *names[MAX_PLAYERS];

    int i;

    for(i=MAX_PLAYERS-1;i>=0;i--){
        tOutput title;
        title.SetTemplateParameter(1, i+1);
        title << "$player_menu_text";

        tOutput help;
        help.SetTemplateParameter(1, i+1);
        help << "$player_menu_help";

        names[i]=new uMenuItemFunctionInt(&Player_men,
                                          title,
                                          help,
                                          sg_PlayerMenu,i);
    }


    Player_men.Enter();

    //  ePlayerNetID::Update();
    for(i=MAX_VIEWPORTS-1;i>=0;i--){
        delete names[i];
    }
}




void viewport_menu_x(void){
    uMenu sg_PlayerMenu("$viewport_assign_text");

    ArmageTronPlayer_to_viewport_menuitem *select[MAX_VIEWPORTS];

    int i;
    for(i=rViewportConfiguration::s_viewportConfigurations[rViewportConfiguration::next_conf_num]->num_viewports-1;i>=0;i--){
        select[i]=new ArmageTronPlayer_to_viewport_menuitem(&sg_PlayerMenu,i);
    }

    // ArmageTron_viewport_menuitem vp(&sg_PlayerMenu);

    sg_PlayerMenu.Enter();

    for(i=rViewportConfiguration::s_viewportConfigurations[rViewportConfiguration::next_conf_num]->num_viewports-1;i>=0;i--){
        delete select[i];
    }
}


static uActionGlobal con_input( "CONSOLE_INPUT" );


static uActionGlobal screenshot( "SCREENSHOT" );

static uActionGlobal togglefullscreen( "TOGGLE_FULLSCREEN" );

#ifndef DEDICATED
static const char* sg_defaultScreenshotName = "%Y-%m-%d_%H-%M-%S";
#endif

static bool screenshot_func(REAL x){
    if (x>0){
#ifndef DEDICATED
        time_t rawtime;
        char rawname[30];
        time( &rawtime );
        strftime(
            rawname, 29,
            sg_defaultScreenshotName, localtime( &rawtime )
            );
        sr_screenshotName = rawname;
        sr_screenshotIsPlanned=true;
#endif
    }

    return true;
}

static bool con_func(REAL x){
    if (x>0){
        sg_ConsoleInput();
    }

    return true;
}

static bool toggle_fullscreen_func( REAL x )
{
#ifndef DEDICATED
#ifdef DEBUG
    // don't toggle fullscreen while playing back in debug mode, that's annoying
    if ( tRecorder::IsPlayingBack() )
        return true;
#endif

    // only do anything if the application is active (work around odd bug)
#if SDL_VERSION_ATLEAST(2,0,0)
    Uint32 flags = SDL_GetWindowFlags(sr_screen);
    if ( x > 0 && (flags & SDL_WINDOW_SHOWN) && !(flags & SDL_WINDOW_MINIMIZED))
#else
    if ( x > 0 && ( SDL_GetAppState() & SDL_APPACTIVE ) )
#endif
    {
        currentScreensetting.fullscreen = !currentScreensetting.fullscreen;
        sr_ReinitDisplay();
    }
#endif

    return true;
}

static uActionGlobalFunc gaf_ss(&screenshot,&screenshot_func, true );
static uActionGlobalFunc gaf_md(&con_input,&con_func);
static uActionGlobalFunc gaf_tf(&togglefullscreen,&toggle_fullscreen_func, true );
