# SentinelScan v0.1 — Project Architecture

## 1. Folder Structure

```
SentinelScan/
├── src/
│   ├── main.c          Entry point. Argument parsing, orchestration.
│   ├── scanner.c       Directory traversal. File metadata collection.
│   ├── entropy.c       Shannon entropy calculation (future).
│   └── report.c        Console output + CSV export (future).
├── include/
│   ├── scanner.h       FileInfo / FileList types, scanner API.
│   ├── entropy.h       entropy_calculate declaration (future).
│   └── report.h        report_console / report_csv declarations (future).
├── tests/              Unit and integration tests.
├── docs/               Architecture, design decisions, changelog.
├── reports/            Generated CSV output directory.
├── .gitignore
└── README.md
```

### Directory Responsibilities

| Directory | Purpose |
|-----------|---------|
| `src/`    | All .c implementation files. One .c per module. |
| `include/`| All public .h headers. Consumers of a module only see its header. |
| `tests/`  | Test source files. Mirrors src/ structure. |
| `docs/`   | Design documents, architecture decisions, developer guides. |
| `reports/`| Runtime output directory. Not committed. Listed in .gitignore. |

---

## 2. File Responsibilities

### src/main.c

**Role:** Process entry point.

- Parse command-line arguments (directory path).
- Initialize modules in dependency order (scanner → entropy → report).
- Drive the pipeline: scan → analyze → output.
- Handle fatal errors with clear messages and proper cleanup.
- Return 0 on success, 1 on failure.

### src/scanner.c

**Role:** Filesystem discovery.

- Walk a directory tree recursively using Win32 API (`FindFirstFile` / `FindNextFile`).
- Filter files by extension (.exe, .dll, .msi).
- Populate a `FILE_LIST` with name, path, extension, and size for each match.
- Grow the file list dynamically using geometric doubling.

### src/entropy.c *(placeholder for v0.2)*

**Role:** Static analysis engine.

- Read binary file contents.
- Compute Shannon entropy from byte frequency histogram.
- Return a double in [0.0, 8.0] or -1.0 on failure.

### src/report.c *(placeholder for v0.2)*

**Role:** Output formatting.

- Print formatted results to stdout (console report).
- Write results to CSV file (report.csv).
- Classify entropy scores into Normal / Moderate / Suspicious.

---

## 3. Coding Standards

### 3.1 Compiler and Language

- **Compiler:** MSVC (`cl.exe`) via Developer Command Prompt for Visual Studio.
- **Standard:** C11 (MSVC has incomplete C99/C11 support — avoid VLAs, complex, restrict).
- **Character set:** Narrow (ANSI / UTF-8). Wide-char API calls are suffixed with `W`.
- **Warnings:** `/W4` with selected treat-as-error via `/we`.

### 3.2 Include Guard Style

Use `#pragma once` (supported by MSVC, GCC, Clang). Avoid traditional `#ifndef` guards.

```c
#pragma once
```

### 3.3 Comment Style

- No inline comments in source code.
- Public API functions in headers may include a brief purpose statement above the declaration.
- Implementation details are expressed through clear naming, not comments.

### 3.4 Error Handling

- Functions that can fail return `INT`: 0 for success, -1 for failure.
- NULL pointer checks on all public API entry points.
- Heap allocation failures return -1 to the caller. Callers propagate or handle.
- Never `assert` in release code. Never silently swallow unexpected errors.

### 3.5 Memory Management

- One allocator per module: `ScannerInit` / `ScannerFree`.
- `free` must match `malloc` or `realloc` exactly — no mismatched allocators.
- After `free`, set pointer to NULL.
- Use `realloc` through a temporary variable to avoid leaks on failure.

### 3.6 Function Design

- One function, one responsibility.
- Maximum 80 lines per function. If a function exceeds 80 lines, refactor.
- Static helper functions for internal logic (file-scoped with `static`).
- No function-like macros. Use inline functions or constants.

### 3.7 Return Value Conventions

| Context | Convention |
|---------|-----------|
| Success / failure | 0 = success, -1 = failure |
| Boolean predicates | 0 = false, non-zero = true (MSVC BOOLEAN style) |
| Output via pointer | Caller provides buffer, callee fills |
| Multi-value returns | Output parameters via pointer, not struct returns |

---

## 4. Naming Conventions

### 4.1 Types

PascalCase, prefixed to avoid collisions.

```c
FILE_INFO           struct
FILE_LIST           struct
PFILE_INFO          pointer to FILE_INFO
PFILE_LIST          pointer to FILE_LIST
```

### 4.2 Functions

PascalCase, prefixed by module name.

```c
ScannerInit()
ScannerFree()
ScannerScan()
```

Entry point uses `main` (C standard).

### 4.3 Macros and Constants

UPPER_CASE with underscores.

```c
#define MAX_EXTENSION 8
#define INITIAL_CAPACITY 1024
```

### 4.4 Variables

Lowercase, underscores for readability.

```c
INT    fileCount;
PCSTR  searchDirectory;
```

### 4.5 Parameters

Descriptive, avoiding abbreviations where reasonable.

```c
_In_ PCSTR  RootPath
_Inout_ PFILE_LIST  List
```

### 4.6 Prefix Summary

| Category | Prefix/Style | Example |
|----------|-------------|---------|
| Module prefix | PascalCase, noun + verb | `ScannerScan` |
| Static helper | PascalCase | `GrowList` |
| Type | PascalCase, underscores | `FILE_INFO` |
| Pointer typedef | `P` prefix | `PFILE_INFO` |
| Macro | UPPER_CASE | `MAX_EXTENSION` |
| Variable | snake_case | `currentIndex` |

---

## 5. Internal Dependency Graph

```
main.c
  │
  ├──► scanner.h ──► scanner.c
  │
  ├──► entropy.h ──► entropy.c   (future)
  │
  └──► report.h  ──► report.c    (future)
```

- Headers never include other project headers unless they consume a type.
- scanner.h is standalone (depends only on `Windows.h`).
- entropy.h depends on nothing.
- report.h includes scanner.h only if it consumes FILE_LIST in its interface.

---

## 6. Build System

### MSVC Build (Developer Command Prompt)

```
cl /nologo /W4 /Iinclude src\*.c /Fe:sentinel.exe
```

### GNU Make (MinGW)

```
CC = cl
CFLAGS = /nologo /W4 /Iinclude
TARGET = sentinel.exe

$(TARGET): src/main.c src/scanner.c
    $(CC) $(CFLAGS) $** /Fe:$@

clean:
    del /Q $(TARGET) 2>nul

.PHONY: clean
```

---

## 7. Testing Strategy

- Test source files live in `tests/` and mirror `src/` naming.
- Each test is a standalone .c that links against the module under test.
- Tests are NOT part of the main build — they have a separate build target.
- Output is plaintext: PASS/FAIL to stdout. No test framework dependency for v0.1.

```
tests/
├── test_scanner.c    exercises scanner module
├── test_entropy.c    exercises entropy module  (future)
└── test_report.c     exercises report module    (future)
```
