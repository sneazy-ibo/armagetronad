/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2026 The Armagetron Advanced Development Team

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

#include "eColorPalette.h"

#include "ePlayer.h"
#include "rConsole.h"
#include "tConfiguration.h"
#include "tDirectories.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace eColorPalette
{
    //! file name in the user config directory
    static char const * const sr_fileName = "colors.txt";

    static Entries     sr_entries;
    static bool        sr_loaded = false;

    //! set by the game so an applied colour reaches a live cycle at once
    static ApplyCallback sr_applyCallback = NULL;

    void SetApplyCallback( ApplyCallback callback )
    {
        sr_applyCallback = callback;
    }

    //! index of the entry last applied, so Next() can continue from it. -1 means
    //! "nothing applied yet", so the first cycle starts at the beginning.
    static int         sr_lastApplied = -1;

    Entries const & All()
    {
        Load();
        return sr_entries;
    }

    Entry const * Find( tString const & name )
    {
        Load();
        for ( Entries::const_iterator i = sr_entries.begin(); i != sr_entries.end(); ++i )
            if ( i->name == name )
                return &( *i );
        return NULL;
    }

    //! clamp a channel to the range the game accepts; used by Load and Save
    static int ClampChannel( int value )
    {
        return std::max( -255, std::min( 255, value ) );
    }

    void Load()
    {
        if ( sr_loaded )
            return;
        sr_loaded = true;

        std::ifstream in;
        if ( !tDirectories::Config().Open( in, sr_fileName ) )
            return;

        std::string line;
        while ( std::getline( in, line ) )
        {
            // "name red green blue", one per line; '#' starts a comment
            std::string::size_type hash = line.find( '#' );
            if ( hash != std::string::npos )
                line.erase( hash );

            std::istringstream ls( line );
            std::string name;
            int r, g, b;
            if ( ls >> name >> r >> g >> b )
            {
                Entry e;
                e.name = name.c_str();
                // keep the full range the game accepts: colours can overflow
                // (bike and trail differ) and go negative (bike wraps, trail
                // clamps), and this menu exists to keep exactly those
                e.r = ClampChannel( r );
                e.g = ClampChannel( g );
                e.b = ClampChannel( b );
                sr_entries.push_back( e );
            }
        }
    }

    void SaveToDisk()
    {
        std::ofstream out;
        if ( !tDirectories::Config().Open( out, sr_fileName ) )
        {
            con << "Could not write " << sr_fileName << " in the config directory\n";
            return;
        }

        out << "# Named player colours, one per line: name red green blue (each -255..255;\n"
               "# outside 0..15 the bike wraps and the trail saturates, giving two colours)\n";
        for ( Entries::const_iterator i = sr_entries.begin(); i != sr_entries.end(); ++i )
            out << i->name << ' ' << i->r << ' ' << i->g << ' ' << i->b << '\n';
    }

    void GetCurrent( int & r, int & g, int & b )
    {
        r = g = b = 0;

#ifndef DEDICATED
        if ( ePlayer * me = ePlayer::PlayerConfig( 0 ) )
        {
            r = me->rgb[0];
            g = me->rgb[1];
            b = me->rgb[2];
        }
#endif
    }

    void PreviewColors( int r, int g, int b,
                        float & bodyR, float & bodyG, float & bodyB,
                        float & trailHorizontalR, float & trailHorizontalG, float & trailHorizontalB,
                        float & trailVerticalR, float & trailVerticalG, float & trailVerticalB )
    {
        // A channel is stored in an unsigned char (tShortColor, as ePlayerNetID
        // carries it), so a negative value wraps: -1 is 255. ePlayerNetID::Color
        // then divides by 15.0 and the result is used two ways:
        //
        //   bike:  baked into the bike texture as a GLubyte, i.e. narrowed mod 256
        //          (gTextureCycle::ProcessImage: GLubyte R = int(color_.r_ * 255))
        //   trail: multiplied by a direction shading factor (see gWall.cpp), then
        //          handed to glColor, which clamps each component to 1.0
        //
        // The trail therefore shows as two shades of the same colour depending on
        // whether the wall runs horizontally or vertically.
        int const channel[3] = { r, g, b };
        float body[3];
        float trail[3];

        for ( int i = 0; i < 3; ++i )
        {
            // the byte the game keeps, so negatives wrap the same way
            unsigned int const byteValue = (unsigned int)( channel[i] & 0xFF );
            float const c = byteValue / 15.0f;

            int const byte = int( c * 255.0f ) & 0xFF;   // the wrap
            body[i] = byte / 255.0f;

            trail[i] = c;
        }

        // Horizontal walls are drawn at full brightness, vertical ones dimmed;
        // this ratio comes from gNetPlayerWall::Render (intensity = .7 + .3*xs/denom).
        // The game shades first and lets glColor clamp afterwards, so an
        // overflowing channel stays at full brightness instead of being dimmed.
        float const vi = 0.7f;

        bodyR = body[0]; bodyG = body[1]; bodyB = body[2];

        trailHorizontalR = std::min( trail[0], 1.0f );
        trailHorizontalG = std::min( trail[1], 1.0f );
        trailHorizontalB = std::min( trail[2], 1.0f );
        trailVerticalR   = std::min( trail[0] * vi, 1.0f );
        trailVerticalG   = std::min( trail[1] * vi, 1.0f );
        trailVerticalB   = std::min( trail[2] * vi, 1.0f );
    }

    bool Apply( tString const & name )
    {
        Entry const * e = Find( name );
        if ( !e )
        {
            con << "No saved colour called \"" << name << "\"\n";
            return false;
        }

#ifndef DEDICATED
        ePlayer * me = ePlayer::PlayerConfig( 0 );
        if ( !me )
        {
            con << "No local player to colour\n";
            return false;
        }

        // Write the player's own rgb[] directly, the same way the player setup
        // menu changes colours. ePlayerNetID::Update copies rgb[] into the synced
        // colour every frame, so this takes effect and reaches the server without
        // going through the config commands (which throw when run from a menu).
        me->rgb[0] = e->r;
        me->rgb[1] = e->g;
        me->rgb[2] = e->b;

        // let the game push the colour onto a live cycle now
        if ( sr_applyCallback )
            sr_applyCallback( e->r, e->g, e->b );

        // remember the index so cycling continues from here (Find already loaded)
        for ( size_t i = 0; i < sr_entries.size(); ++i )
            if ( sr_entries[i].name == name )
            {
                sr_lastApplied = (int)i;
                break;
            }
#endif

        con << "Colour set to " << name << "\n";
        return true;
    }

    void Save( tString const & name, int r, int g, int b )
    {
        Load();
        // no clamp to 15 here: an overflowing (or negative) colour is a
        // legitimate choice and the bike/trail split depends on it being kept
        r = ClampChannel( r );
        g = ClampChannel( g );
        b = ClampChannel( b );

        bool replaced = false;
        for ( Entries::iterator i = sr_entries.begin(); i != sr_entries.end(); ++i )
        {
            if ( i->name == name )
            {
                i->r = r; i->g = g; i->b = b;
                replaced = true;
                break;
            }
        }
        if ( !replaced )
        {
            Entry e;
            e.name = name;
            e.r = r; e.g = g; e.b = b;
            sr_entries.push_back( e );
        }

        SaveToDisk();
        con << ( replaced ? "Updated" : "Saved" ) << " colour " << name
            << " (" << r << ' ' << g << ' ' << b << ")\n";
    }

    bool Remove( tString const & name )
    {
        Load();
        for ( Entries::iterator i = sr_entries.begin(); i != sr_entries.end(); ++i )
        {
            if ( i->name == name )
            {
                sr_entries.erase( i );
                sr_lastApplied = -1;
                SaveToDisk();
                con << "Removed colour " << name << "\n";
                return true;
            }
        }
        con << "No saved colour called \"" << name << "\"\n";
        return false;
    }

    tString Next()
    {
        Load();
        if ( sr_entries.empty() )
        {
            con << "No colours saved yet. Use /savecolor to add some.\n";
            return tString();
        }

        // continue after the last applied one, skipping if it was deleted
        int next = sr_lastApplied + 1;
        if ( next < 0 || next >= (int)sr_entries.size() )
            next = 0;
        sr_lastApplied = next;

        tString const name = sr_entries[next].name;
        Apply( name );
        return name;
    }

    // ------------------------------------------------------------------ commands

    //! /savecolor <name> [player]: save a colour under a name. With a player
    //! name, copies theirs; without one, saves the colour you have now.
    static void SaveColorConf( std::istream & s )
    {
        tString name;
        s >> name;
        if ( name.Len() <= 1 )
        {
            con << "Usage: SAVECOLOR <name> [player]\n"
                << "  with a player, copies their colour; without one, saves yours\n";
            return;
        }

        int r, g, b;

        tString player;
        s >> player;
        if ( player.Len() > 1 )
        {
            ePlayerNetID * target = ePlayerNetID::FindPlayerByName( player );
            if ( !target )
            {
                con << "Could not find a player called \"" << player << "\"\n";
                return;
            }
            r = target->color.r_;
            g = target->color.g_;
            b = target->color.b_;
        }
        else
        {
            GetCurrent( r, g, b );
        }

        Save( name, r, g, b );
    }

    //! /colors: list what is saved
    static void ColorsConf( std::istream & )
    {
        Load();
        if ( sr_entries.empty() )
        {
            con << "No colours saved yet. Use /savecolor <name> [player].\n";
            return;
        }

        con << sr_entries.size() << " saved colour(s):\n";
        for ( Entries::const_iterator i = sr_entries.begin(); i != sr_entries.end(); ++i )
            con << "  " << i->name << "  (" << i->r << ' ' << i->g << ' ' << i->b << ")\n";
    }

    //! /setcolor <name>
    static void SetColorConf( std::istream & s )
    {
        tString name;
        s >> name;
        if ( name.Len() <= 1 )
        {
            con << "Usage: SETCOLOR <name>\n";
            return;
        }
        Apply( name );
    }

    //! /delcolor <name>
    static void DelColorConf( std::istream & s )
    {
        tString name;
        s >> name;
        if ( name.Len() <= 1 )
        {
            con << "Usage: DELCOLOR <name>\n";
            return;
        }
        Remove( name );
    }

    //! /nextcolor: apply the next saved colour, for key cycling
    static void NextColorConf( std::istream & )
    {
        Next();
    }

    void RegisterCommands()
    {
        static tConfItemFunc saveColor( "SAVECOLOR", &SaveColorConf );
        static tConfItemFunc colors( "COLORS", &ColorsConf );
        static tConfItemFunc setColor( "SETCOLOR", &SetColorConf );
        static tConfItemFunc delColor( "DELCOLOR", &DelColorConf );
        static tConfItemFunc nextColor( "NEXTCOLOR", &NextColorConf );

        // normal players may use these from the console or chat
        static tAccessLevelSetter saveLevel( saveColor, tAccessLevel_Default );
        static tAccessLevelSetter colorsLevel( colors, tAccessLevel_Default );
        static tAccessLevelSetter setLevel( setColor, tAccessLevel_Default );
        static tAccessLevelSetter delLevel( delColor, tAccessLevel_Default );
        static tAccessLevelSetter nextLevel( nextColor, tAccessLevel_Default );
    }
}
