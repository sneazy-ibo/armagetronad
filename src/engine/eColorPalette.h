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

#ifndef ArmageTron_COLORPALETTE_H
#define ArmageTron_COLORPALETTE_H

#include "tString.h"

#include <vector>

//! A named collection of player colours. A colour is three channels that a
//! player carries; the game keeps each in a byte, so 0..255 is the interesting
//! range and values above 15 (or negative) make the bike and the two trail shades
//! differ (see PreviewColors). Entries are kept in the full -255..255 range.
namespace eColorPalette
{
    //! one saved colour
    struct Entry
    {
        tString name;
        int r, g, b;    //!< -255..255, as the COLOR_R/G/B settings accept
    };

    //! all saved entries, in the order they were added
    typedef std::vector<Entry> Entries;
    Entries const & All();

    //! the colour currently set for the local player
    void GetCurrent( int & r, int & g, int & b );

    //! The colours a saved entry shows on the track, each channel 0..1.
    //!
    //! Each channel is stored in an unsigned char, so anything outside 0..255
    //! wraps (a negative channel, in particular, becomes its byte value). The
    //! bike (body*) is that byte baked into the bike texture and wraps again
    //! through the byte range.
    //!
    //! The trail has no single colour: a wall is shaded by its direction, from
    //! full brightness (horizontal) to 0.7 (vertical), see gNetPlayerWall::Render
    //! (intensity = .7 + .3 * xs/denom). So trailHorizontal* and trailVertical*
    //! are the two shades the same colour produces.
    void PreviewColors( int r, int g, int b,
                        float & bodyR, float & bodyG, float & bodyB,
                        float & trailHorizontalR, float & trailHorizontalG, float & trailHorizontalB,
                        float & trailVerticalR, float & trailVerticalG, float & trailVerticalB );

    //! applies a saved colour to the local player. false if the name is unknown
    bool Apply( tString const & name );

    //! saves a colour under a name, replacing an entry with the same name
    void Save( tString const & name, int r, int g, int b );

    //! removes a saved colour. false if the name is unknown
    bool Remove( tString const & name );

    //! looks an entry up by name, or NULL
    Entry const * Find( tString const & name );

    //! applies the next saved colour after the one currently in use, wrapping.
    //! Returns the name applied, or an empty string when none are saved.
    tString Next();

    //! reads the palette from the config directory
    void Load();
    //! writes the palette to the config directory
    void SaveToDisk();

    //! called right after a colour is applied, with the saved -255..255 channels.
    //! Lets the game recolour a live cycle at once instead of waiting for the
    //! next round to recreate it.
    typedef void (*ApplyCallback)( int r, int g, int b );
    void SetApplyCallback( ApplyCallback callback );

    //! registers the console commands. Called once from a static initialiser.
    void RegisterCommands();
}

#endif
