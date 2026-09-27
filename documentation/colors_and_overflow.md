# Colours, overflow, and the bike/trail preview

This document explains exactly how a saved player colour turns into the colours you
see in the game, why the *bike* and the *trail* can be two different colours, and
what "colour overflow" really does. It is written so you can predict the result of
any `(r, g, b)` you type, and it includes the technical details and the code
references at the end.

If you only read one paragraph, read this:

> A saved colour is three integers. The game stores each one in a **byte**, then
> divides it by 15 to get a float. The **bike** uses that float after it has been
> squeezed back through a byte, so above 15 it *wraps around*. The **trail** uses
> the same float but is *clamped* to 1 (full brightness). When a channel is above
> 15 the bike and the trail therefore disagree: the trail saturates to full while
> the bike keeps wrapping. That disagreement is the whole two-colour trick.

---

## 1. What a saved colour is

`Colors` in the modded settings are stored in `colors.txt` in the user config
directory, one per line:

```
name red green blue
```

Each channel is an integer. The menu lets you use **-255 … 255**, and values outside
that range are clamped when saving and loading. From the console you can set the
channels directly with `COLOR_R_1`, `COLOR_G_1` and `COLOR_B_1`, then keep them with
`SAVECOLOR <name>`.

The game itself represents a player colour as three **unsigned char** values
(`tShortColor` in `src/engine/ePlayer.h`). Assigning an `int` to an `unsigned char`
wraps it modulo 256, so the value the game actually keeps is:

```
u = n mod 256          (n = the number you saved)
```

Examples:

| saved `n` | kept `u` |
|---|---|
| 0 | 0 |
| 15 | 15 |
| 16 | 16 |
| 61 | 61 |
| 255 | 255 |
| -1 | 255 |
| -16 | 240 |
| -255 | 1 |

`-1` and `255` are **the same colour** to the game. This document always uses `u`
for the byte value the renderer sees.

---

## 2. The three colours in the preview

The preview at the bottom of the `Colors` menu shows three swatches:

| Swatch | What it is | How the game uses it |
|---|---|---|
| **bike** | the colour baked into the bike texture | the cycle body |
| **trail -** | the horizontal wall (drawn with a dash), full brightness | walls going east/west |
| **trail \|** | the vertical wall (drawn with a bar), 70 % brightness | walls going north/south |

"trail -" and "trail |" are the same paint under different lighting: the game
shades a wall by its direction, so a wall looks brighter when it runs one way than
the other. The two swatches are the two extremes of that shading; a diagonal wall
is somewhere in between.

---

## 3. The maths, step by step

### 3.1 Channel byte → float

The engine converts the byte to a float by dividing by 15:

```
c = u / 15.0
```

So the classic range `0 … 15` maps to `0.0 … 1.0`, and anything above 15 is
**above 1.0**. `u = 61` gives `c ≈ 4.067`; `u = 255` gives `c = 17.0`.

