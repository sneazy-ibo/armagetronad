# src/doc/ Directory

## Summary
Documentation files and Doxygen configuration for Armagetron Advanced.

## Details

The doc directory contains documentation-related files for Armagetron Advanced. These include configuration files for Doxygen, HTML generation, and other documentation processing tools.

Key files include:
- `Doxyfile.in` - Template for Doxygen configuration file. This is processed by configure to generate the actual Doxyfile with proper paths and settings for API documentation generation.
- `HtmlMakefile` - Makefile for generating HTML documentation. This provides targets for building HTML-based documentation from various sources.
- `net/` - Network-related documentation (likely historical or reference material)
- `Content_Creation/` - Documentation about creating game content

The documentation system generates API reference from source code comments using Doxygen. The HTML documentation can be built as part of the development process and is useful for understanding the codebase structure and APIs.

## Directory Structure

```
.
├── Content_Creation/  # Documentation about creating game content
├── Doxyfile.in        # Doxygen configuration template
├── HtmlMakefile      # HTML documentation build Makefile
└── net/              # Network-related documentation
```

## Technologies

- **Documentation**: Doxygen
- **Build System**: Autotools integration
- **Output**: HTML, LaTeX, or other formats

## Coding Conventions

- **Doxygen Comments**: Standard Doxygen comment format (`///`, `/** */`, etc.)
- **Template Files**: .in extension for configure templates
- **Documentation Style**: Follows project documentation guidelines

## Key Patterns

- Documentation generation pattern
- Template processing pattern
- API reference documentation

## Build System

- `Doxyfile.in` processed by configure to create `Doxyfile`
- `HtmlMakefile` provides documentation build targets
- Documentation can be built with `make doxygen` or similar targets
- Installed to `${docdir}` (typically /usr/share/doc/armagetronad/)
