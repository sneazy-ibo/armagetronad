# src/tools/ Directory

## Summary
Core utility library providing foundational data structures, configuration management, error handling, and cross-cutting concerns for the entire codebase. Built as `libtools.a`.

## Details

This directory contains the lowest-level infrastructure used throughout Armagetron Advanced. The tools library has no external dependencies (except standard library and thirdparty/binreloc) and forms the base layer of the dependency hierarchy.

Key subsystems include: custom container classes (`tArray`, `tLinkedList`, `tRing`, `tHeap`, `tMemStack`) that predate or replace STL containers; a powerful configuration system (`tConfiguration`, `tConfItemBase`) supporting runtime configuration via text files with access level controls and CASACL (Configuration Access Control List); error handling with debug levels and channels (`tError.h`); string utilities (`tString` with formatting, parsing, color coding); time management (`tSysTime`); resource management (`tResourceManager`); recording/playback system (`tRecorder`) for game demos; command line parsing (`tCommandLine`); threading abstraction (`tZThread`, `pthread-binding.h`); cryptographic utilities (`tCrypt`); memory management (`tMemManager`) with custom allocators; and miscellaneous utilities like callbacks (`tCallback`), colors (`tColor`), locales (`tLocale`), events (`tEventQueue`), and random number generation (`tRandom`).

The library uses a consistent naming convention with `t` prefix for template/type classes and `st_` prefix for static/global functions and variables. Header guards use the pattern `ArmageTron_<NAME>_H`. The code heavily uses macros like `tASSERT`, `tERR_ERROR`, `tERR_WARN` for debugging. Reference counting is implemented via `tReferencable` base class and smart pointer variants (`tSafePTR`, `tJUST_CONTROLLED_PTR`, `tCONTROLLED_PTR`).

Build produces `libtools.a` static library. Includes `defs.h` for global definitions and `config.h` for autoconf-generated configuration.

## Directory Structure

```
.
├── tArray.cpp              # Dynamic array implementation
├── tArray.h               # Dynamic array header
├── tCallback.cpp          # Callback system
├── tCallback.h           # Callback system header
├── tCallbackString.h      # String callback utilities
├── tColor.cpp             # Color management
├── tColor.h              # Color management header
├── tCommandLine.cpp       # Command line parsing
├── tCommandLine.h        # Command line parsing header
├── tConfiguration.cpp     # Configuration system
├── tConfiguration.h      # Configuration system header
├── tConsole.cpp           # Console system
├── tConsole.h            # Console system header
├── tCrypt.cpp             # Cryptographic utilities
├── tCrypt.h              # Cryptographic utilities header
├── tDirectories.cpp       # Directory management
├── tDirectories.h        # Directory management header
├── tError.cpp             # Error handling
├── tError.h              # Error handling header
├── tEventQueue.cpp        # Event queue
├── tEventQueue.h         # Event queue header
├── tException.cpp         # Exception handling
├── tException.h          # Exception handling header
├── tHeap.cpp              # Memory heap
├── tHeap.h               # Memory heap header
├── tInitExit.h            # Initialization/exit handling
├── tLinkedList.cpp        # Linked list implementation
├── tLinkedList.h         # Linked list header
├── tList.h                # List template
├── tLocale.cpp            # Localization
├── tLocale.h             # Localization header
├── tMath.h                # Math utilities
├── tMemManager.cpp        # Memory manager
├── tMemManager.h         # Memory manager header
├── tMemStack.cpp          # Memory stack
├── tMemStack.h           # Memory stack header
├── tRandom.cpp            # Random number generation
├── tRandom.h             # Random number generation header
├── tRecorder.cpp          # Game recording system
├── tRecorder.h           # Game recording system header
├── tRecorderInternal.cpp  # Internal recording utilities
├── tRecorderInternal.h   # Internal recording utilities header
├── tReferenceHolder.h     # Reference counting base
├── tRing.cpp              # Ring buffer
├── tRing.h               # Ring buffer header
├── tSafePTR.cpp           # Safe pointer implementation
├── tSafePTR.h            # Safe pointer header
├── tString.cpp            # String class
├── tString.h             # String class header
├── tSysTime.cpp           # System time
├── tSysTime.h            # System time header
├── tToDo.cpp              # TODO task management
├── tToDo.h               # TODO task management header
├── tZThread.h             # Thread support
├── pthread-binding.h      # POSIX thread binding
└── defs.h                 # Global definitions
```

## Technologies

- **Language**: C++ (C++98 standard)
- **Build System**: Autotools (automake)
- **Threading**: POSIX threads (pthread) via `pthread-binding.h` abstraction
- **Dependencies**: Standard C++ library, thirdparty/binreloc

## Coding Conventions

- **Naming**: `t` prefix for custom classes, `st_` prefix for static/utility functions, `s_` prefix for static variables
- **Smart Pointers**: `tJUST_CONTROLLED_PTR`, `tCONTROLLED_PTR`, `tSafePTR` for reference counting
- **Error Handling**: Debug levels (high, normal, low, very_low) and channels (flow, dump)
- **Configuration**: Hierarchical config system with access control and network sync support
- **Memory**: Custom allocators, memory pools, stack-based allocation (`tMemStack`)
- **Header Guards**: `#ifndef ArmageTron_<NAME>_H` pattern
- **Assertions**: `tASSERT`, `tERR_ERROR_INT`, `tERR_WARN` macros

## Key Patterns

- Singleton pattern for global managers
- Factory pattern for object creation
- Observer pattern via callbacks
- Reference counting for automatic memory management
- Template metaprogramming for generic containers
- RAII for resource management

## Build System

- Compiled as `libtools.a` static library
- Includes `-iquote @srcdir@/thirdparty/binreloc` for binreloc headers
- `tResourceManager.cpp` only included when `BUILDMAIN` is defined
- Test: `chat_prefix_test.cpp` in `src/test/` uses libtools
