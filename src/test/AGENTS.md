# src/test/ Directory

## Summary
Unit and integration tests for Armagetron Advanced components.

## Details

The test directory contains test programs for verifying the correctness of Armagetron Advanced components. Tests cover parsing logic, data structures, geometry, memory management, and other critical functionality.

Tests are compiled as separate programs that link against the relevant project libraries. The main test executable `unit_tests` combines most tests using the doctest framework, while `chat_prefix_test` is a legacy standalone test. Tests can be run independently or as part of the test suite during development and CI/CD pipelines.

## Directory Structure

```
.
├── unit_tests_main.cpp     # Main test harness entry point
├── chat_prefix_test.cpp    # Legacy chat prefix parsing tests
├── eAxis_test.cpp          # Axis class tests
├── eCoord_test.cpp         # Coordinate system tests
├── eRectangle_test.cpp     # Rectangle geometry tests
├── tArray_test.cpp         # Dynamic array tests
├── tCallbackString_test.cpp # String callback tests
├── tCallback_test.cpp      # Callback system tests
├── tColor_test.cpp         # Color handling tests
├── tException_test.cpp     # Exception system tests
├── tHeap_test.cpp          # Heap memory tests
├── tLinkedList_test.cpp    # Linked list tests
├── tList_test.cpp          # List container tests
├── tMemStack_test.cpp      # Memory stack tests
├── tRandomizer_test.cpp    # Randomization tests
├── tRing_test.cpp          # Ring buffer tests
└── tString_test.cpp        # String class tests
```

## Technologies

- **Language**: C++ (C++11 features)
- **Build System**: Autotools (integrated with main src/Makefile.am)
- **Test Framework**: doctest (primary), custom harness (legacy)
- **Dependencies**: project libraries (libtools, libnetwork, libengine, libui, librender, libtron)

## Coding Conventions

- **Test Style**: doctest framework for most tests, custom main() for legacy
- **Assertions**: doctest assertions and project's assertion macros (tASSERT, etc.)
- **Integration**: Tests link against actual project libraries
- **File Naming**: Test files follow `_test.cpp` naming convention

## Key Patterns

- Unit testing with doctest framework
- Direct library testing against production code
- Geometry and data structure verification
- Memory management testing
- Exception safety testing

## Build System

- Tests built as separate executables via src/Makefile.am
- Primary executable: `unit_tests` combining all doctest-based tests
- Legacy executable: `chat_prefix_test` for XML parsing
- `unit_tests_SOURCES`: Includes all test files with doctest
- `chat_prefix_test_SOURCES`: chat_prefix_test.cpp
- Links against: libtron.a, libengine.a, libnetwork.a, libui.a, librender.a, libtools.a

## Automated Tests

- `unit_tests`: Main test suite with doctest
  - Tests coordinate systems (eCoord, eAxis, eRectangle)
  - Tests data structures (tArray, tList, tLinkedList, tRing)
  - Tests memory management (tHeap, tMemStack)
  - Tests utilities (tString, tColor, tCallback, tRandomizer, tMath, tSysTime)
  - Tests exception handling
  - Tests command line parsing (tCommandLine)
  - Tests event queue system (tEventQueue)
  - Tests directory management (tDirectories)
  - Tests cryptographic utilities (MD5)
  - Tests engine components (eTimer, eGrid - limited due to dependencies)
  - Tests network components (nSocket - limited due to dependencies)
  - Tests cryptography (tCrypt - limited due to dependencies)
- `chat_prefix_test`: Legacy test for XML parsing
  - Tests parsing of player chat messages
  - Validates prefix formatting
  - Ensures proper handling of special characters

## Notes from Humans
### GUARDRAIL: The AI Agents keep out of this section.

### Guidelines

- Code touched by AI agents must be covered by automated tests.
- Employ Test Driven Development whenever appropriate.
- Changes to the tested code are discoraged when writing tests, but sometimes required. All changes should be reported to the user and be noted in commit messages.
- It is likely you will find classes that are not currently testable in isolation. Your choices then, before doing anything else:
  - Refactor them to make them testable. On legacy branches, only do this if it is possible with minimal changes.
  - Test them integrated with the parts they cannot be removed from.
- If not in conflict with user preferences, do the adaptions that make code testable in separate commits.
- **Test Framework**: Use doctest as the default test framework. Only write ad-hoc custom test programs when explicitly demanded by the user.
- Test files end in `_test.cpp`.
- If, during writing of tests, you find bugs or odd behavior, or cannot write a test because there is something blocking it, write them down in `TODO.md`.

### Remarks

- `chat_prefix_test` is just a relic, not actively used.