(If you are in a team, this value is blended with the team colour first — see
[§7.1](#71-team-blending).)

### 3.2 The bike: a wrap through a byte

Baking the bike texture (`gTextureCycle::ProcessImage`) takes the float, multiplies
by 255 and stores it in a byte:

```
bikeByte = int(c * 255) mod 256
```

Substituting `c = u / 15` and remembering `u / 15 * 255 = 17 * u`:

```
bikeByte = (17 * u) mod 256
```

and the preview shows it as `bikeByte / 255`.

Two consequences:

* For `u ≤ 15` the multiply does not wrap, so `bikeByte = 17u`: `u = 15` gives
  `255` (**pure white**), `u = 14` gives `238`, `u = 8` gives `136`, etc.
  In that range the bike and the horizontal trail are **exactly the same colour**,
  because `bikeByte / 255 = 17u / 255 = u / 15 = c`.
* For `u > 15` the result wraps modulo 256. `u = 16` gives `16` (almost black!),
  `u = 30` gives `254`, `u = 45` gives `253`, and so on.

Because 17 and 256 share no factor, **the wrap visits every byte value exactly
once** as `u` runs from 0 to 255. In other words, for any bike byte you want there
is exactly one channel value that produces it.

> **The overflow trick in one line:** a channel above 15 makes the bike *wrap* to
> an unrelated byte, while the trail (below) simply *saturates* to full. The bike
> and the trail stop agreeing.

### 3.3 The trail: saturation then directional shading

The trail is drawn per wall (`gNetPlayerWall::RenderList`). The colour is the same
float `c`, multiplied by a direction factor and then clamped by OpenGL:

```
intensity   = 0.7 + 0.3 * x² / (x² + y²)      // 0.7 … 1.0
wallChannel = min(c * intensity, 1.0)          // glColor clamps each component
```

* A wall along x (horizontal) has `intensity = 1.0`.
* A wall along y (vertical) has `intensity = 0.7`.
* A diagonal is in between.

So the two preview swatches are exactly:

```
trail horizontal = min(c,     1.0)
trail vertical   = min(c*0.7, 1.0)
```

Crucially, the clamping happens **after** the multiply. A channel with `c = 2`
(bike wraps!) has a horizontal trail of `min(2·1, 1) = 1` and a vertical trail of
`min(2·0.7, 1) = 1` — both saturated, even though the bike wrapped. Only a channel
with `0.7c < 1`, i.e. `u < 21.4`, keeps a visible difference between the two trail
shades.

### 3.4 Summary table

The full path for one channel, with `u` the stored byte:

| quantity | formula | range |
|---|---|---|
| float | `c = u / 15` | `0 … 17` |
| bike (preview) | `((17·u) mod 256) / 255` | `0 … 1` |
| trail horizontal | `min(c, 1)` | `0 … 1` |
| trail vertical | `min(0.7·c, 1)` | `0 … 1` |

### 3.5 Channel table

| u | bike byte | bike | trail - (horiz) | trail \| (vert) |
|---:|---:|---:|---:|---:|
| 0 | 0 | 0.000 | 0.000 | 0.000 |
| 8 | 136 | 0.533 | 0.533 | 0.373 |
| 12 | 204 | 0.800 | 0.800 | 0.560 |
| 14 | 238 | 0.933 | 0.933 | 0.653 |
| **15** | **255** | **1.000** | **1.000** | **0.700** |
| 16 | 16 | 0.063 | 1.000 | 0.747 |
| 17 | 33 | 0.129 | 1.000 | 0.793 |
| 18 | 50 | 0.196 | 1.000 | 0.840 |
| 20 | 84 | 0.329 | 1.000 | 0.933 |
| 21 | 101 | 0.396 | 1.000 | 0.980 |
| 22 | 118 | 0.463 | 1.000 | 1.000 |
| 30 | 254 | 0.996 | 1.000 | 1.000 |
| 45 | 253 | 0.992 | 1.000 | 1.000 |
| 60 | 252 | 0.988 | 1.000 | 1.000 |
| 240 | 240 | 0.941 | 1.000 | 1.000 |
| 255 (-1) | 239 | 0.937 | 1.000 | 1.000 |

Notice the pattern at multiples of 15: `u = 15k` gives `bikeByte = 256 - k`, i.e.
`255, 254, 253, …`, while the trail is fully saturated for all of them. That is why
values like `30`, `45`, `60` give a near-white bike but a solid white trail.

---

## 4. Overflow, multi-trail and "bicolor"

The three interesting regimes for a channel:

1. **`u < 15` — no overflow, both agree.**
   The bike rises with the channel, the trail rises with it, and the two trail
   shades differ (up to 30 %). Everything is "normal".

2. **`15 ≤ u < 21.4` — saturated trail, dimmed second shade.**
   Horizontal trail is full, vertical trail is between 0.7 and 1. The bike may
   already wrap (`u = 16` wraps to nearly black), so this is where the two-colour
   effect begins: a wrapped bike against a full-brightness trail.

3. **`u ≥ 21.4` — fully saturated.**
   Both trail shades are exactly 1; the trail is the brightest possible version of
   the colour. The bike byte keeps wrapping around the wheel, giving a different
   colour every ~15 channel steps.

**Multi-trail** (the two trail shades reading as different colours) is strongest in
regime 2, because the vertical shade is still below 1 while the horizontal one is
already at 1. The largest gap between the two shades is at `u = 15`: `1.0` versus
`0.7`, a 30 % difference. As `u` grows past 15 the gap shrinks and vanishes at
about `u = 21.4`.

**Bicolor** (bike different from trail) needs regime 2 or 3: the trail is saturated
while the bike wraps. A good example is a channel at `u = 61`: the bike wraps to
`13` (very dark) while the trail is full. Set the other channels to 15 and you get
a **black bike with a full-brightness trail**.

---

## 5. Worked examples

| entry `(r g b)` | stored `u` | bike | trail horizontal | trail vertical | look |
|---|---|---|---|---|---|
| `15 15 15` | 15 15 15 | 1.00, 1.00, 1.00 | 1.00, 1.00, 1.00 | 0.70, 0.70, 0.70 | white bike, white → gray trail |
| `15 15 0` | 15 15 0 | 1.00, 1.00, 0.00 | 1.00, 1.00, 0.00 | 0.70, 0.70, 0.00 | yellow bike, yellow → olive trail |
| `15 0 15` | 15 0 15 | 1.00, 0.00, 1.00 | 1.00, 0.00, 1.00 | 0.70, 0.00, 0.70 | magenta bike and trail |
| `0 15 15` | 0 15 15 | 0.00, 1.00, 1.00 | 0.00, 1.00, 1.00 | 0.00, 0.70, 0.70 | cyan bike and trail |
| `30 15 0` | 30 15 0 | 1.00, 1.00, 0.00 | 1.00, 1.00, 0.00 | 1.00, 0.70, 0.00 | bright wrapped bike, yellow → orange trail |
| `15 15 14` | 15 15 14 | 1.00, 1.00, 0.93 | 1.00, 1.00, 0.93 | 0.70, 0.70, 0.65 | near-white bike, palest yellow trail |
| `-1 -1 -1` | 255 255 255 | 0.94, 0.94, 0.94 | 1.00, 1.00, 1.00 | 1.00, 1.00, 1.00 | near-white bike, fully saturated trail |

Values from 16 up (and any negative value) wrap the bike, which is what produces
the "different bike and trail" look; the closer to 15 a channel is, the brighter
its trail.

---

## 6. The preview, exactly

`eColorPalette::PreviewColors()` implements §3.4 for all three channels and hands
the result to the menu, which draws the three swatches in
`sg_drawColorPreview()`.

The menu also draws a small yellow triangle and the word **overflow** above the
swatches whenever any channel is outside the normal range — above 15 **or**
negative. That is exactly the condition under which the bike wraps to a colour the
trail does not share. It is checked on the raw saved values:

```cpp
if ( r > 15 || g > 15 || b > 15 || r < 0 || g < 0 || b < 0 )
```

---

## 7. Edge cases and extra technical detail

### 7.1 Team blending

In a team game the colour is not used raw. `ePlayerNetID::Color()` blends your
colour with the team colour:

```
w = 5, r_w = 2, g_w = 1, b_w = 2
if team has more than one player: clamp each channel to 15 first
c_r = (r_w·r + w·teamR) / (15·(w + r_w))
c_g = (g_w·g + w·teamG) / (15·(w + g_w))
c_b = (b_w·b + w·teamB) / (15·(w + b_w))
```

The weights are deliberately uneven (the green weight is half the others), so
overflow channels are clamped in real teams and the result is mostly the team
colour. The preview does **not** model team blending: it always shows your raw
colour.

### 7.2 The floor-colour check (`se_MakeColorValid`)

Before a cycle is created, both the bike and trail colours are passed through
`se_MakeColorValid()` in `src/engine/eFloor.cpp`. If a colour is too close to the
arena floor colour — or simply too dark to see — it is nudged brighter in small
steps until it stands out (or turns white).

This is why a colour that previews as very dark can still be visible in the game.
The preview does not model it, and it depends on the arena (the floor is black
unless `eFloor::Floor` exists and `sr_floorDetail > 1`).

### 7.3 Negative channels

A negative saved value is just a byte value above 240 after the modulo. For
example `-1 → 255`, whose bike byte is `239` (bright) and whose trail is fully
saturated. So negatives are a second way to trigger overflow, and they tend to give
a **bright** wrapped bike rather than a dark one, because `240 … 255` map back to
`239, 238, …`.

### 7.4 The bike is also a texture

The bike swatch in the preview is the pure player colour. In the game the colour is
composited into the bike texture (`gTextureCycle::ProcessImage`): fully transparent
texture pixels become the player colour, but opaque pixels keep the texture's own
art. The stock bike texture has both — roughly a fifth of its pixels are fully
transparent (so they take your colour), and the rest is the drawn lightcycle
shading. The apparent bike colour is therefore *your colour plus the texture art*,
which is one reason a swatch and the bike on screen can look a little different.

### 7.5 Lighting, glow and texture mode

Two more things can tint the trail away from the preview:

* The first bit of a wall (right behind the cycle) is drawn with an added glow
  (`cfunc(rat)` in `gNetPlayerWall::RenderBegin`), and the very top line can be
  forced white when wall textures are disabled (`upperlinecolor`). This makes the
  leading "spike" of the trail lighter than the rest.
* With cycle objects set to *untextured* (`TEXTURE_MODE` for object/texture group
  `< 0`), the bike is drawn with the raw float and `glColor` clamps it, so any
  overflowing channel renders **white** instead of wrapping.

### 7.6 Floating point

The engine uses 32-bit `float` (`REAL` in `src/defs.h`). The formulas above assume
exact arithmetic; in practice `int(u / 15.0f * 255.0f)` matches `(17u) mod 256`
for every `u` in `0 … 255`, so the table is exact.

---

## 8. Finding a white bike with a colourful trail

Short answer: **not possible from the colour values alone**, and here is the proof.

The bike and the trail read the same byte `u`, but the bike *wraps* while the trail
*saturates*:

* For the trail to be a strong colour, at least one channel must be low, otherwise
  all three saturate and the trail is white.
* A low trail channel means `u < 15` for that channel.
* For a channel with `u < 15`, the bike byte is `17u ≤ 238` — the channel's bike
  value can never reach white (255) unless `u = 15`, which would saturate the
  trail.

In other words, a **pure white bike forces a white (or white/gray) trail**. The
bike byte reaches white only at `u = 15` (or near-white at multiples `15k`), and
all of those give a trail that is at (or near) full brightness.

So a white bike with a *different*, strongly coloured trail cannot come from the
channels. It can only come from the **texture art** (the bike is drawn with white
pixels in the texture, e.g. the stock lightcycle's white highlights, or a moviepack
bike whose body is white), with any strong colour supplying the trail.

The best compromises, if you want something close:

| Want | Try | Why |
|---|---|---|
| Pure white bike, brightest trail it can have | `15 15 15` | bike 1.00; trail white (H) → gray (V) |
| Near-white bike, maximum trail brightness | `30 30 30` or `240 240 240` | bike ≈ 0.99 / 0.94, trail fully saturated white |
| Bright bike, strong two-tone trail | `15 15 0`, `0 15 15`, `15 0 15` | bike bright, trail full + 70 % second shade |
| Dark/wrapped bike, saturated trail (bicolor) | `60 15 0` | bike ≈ black, trail yellow |
| Negatives for a bright wrapped bike | `-1 -1 -1`, `15 15 -1` | bike ≈ 0.94–1.0, trail fully saturated |

The trade-off is smooth: the whiter the bike, the paler the trail. The extremes are

| the coloured channel `u` | its bike = horizontal trail | colour contrast against the other two |
|---:|---:|---|
| 15 (or 30, 45, …) | 1.00 (white) | 0 % — every channel saturated, trail is white |
| 14 | 0.93 | ≈ 7 % — a faint tint |
| 12 | 0.80 | ≈ 20 % |
| 8 | 0.53 | ≈ 47 % |
| 5 | 0.33 | ≈ 67 % |
| 0 | 0.00 | 100 % — full colour, black bike |

A **negative** channel (`-1`, `-16`, …) is the closest you get to a white bike
with a tinted trail: the bike byte jumps to `239` (near-white) while that channel's
trail saturates to `1`. So all three channels negative or near `15` give a white
bike, and the ones that are `-1` add a pale tint to the vertical trail shade:

| entry | bike | trail |
|---|---|---|
| `15 15 -1` | near-white | white → pale **blue** (cool) |
| `15 -1 15` | near-white | white → pale **green** |
| `-1 15 15` | near-white | white → pale **red** |
| `-1 -1 15` | near-white | white → pale **yellow** |

Note these trails are all *pale* (the coloured channel is saturated), so you still
cannot get a white bike with a *strong* saturated hue. If you specifically want
that, the practical route is a bike texture with a white body (a moviepack) plus a
strong trail colour such as `15 15 0`, `0 15 15`, or `15 0 15`.

---

## 9. Commands and file format

Console commands (also usable from chat with `/`):

| command | effect |
|---|---|
| `COLORS` | list saved colours |
| `SAVECOLOR <name> [player]` | save your colour, or copy another player's, under `<name>` |
| `SETCOLOR <name>` | apply a saved colour |
| `DELCOLOR <name>` | remove a saved colour |
| `NEXTCOLOR` | apply the next saved colour, wrapping |

In the `Colors` menu:

| key | effect |
|---|---|
| `up` / `down` | move the highlight |
| `enter` | apply the highlighted colour |
| `delete` / `backspace` | remove the highlighted colour |
| `n` | create a new colour (starts from the highlighted one) |
| `e` | edit the highlighted colour's channels |
| `r` | rename the highlighted colour |

In the creator: `left`/`right` pick a channel, `up`/`down` change it, digits and `-`
edit it directly, `enter` saves, `esc` leaves.

The file is plain text; lines starting with `#` are comments:

```
# name red green blue
gold    15 15 0
cyan    0 15 15
violet  15 0 15
wrapped 60 15 0
cool    15 15 -1
```

---

## 10. Code map

| What | Where |
|---|---|
| palette storage, commands, `PreviewColors` | `src/engine/eColorPalette.cpp/.h` |
| player colour → float, team blend | `ePlayerNetID::Color` in `src/engine/ePlayer.cpp` |
| floor/visibility adjustment | `se_MakeColorValid` in `src/engine/eFloor.cpp` |
| cycle colour setup and live recolour | `gCycle` constructor and `gCycle::SetColor` in `src/tron/gCycle.cpp` |
| bike texture baking (`ProcessImage`) | `gTextureCycle` in `src/tron/gCycle.cpp` |
| trail shading and drawing | `gNetPlayerWall::RenderList` in `src/tron/gWall.cpp` |
| the menu, preview and overflow marker | `src/tron/gMenus.cpp` (search for `sg_drawColorPreview`) |

---

## 11. Glossary

* **channel** — one of red, green, blue, a number.
* **byte value `u`** — the saved channel after wrapping modulo 256.
* **float `c`** — `u / 15`, the value the engine uses.
* **overflow** — any channel above 15 (or negative), where the bike wraps.
* **bike** — the cycle body.
* **trail** — the wall the cycle leaves behind.
* **multi-trail** — the two trail shades (horizontal and vertical) reading as
  different brightnesses; strongest around `u = 15`.
* **bicolor** — bike and trail showing different colours, caused by overflow.
