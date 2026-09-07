# src/test/ Directory

## Summary
Unit and integration tests for Armagetron Advanced components.

## Details

The test directory contains test programs and utilities for verifying the correctness of Armagetron Advanced components. Tests are primarily focused on verifying parsing logic, data structure integrity, and other critical functionality.

The current test suite includes a test for chat prefix parsing from XML, which verifies the game's ability to correctly extract and handle chat prefixes from server messages. This is important for proper display of player communications in the game's UI.

Tests are compiled as separate programs that link against the relevant project libraries. They can be run independently or as part of a test suite during development and CI/CD pipelines.

## Directory Structure

```
.
├── Makefile.am          # Test build configuration
├── chat_prefix_test.cpp # Chat prefix parsing tests
└── chat_prefix_test     # Compiled test binary (generated)
```

## Technologies

- **Language**: C++
- **Build System**: Autotools (integrated with main build)
- **Dependencies**: libxml2, project libraries (libtools, libnetwork, etc.)

## Coding Conventions

- **Test Style**: Custom test harness using main() function
- **Assertions**: Uses project's assertion macros (tASSERT, etc.)
- **Integration**: Tests link against actual project libraries

## Key Patterns

- Simple test harness pattern
- Direct library testing
- XML parsing verification

## Build System

- Tests built as separate executables
- Integrated with main Makefile.am via `check_PROGRAMS` or similar
- `chat_prefix_test_SOURCES = chat_prefix_test.cpp`
- Links against: libtools.a, libnetwork.a, librender.a, libui.a, libengine.a, libtron.a
- Uses libxml2 for XML parsing

## Automated Tests

- `chat_prefix_test`: Verifies chat prefix extraction from XML
  - Tests parsing of player chat messages
  - Validates prefix formatting
  - Ensures proper handling of special characters

## Notes from Humans
### GUARDRAIL: The AI Agents keep out of this section.

### Guidelines

- Code touched by AI agents must be covered by unit tests.
- Employ Test Driven Development whenever appropriate. As a reminder, that means the development loop is:
  - Write a test that you know will fail because the change you are instructed to do is missing.
  - Verify the test fails.
  - Make the minimal code change that makes the test succeed.
  - Verify all tests succeed.
  - If appropriate, refactor the code you touched to improve quality, then verify all tests succeed again.
- Changes to the tested code are discoraged when writing tests, but sometimes required. All changes should be reported to the user and be noted in commit messages.
- It is likely you will find classes that are not currently testable in isolation. Your choices then, before doing anything else:
  - Refactor them to make them testable. As this is a legacy branch, only do this if it is possible with minimal changes.
  - Test them integrated with the parts they cannot be removed from.
- If not in conflict with user preferences, do the adaptions that make code testable in separate commits.