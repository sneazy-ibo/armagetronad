# resource/proto/ Directory

## Summary
Source resource files and templates for game assets.

## Details

The proto directory contains the source resource files for Armagetron Advanced. These are the raw asset files that are processed during the build and distributed with the game. The term "proto" likely stands for "prototype" or "source" - these are the original files from which the final distributed resources are created.

The directory is organized into subdirectories by author/contributor:
- `AATeam/` - Resources from the AATeam group
- `Anonymous/` - Resources from anonymous or unidentified contributors
  - `original/` - Original creations
  - `polygon/` - Polygon-based resources
    - `regular/` - Regular polygon resources
    - `shapes/` - Various shape resources
- `Luke-Jr/` - Resources from Luke-Jr contributor
  - `n-gon/` - N-sided polygon resources
- `Your_mom/` - Resources from Your_mom contributor
  - `inaktek/` - Inaktek subdirectory
  - `repeat/` - Repeating pattern resources
- `Z-Man/` - Resources from Z-Man (likely a key contributor)

Resource types in these directories may include:
- Map files (.xml)
- Texture images (.png, .jpg)
- Model definitions (.mod)
- Sound files (.wav, .ogg)
- Other asset types

The resources are processed by the build system, sorted for consistent ordering, and then installed to the appropriate distribution directories.

## Directory Structure

```
.
├── AATeam/           # AATeam resource collection
├── Anonymous/        # Anonymous contributor resources
│   ├── original/     # Original creations
│   └── polygon/      # Polygon-based resources
│       ├── regular/  # Regular polygons
│       └── shapes/    # Various shapes
├── Luke-Jr/          # Luke-Jr contributor resources
│   └── n-gon/        # N-sided polygon resources
├── Your_mom/         # Your_mom contributor resources
│   ├── inaktek/      # Inaktek subdirectory
│   └── repeat/       # Repeating patterns
└── Z-Man/           # Z-Man contributor resources
```

## Technologies

- **Resource Types**: Maps, textures, models, sounds
- **Formats**: XML, PNG, JPG, MOD, WAV, OGG, etc.
- **Processing**: Sorted by batch/make/sortresources.py

## Coding Conventions

- **Organization**: By author/contributor, then by type
- **Naming**: Descriptive names for resource files
- **Structure**: Hierarchical organization

## Key Patterns

- Resource organization by contributor
- Hierarchical resource structure
- Template/instance pattern

## Build System

- Resources found via `find ${top_srcdir}/resource/proto/ -type f`
- Sorted by `batch/make/sortresources.py`
- Excludes CVS directories and backup files
- Installed to `${datadir}/resource/included/`
- Processed during `make` in resource/Makefile.am
