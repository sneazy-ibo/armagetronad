# desktop/icons/ Directory

## Summary
Icon files for desktop integration of Armagetron Advanced.

## Details

The icons directory contains application icons in various sizes for desktop integration. These icons are used for:
- Application launchers in desktop menus
- Window decorations
- Taskbar/dock icons
- Application browser representations

The directory is organized by icon size, with subdirectories for each standard icon size:
- `16x16/` - 16x16 pixel icons (small, for menu items)
- `32x32/` - 32x32 pixel icons (standard small)
- `48x48/` - 48x48 pixel icons (medium)
- `64x64/` - 64x64 pixel icons (large)
- `128x128/` - 128x128 pixel icons (extra large, for high-DPI displays)

Each subdirectory contains icon files in appropriate formats (typically PNG) for that size. Icons are installed as part of the desktop integration process and are used by the .desktop file to provide visual representation of the game in desktop environments.

## Directory Structure

```
.
├── 16x16/    # 16x16 pixel icons
├── 32x32/    # 32x32 pixel icons
├── 48x48/    # 48x48 pixel icons
├── 64x64/    # 64x64 pixel icons
└── 128x128/  # 128x128 pixel icons
```

## Technologies

- **Formats**: PNG (Portable Network Graphics)
- **Sizes**: Multiple standard sizes for different display contexts
- **Integration**: freedesktop.org icon theme specification

## Coding Conventions

- **File Naming**: Consistent naming across all sizes
- **Formats**: PNG for transparency support
- **Sizes**: Power-of-two dimensions where possible

## Key Patterns

- Multi-size icon pattern
- Desktop integration pattern
- High-DPI support pattern

## Build System

- Icons installed to appropriate desktop icon directories
- Typically `${datadir}/icons/` or `/usr/share/icons/`
- Referenced by `armagetronad.desktop` file
- Multiple sizes for different display contexts
