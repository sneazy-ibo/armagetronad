# MacOS/ Directory

## Summary
Legacy macOS-specific project files and Xcode configuration.

## Details

The MacOS directory (note the mixed case) contains legacy macOS-specific files from earlier development periods. This appears to be older Xcode project files and related macOS build configurations that may not be actively used in the current build system.

Key contents include:
- `Armagetron Advanced.xcodeproj` - Legacy Xcode project file for the main game
- `Armagetron.pbproj` - Older Xcode project bundle file
- `build_tools/` - Tools and scripts for macOS builds

These files are likely historical artifacts from when the project used Xcode for macOS development. The current build system primarily uses Autotools with platform-specific code in `src/macosx/`. However, these legacy files may still be useful for developers using Xcode or for historical reference.

## Directory Structure

```
.
├── Armagetron Advanced.xcodeproj/  # Xcode project file
├── Armagetron.pbproj/              # Legacy Xcode project bundle
└── build_tools/                    # macOS build tools
```

## Technologies

- **Format**: Xcode project files
- **Platform**: macOS
- **Status**: Legacy/historical

## Coding Conventions

- **Xcode Files**: Standard Xcode project file format
- **Legacy**: May use older macOS development conventions

## Key Patterns

- Legacy project file pattern
- Historical artifact pattern
- Platform-specific build configuration

## Build System

- Files are in `EXTRA_DIST` - distributed but not built
- Not part of active Autotools build system
- May be used by developers for Xcode-based development
- Primarily for reference/historical purposes
