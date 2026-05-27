# SentinelScan v0.1 — Technical Architecture

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Data Flow](#2-data-flow)
3. [Module Structure](#3-module-structure)
4. [Function-by-Function Breakdown](#4-function-by-function-breakdown)
5. [Memory Model](#5-memory-model)
6. [API & Systems Internals](#6-api--systems-internals)
7. [Security Relevance](#7-security-relevance)
8. [Risks & Edge Cases](#8-risks--edge-cases)
9. [Potential Optimizations](#9-potential-optimizations)
10. [What I Learned](#10-what-i-learned)

---

## 1. Project Overview

SentinelScan is a Windows command-line tool written in C that scans directories for executable files (.exe, .dll, .msi), calculates their Shannon entropy as a static analysis heuristic, and outputs a formatted report.

### The Problem

When you download a program from the internet, how do you know if it's legitimate or malicious? One signal is **entropy** — a measure of randomness. Packed or encrypted malware tends to have very high entropy because compressed/encrypted data looks nearly random. Legitimate compiled code has more structure and therefore lower entropy.

This tool automates the measurement of that signal across hundreds or thousands of files.

### How a First-Year CS Student Should Think About This

| Concept | Everyday Analogy |
|---------|-----------------|
| Directory scanning | Manually opening every folder and drawer in a filing cabinet |
| File metadata | Reading the label on each file folder (name, size, type) |
| Entropy | How unpredictable the contents are — like a shuffled deck vs sorted deck |
| CSV report | A spreadsheet summarizing everything you found |

---

## 2. Data Flow

```
User runs: sentinel.exe C:\Downloads
         │
         ▼
   ┌─────────────┐     ┌──────────────────────┐
   │   main.c    │────►│   scanner_init()     │
   │  (driver)   │     │   malloc 1024 slots  │
   └─────────────┘     └──────────────────────┘
         │
         ▼
   ┌─────────────┐     ┌──────────────────────┐
   │ scanner_scan│────►│  FindFirstFileA()    │
   │ (recursive) │     │  for each entry:     │
   │             │     │    if dir → recurse  │
   │             │     │    if .exe/.dll/.msi │
   │             │     │      → copy to list  │
   └─────────────┘     └──────────────────────┘
         │
         ▼
   ┌─────────────┐     ┌──────────────────────┐
   │report_console│────►│  entropy_calculate() │
   │ (per file)   │     │  fopen → fread loop  │
   │              │     │  → byte histogram    │
   │              │     │  → Shannon formula   │
   └─────────────┘     └──────────────────────┘
         │
         ▼
   ┌─────────────┐     ┌──────────────────────┐
   │ report_csv  │────►│  fopen("w")          │
   │             │     │  fprintf rows         │
   │             │     │  fclose               │
   └─────────────┘     └──────────────────────┘
         │
         ▼
   report.csv written
```

### Summary of the Pipeline

```
[Files on Disk] → [scanner: discover & filter]
                → [entropy: read & analyze]
                → [report: format & export]
```

Each stage is a separate C module (separate .c/.h pair). This is the **modular design** principle — each module has one job.

---

## 3. Module Structure

```
SentinelScan/
├── src/
│   ├── main.c         Orchestrator — calls init, scan, report, cleanup
│   ├── scanner.c      Filesystem traversal (Win32 API)
│   ├── entropy.c      Shannon entropy math
│   └── report.c       Console + CSV output
├── include/
│   ├── scanner.h      FileInfo struct, FileList API
│   ├── entropy.h      entropy_calculate() declaration
│   └── report.h       report_console() and report_csv() declarations
├── docs/
│   └── architecture.md
├── Makefile
└── ...
```

### Why headers are separate from source files

Headers (.h) declare the public API — what functions exist and what structs look like. Source files (.c) contain the implementation. This allows the compiler to:

1. **Compile each .c independently** into an object file (.o)
2. **Link them together** into the final executable
3. **Type-check calls** across files (the #include ensures main.c knows the signature of scanner_scan before calling it)

If headers and source were combined, every file would need to be recompiled when anything changed. With the separation, changing `entropy.c` only recompiles entropy.c — the others just re-link.

---

## 4. Function-by-Function Breakdown

---

### 4.1 `scanner_init(FileList *list)`

**Source:** `src/scanner.c:16`

```c
int scanner_init(FileList *list)
{
    list->files = (FileInfo*)malloc(INITIAL_CAPACITY * sizeof(FileInfo));
    if (list->files == NULL) return -1;
    list->count = 0;
    list->capacity = INITIAL_CAPACITY;
    return 0;
}
```

#### What it does

Allocates a block of heap memory large enough to hold 1,024 `FileInfo` structs. Each `FileInfo` is 528 bytes, so this allocates 528 × 1024 = **540,672 bytes** (~528 KB).

#### Why we pre-allocate instead of growing one-by-one

`malloc` + `realloc` are system calls that transition into kernel mode. Making 10,000 individual `malloc(1)` calls would be catastrophically slow. By pre-allocating 1,024 slots in one call, we amortize the cost. Most scans will fit entirely within the initial block. If they don't, `grow_list` doubles the capacity (another single allocation).

#### Why `void*` is cast to `(FileInfo*)`

In C, `malloc` returns `void*` (a pointer to raw memory). The cast tells the compiler "this memory is for FileInfo structs." Without the cast, modern C compilers warn because assigning `void*` to `FileInfo*` without an explicit cast is technically a violation in C++ (though C allows it implicitly).

#### What happens if malloc fails

Returns NULL. We check and return -1. The caller (`main`) sees the -1 and exits with an error message. This is **defensive programming** — every allocation must be checked.

---

### 4.2 `scanner_free(FileList *list)`

**Source:** `src/scanner.c:25`

```c
void scanner_free(FileList *list)
{
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}
```

#### What it does

Releases the heap memory back to the operating system. Sets `count` and `capacity` to 0, and nulls the pointer so any accidental use-after-free will crash immediately (which is easier to debug than corrupt memory).

#### Why `free` needs a pointer

When you call `malloc(size)`, the heap manager stores metadata in the bytes immediately before the returned address (typically the block size and allocation flags). `free` reads that metadata to know how many bytes to release. This is why you must only `free` what `malloc` gave you — passing a pointer to the middle of a block corrupts the heap.

#### Why set pointer to NULL

Defense in depth. If someone calls `scanner_free` twice, the second call's `if (list->files)` check fails (because it's NULL), preventing a double-free crash.

---

### 4.3 `has_target_extension(const char *filename)`

**Source:** `src/scanner.c:6`

```c
static int has_target_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) return 0;
    if (strcmp(dot, ".exe") == 0) return 1;
    if (strcmp(dot, ".dll") == 0) return 1;
    if (strcmp(dot, ".msi") == 0) return 1;
    return 0;
}
```

#### What `strrchr` does

Walks the string from the **end** backwards, looking for the last `.`. This matters for files like `my.file.name.dll` — `strrchr` returns a pointer to `.dll`, while `strchr` (which searches from the beginning) would return `.file`.

```
Filename:    m y . f i l e . n a m e . d l l \0
Index:       0 1 2 3 4 5 6 7 8 9 ...      ^
                                           strrchr returns pointer here
```

#### Why `static`

The `static` keyword on a function means **file scope** — this function is invisible to other .c files. It's a private helper used only within `scanner.c`. This prevents namespace collisions and signals to other developers: "internal function, don't call from outside."

#### Return value convention

Returns `1` (true) or `0` (false). C has no boolean type in C89/C99 — integers serve the purpose.

---

### 4.4 `grow_list(FileList *list)`

**Source:** `src/scanner.c:35`

```c
static int grow_list(FileList *list)
{
    int newcap = list->capacity * 2;
    FileInfo *tmp = (FileInfo*)realloc(list->files,
        newcap * sizeof(FileInfo));
    if (tmp == NULL) return -1;
    list->files = tmp;
    list->capacity = newcap;
    return 0;
}
```

#### How `realloc` works internally

`realloc` asks the heap manager one of two things:

**Case A — Expand in place (fast, O(1)):**

```
Before:  [  your data  |  free space  ]
After:   [  your data (bigger)        ]
```

If the bytes immediately after your block are unused, the heap manager extends your block without moving anything.

**Case B — Move to new location (slow, O(n)):**

```
Before:  [  your data  ]  [  used by someone else  ]
After:   [  used      ]  [  used                  ]
         [  your data (copied)                     ]
```

The heap manager allocates a new block, memcpy's your old data into it, and frees the old block. Your pointer changes.

#### The `tmp` safety pattern

Never write `list->files = realloc(list->files, new_size)`. If `realloc` fails (returns NULL), you've just overwritten your only pointer to the old block — it's now leaked forever. Always use a temporary variable:

```c
FileInfo *tmp = realloc(list->files, new_size);
if (tmp == NULL) {
    // list->files still points to the old block — data safe
    return -1;
}
list->files = tmp;  // only reassign on success
```

#### Doubling strategy amortization

Starting at capacity 1,024:
- Grow to 2,048 (copies 1,024 elements)
- Grow to 4,096 (copies 2,048 elements)
- Grow to 8,192 (copies 4,096 elements)
- Grow to 16,384 (copies 8,192 elements)

The total bytes copied across all growth steps is roughly 2× the final array size. This means **amortized O(1) per insertion** — the geometric growth ensures the occasional large copy is spread out over many cheap insertions.

---

### 4.5 `scanner_scan(const char *root, FileList *list)`

**Source:** `src/scanner.c:46`

This is the most complex function. Let's dissect it piece by piece.

#### The Windows filesystem API

```c
WIN32_FIND_DATAA fdata;
char search_path[MAX_PATH];
HANDLE hFind;

snprintf(search_path, MAX_PATH, "%s\\*", root);
hFind = FindFirstFileA(search_path, &fdata);
```

**Why UTF-8 (A) and not Wide (W):** The "A" suffix means ANSI (narrow/8-bit characters). The alternative is `FindFirstFileW` (wide/UTF-16). For English paths, both work. For international characters (Cyrillic, CJK), the Wide version is needed. We use the A version for simplicity in v0.1.

**Why `*` wildcard:** The Windows filesystem API requires an explicit wildcard pattern. `FindFirstFileA("C:\Users\*", ...)` returns every entry in the directory. You can also filter by pattern like `*.exe`, but we choose to get everything and filter ourselves because we need to distinguish files from directories.

**What `HANDLE` is:** A handle is an opaque 64-bit integer representing a kernel object. In this case, it's a **directory enumeration handle**. It's not a file handle — you can't read from it. You can only iterate with it.

**What `INVALID_HANDLE_VALUE` is:** `((HANDLE)(LONG_PTR)-1)` — the sentinel value meaning "the operation failed." This is NOT NULL (which is a valid handle on some Windows versions). Always compare to `INVALID_HANDLE_VALUE`.

#### The recursive loop

```c
do {
    if (strcmp(fdata.cFileName, ".") == 0 ||
        strcmp(fdata.cFileName, "..") == 0) continue;
    ...
} while (FindNextFileA(hFind, &fdata) != 0);
```

**Why `do-while` and not `while`:** `FindFirstFileA` already retrieved the first entry. If we used `while (FindNextFileA(...))`, we'd skip the first result. `do-while` ensures we process the first entry before looping for the rest.

**Why skip `.` and `..`:** Every directory on an NTFS volume contains two special entries:
- `.` — a hard link to the directory itself
- `..` — a hard link to the parent directory

If we didn't skip them, we'd recurse into `.` (the same directory) infinitely, creating a stack overflow.

#### The recursion itself

```c
if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    snprintf(subdir, MAX_PATH, "%s\\%s", root, fdata.cFileName);
    scanner_scan(subdir, list);
}
```

**Bitwise AND (`&`):** `dwFileAttributes` is a bitmask — a single integer where each bit represents a different file attribute:
- Bit 0 (0x01): `FILE_ATTRIBUTE_READONLY`
- Bit 4 (0x10): `FILE_ATTRIBUTE_DIRECTORY`
- Bit 5 (0x20): `FILE_ATTRIBUTE_ARCHIVE`
- ...and many more

`fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY` checks if bit 4 is set. The result is either 0 (not set) or a non-zero value (set). In C, non-zero is truthy, so the `if` triggers when it IS a directory.

**Recursion depth:** The call stack grows with each nested directory. If your directory tree is 100 folders deep, you'll have 100 `scanner_scan` frames on the stack. Each frame is about 1.2 KB (locals + return addresses + alignment), totaling ~120 KB — well within Windows' 1 MB default thread stack. But add too many more local variables and you'd risk a stack overflow.

#### Adding a file to the list

```c
FileInfo *fi = &list->files[list->count];
strncpy(fi->filename, fdata.cFileName, MAX_PATH - 1);
fi->filename[MAX_PATH - 1] = '\0';
```

**Why `strncpy` and not `strcpy`:** `strcpy` copies until it finds a null byte with no regard for buffer size. If `fdata.cFileName` is 300 bytes (possible with long paths), `strcpy` would write past `fi->filename[259]`, corrupting adjacent struct fields. `strncpy` caps the copy at `MAX_PATH - 1` bytes, then we explicitly null-terminate.

**Why explicit null termination:** `strncpy` does NOT null-terminate if the source is ≥ the limit. Without line 75, `fi->filename` could end without a `\0`, causing `printf`, `strlen`, or any string function to read past the buffer until it finds a random zero byte somewhere in memory — a potential crash or information leak.

---

### 4.6 `entropy_calculate(const char *filepath)`

**Source:** `src/entropy.c:8`

This is the mathematical heart of the program.

#### Step 1: Prepare the histogram

```c
unsigned long long freq[256];
memset(freq, 0, sizeof(freq));
```

`freq` is a stack-allocated array of 256 `unsigned long long`s (256 × 8 = 2,048 bytes). Each slot `freq[i]` will count how many times byte value `i` appears in the file.

`memset` writes zero to every byte of the array, ensuring all counts start at 0.

**Why `unsigned long long` and not `int`:** A file can be several gigabytes. `int` (32-bit signed) maxes out at ~2.1 billion. If a 4 GB file of all null bytes were scanned, `freq[0]` would need the value 4,294,967,296 — which overflows a 32-bit `int`. `unsigned long long` is guaranteed to be at least 64 bits, capable of storing up to ~1.8 × 10¹⁹.

#### Step 2: Open and read the file

```c
fp = fopen(filepath, "rb");
if (fp == NULL) return -1.0;
```

**What happens inside `fopen("file.exe", "rb")`:**

`fopen` is the C standard library wrapper. Internally:

1. `fopen` calls `CreateFileA` (Windows) with `GENERIC_READ`, `FILE_SHARE_READ`, `OPEN_EXISTING`
2. The Windows kernel creates a **file object** in kernel memory
3. A handle (integer) is returned to user mode
4. The C runtime wraps this handle in a `FILE*` struct that includes:
   - A pointer to an internal **buffer** (typically 4 KB)
   - The file position
   - Error/EOF flags

The `"rb"` mode means:
- `r` = read (not write)
- `b` = binary mode (don't translate `\r\n` to `\n`)

**If the file doesn't exist or is inaccessible:** `fopen` returns NULL. We return -1.0 as an error sentinel. The caller (`classify`) treats negative entropy as "Error."

#### Step 3: Read in chunks and build histogram

```c
unsigned char buffer[CHUNK_SIZE];  // 65,536 bytes on stack

while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
    for (size_t i = 0; i < bytes_read; i++) {
        freq[buffer[i]]++;
    }
    total += bytes_read;
}
```

**Why 65,536 bytes per chunk:** This is a classic I/O tuning parameter.
- Too small (e.g., 1 byte): Millions of system calls per file — extremely slow
- Too large (e.g., 64 MB): Wastes stack space and memory bandwidth
- 64 KB: Matches the Windows cache manager's **large-page** granularity. It's large enough to amortize syscall overhead but small enough to live safely on the stack.

**Memory model for the histogram update:**

```
buffer = [0x4D, 0x5A, 0x90, 0x00, 0x4D, 0x5A, ...]
              │     │     │     │     │     │
              ▼     ▼     ▼     ▼     ▼     ▼
freq[0x4D]++  │     │     │     │  └──┘     │
freq[0x5A]++  │     │     │     │          │
freq[0x90]++  │     │  └──┘     │          │
freq[0x00]++  │  └──┘           │          │
              ▼                 ▼          ▼
          freq[77] ++       freq[0] ++   freq[90] ++
```

Each byte of the file maps directly to an index in the 256-slot array. The value at that index is incremented by 1. This is an **O(1) operation per byte** — just an array lookup and increment.

#### Step 4: Apply the Shannon Entropy formula

```c
for (int i = 0; i < 256; i++) {
    if (freq[i] == 0) continue;
    double p = (double)freq[i] / (double)total;
    entropy -= p * log2(p);
}
```

**The formula:**

$$H(X) = -\sum_{i=0}^{255} p(x_i) \cdot \log_2 p(x_i)$$

Where:
- $p(x_i)$ = (occurrences of byte value $i$) ÷ (total bytes)
- $\log_2$ = logarithm base 2

**Visual intuition:**

Imagine you have a bag of 100 marbles in 4 colors:

| Color | Count | p(x) | -p(x)·log₂p(x) |
|-------|-------|------|-----------------|
| Red   | 25    | 0.25 | -0.25 × (-2) = 0.50 |
| Blue  | 25    | 0.25 | -0.25 × (-2) = 0.50 |
| Green | 25    | 0.25 | -0.25 × (-2) = 0.50 |
| Yellow| 25    | 0.25 | -0.25 × (-2) = 0.50 |
| **Total** | 100 | 1.00 | **Entropy = 2.00** |

Maximum entropy for 4 colors = log₂(4) = 2 bits. All colors equally likely.

Now if the bag is 97 red, 1 blue, 1 green, 1 yellow:

| Color | Count | p(x) | -p(x)·log₂p(x) |
|-------|-------|------|-----------------|
| Red   | 97    | 0.97 | -0.97 × (-0.044) = 0.043 |
| Blue  | 1     | 0.01 | -0.01 × (-6.64) = 0.066 |
| Green | 1     | 0.01 | -0.01 × (-6.64) = 0.066 |
| Yellow| 1     | 0.01 | -0.01 × (-6.64) = 0.066 |
| **Total** | 100 | 1.00 | **Entropy = 0.24** |

Low entropy — very predictable (almost always red).

**For bytes (256 possible values):**
- **Maximum:** log₂(256) = 8.0 bits (perfectly random — every byte value equally likely)
- **Minimum:** 0.0 bits (all bytes are identical)
- **Typical compiled .exe:** 4.5 – 6.5 bits
- **Packed/encrypted malware:** 7.0 – 8.0 bits

#### Step 5: The log₂(0) guard

```c
if (freq[i] == 0) continue;
```

If `freq[i] == 0`, then `p = 0.0`, and `log2(0.0) = -inf` (IEEE 754 infinity). Multiplying `0 × (-inf)` produces `NaN` (Not a Number). Once `entropy` becomes `NaN`, every subsequent operation propagates it, and the final result is garbage. The `continue` skips nonexistent byte values entirely.

#### Stack memory during entropy calculation

```
┌─────────────────────────────┐
│ entropy_calculate frame     │  ~200 bytes (locals, return addr, etc.)
├─────────────────────────────┤
│ buffer[65536]               │  65,536 bytes
├─────────────────────────────┤
│ freq[256]                   │  2,048 bytes
├─────────────────────────────┤
│ total, bytes_read, entropy  │  ~32 bytes
├─────────────────────────────┤
│ fp, filepath pointer        │  16 bytes
└─────────────────────────────┘
Total: ~68 KB on the stack
```

---

### 4.7 `classify(double entropy)`

**Source:** `src/report.c:5`

```c
static const char* classify(double entropy)
{
    if (entropy < 0.0) return "Error";
    if (entropy < 6.5) return "Normal";
    if (entropy <= 7.2) return "Moderate";
    return "Suspicious";
}
```

#### Where the strings live

The strings `"Error"`, `"Normal"`, `"Moderate"`, `"Suspicious"` are not on the stack or heap. They are **string literals** stored in the `.rdata` section of the PE executable (a read-only data section). The function returns a pointer to this static memory.

This is safe because:
- The strings never go away (they're part of the executable)
- They're read-only (modifying them causes a segfault)
- No `free` is needed

#### The threshold rationale

| Entropy Range | Classification | Meaning |
|---------------|---------------|---------|
| < 6.5 | Normal | Typical compiled code or data |
| 6.5 – 7.2 | Moderate | Possibly packed, worth investigation |
| > 7.2 | Suspicious | Very likely packed/encrypted |

These thresholds are based on empirical observations of real-world binaries. Legitimate PE files typically have `.text` section entropy between 5.5 and 6.5. Packed binaries (compressed with UPX, ASPack, etc.) routinely exceed 7.0. Cryptographic payloads approach 8.0.

**This is a heuristic, not a proof.** Many legitimate programs use packers (installers, games). Many malware variants try to keep entropy low to evade detection.

#### The error sentinel

`-1.0` is returned by `entropy_calculate` when `fopen` fails. Since entropy can never be negative (it's a sum of non-negative terms), -1.0 is a safe sentinel value. The `classify` function catches it with `entropy < 0.0`.

---

### 4.8 `report_console(const FileList *list)`

**Source:** `src/report.c:13`

```c
void report_console(const FileList *list)
{
    int warn_count = 0;

    printf("\n=== SentinelScan Report ===\n");
    printf("%-30s %-10s %-10s %s\n",
        "Filename", "Size", "Entropy", "Status");

    for (int i = 0; i < list->count; i++) {
        FileInfo *fi = &list->files[i];
        double ent = entropy_calculate(fi->filepath);
        ...
    }
}
```

#### The `%-30s` format specifier

`printf` format strings use:
- `%s` — print a string
- `%30s` — right-align in 30 characters
- `%-30s` — **left**-align in 30 characters
- `%.2f` — print a double with exactly 2 decimal places
- `%lu` — print an unsigned long (DWORD on Windows is unsigned long)

#### Why two passes over the list

The function has two `for` loops:
1. First loop: prints a summary table and counts warnings
2. Second loop: prints detailed per-file entries

This means every file's entropy is calculated **twice**. Each calculation re-reads the entire file from disk. This is wasteful but keeps the code simple. An optimization would calculate entropy once, store it, and reference it in both loops.

---

### 4.9 `report_csv(const FileList *list, const char *output_path)`

**Source:** `src/report.c:56`

```c
int report_csv(const FileList *list, const char *output_path)
{
    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) return -1;

    fprintf(fp, "Filename,Path,Extension,Size,Entropy,Status\n");
    ...
    fclose(fp);
    return 0;
}
```

#### CSV format

CSV stands for **Comma-Separated Values**. It's a plain-text spreadsheet format readable by Excel, Google Sheets, and every programming language.

```
Filename,Path,Extension,Size,Entropy,Status
setup.exe,C:\Downloads,.exe,1249280,7.81,Suspicious
```

#### The `"w"` mode

`fopen(path, "w")`:
- If the file exists: **truncate to zero length** (destroy all content)
- If the file doesn't exist: **create** a new file

This is destructive. If `report.csv` contained important data, running the tool again would erase it.

#### CSV injection vulnerability

If a filename contains `=`, `+`, `-`, `@`, or a comma, Excel might interpret it as a formula:

```
Filename: =CMD|' /C calc'!A0
```

When the CSV is opened in Excel, it could execute arbitrary commands. This is a known attack. Proper mitigation:
- Wrap all fields in double quotes: `"=CMD|...",...`
- Escape internal double quotes by doubling them

---

### 4.10 `main(int argc, char *argv[])`

**Source:** `src/main.c:5`

```c
int main(int argc, char *argv[])
{
    const char *root = ".";
    if (argc > 1) root = argv[1];
    ...
}
```

#### `argc` and `argv`

When your program starts, the C runtime (in `_start` → `__tmainCRTStartup`) parses the command line:

```
sentinel.exe C:\Users
    │          │
    │          └── argv[1]
    └── argv[0]
```

- `argc` = count of arguments (at least 1 — the program name)
- `argv` = array of `char*` strings

If the user doesn't provide a path, `argc == 1`, and we use `"."` (current directory).

#### Return value convention

- `return 0`: Success
- `return 1`: Failure

The operating system (or calling shell) can inspect this. In PowerShell: `$LASTEXITCODE`. In batch: `%ERRORLEVEL%`.

---

## 5. Memory Model

### Memory Map at Runtime

```
┌─────────────────────────────────────────┐
│  STACK (≈1 MB reserved, grows down)     │
│  ├─ main() frame          ~48 bytes     │
│  ├─ scanner_scan() frame  ~1.2 KB       │
│  │     (repeated per recursion depth)   │
│  ├─ entropy_calculate()   ~68 KB        │
│  └─ report_console()      ~68 KB        │
├─────────────────────────────────────────┤
│  HEAP (managed by malloc/realloc/free)  │
│  ├─ FileInfo[1024]        528 KB        │
│  └─ (expands as needed via realloc)     │
├─────────────────────────────────────────┤
│  .data / .bss (global vars)             │
│  (minimal — static buffer not used)    │
├─────────────────────────────────────────┤
│  .rdata (read-only)                     │
│  ├─ "Normal", "Moderate", "Suspicious"  │
│  ├─ format strings                     │
│  └─ string literals                    │
├─────────────────────────────────────────┤
│  .text (code)                           │
│  └─ compiled instructions (~30 KB)      │
└─────────────────────────────────────────┘
```

### Stack vs Heap Decision Table

| Data | Why Stack | Why Not Heap |
|------|-----------|--------------|
| `freq[256]` | Small (2 KB), short-lived, no resizing needed | Heap allocation overhead is wasteful |
| `buffer[65536]` | Tightly scoped to one function | Would need explicit free + allocation check |
| `FileInfo` array | Unknown size at compile time | Must be dynamic — grows with scan |
| String literals | Static data built into executable | N/A — they don't change |

### Pointer Map

```
main's stack:
  list { files=0x00A4F8C0, count=42, capacity=1024 }
                   │
                   ▼  (heap)
         ┌─────────┴─────────┐
         │ FileInfo[0]       │  ← &list->files[0]
         │ FileInfo[1]       │  ← &list->files[1]
         │ ...               │
         │ FileInfo[42]      │  ← list->files[list->count - 1]
         └───────────────────┘
```

Each `FileInfo` in the heap contains:
```
FileInfo {
    filename[260]   ← stack of chars inside the heap block
    filepath[260]   ← same
    extension[8]    ← same
    size (DWORD)    ← 4 bytes
}
```

There is no double indirection — all the string data is stored **inline** in the heap block, not as separate allocations. This is cache-friendly (sequential access to filenames is fast).

---

## 6. API & Systems Internals

### 6.1 Why Win32 API and not POSIX

This tool is Windows-only because:
- `FindFirstFile` / `FindNextFile` / `FindClose` are Windows-specific directory enumeration APIs
- The C standard has no directory traversal functions — `<dirent.h>` is POSIX (Unix/Linux)
- The PRD targets Windows 10/11

The Unix equivalent would be `opendir` / `readdir` / `closedir` from `<dirent.h>`.

### 6.2 What `FindFirstFileA` Does Inside the Kernel

```
User mode                          Kernel mode
─────────                          ──────────
kernel32!FindFirstFileA
    │
    ▼
ntdll!NtQueryDirectoryFile  ───►  nt!NtQueryDirectoryFile
(system call)                       │
                                    ▼
                                ntfs!NtfsQueryDirectory
                                    │
                                    ▼
                                ntfs!NtfsEnumerateDirectory
                                    │
                                    ▼
                                [Walk B-tree index in the
                                 directory's Index Allocation]
                                    │
                                    ▼
                                [Read $MFT record for each entry]
                                    │
                                    ▼
                                [Fill FILE_BOTH_DIR_INFORMATION
                                 structure for first match]
                                    │
                                    ▼
                                Return to user mode
```

The key insight: `FindFirstFileA` returns ONE result. Subsequent calls to `FindNextFileA` advance the enumeration cursor and fetch the next entry. The kernel usually buffers ~16 entries to reduce syscall overhead.

### 6.3 What `fread` Does Inside the System

```
User mode                          Kernel mode
─────────                          ──────────
msvcrt!fread(buffer, 1, 65536, f)
    │
    ├─ Check stdio internal buffer
    │   (if data already buffered, copy directly)
    │
    └─ If buffer empty:
        │
        ▼
       kernel32!ReadFile(handle, ...)
           │
           ▼
          nt!NtReadFile
              │
              ▼
             nt!IoPageRead
                 │
                 ▼
             nt!CcMapData
                 │
                 ├─ If cached: copy from system cache (VACB)
                 └─ If not cached:
                       │
                       ▼
                      ntfs!NtfsRead
                          │
                          ▼
                     [Translate VCN → LCN → disk LBA]
                          │
                          ▼
                     [IRP to storage driver → NVMe/ATA]
```

The system cache (managed by `CcMapData`) means repeated reads of the same file might hit RAM instead of disk.

### 6.4 Why `malloc` Instead of Stack Allocation for FileInfo

`FileInfo` is 528 bytes. The directory could contain 100,000 files. Stack allocation is impossible because:
1. The stack is only 1 MB by default
2. The compiler needs to know the array size at compile time
3. The array must survive function returns (it's used by `report_console` and `report_csv` after `scanner_scan` returns)

Only the heap provides the necessary flexibility.

---

## 7. Security Relevance

### 7.1 Entropy as a Malware Signal

Malware authors often **pack** their executables — compress or encrypt the payload so that:
1. **Signature-based AV can't match byte patterns** — a packed file looks completely different from the original
2. **Static analysis tools can't extract strings** — no IP addresses, URLs, or API names visible
3. **Reverse engineering is harder** — the real code only appears in memory after unpacking

A packer like UPX takes code like this:

```
mov eax, 0x12345678
push eax
call MessageBoxA
```

And compresses it into near-random bytes. When the packed file runs, a small unpacking stub decompresses the original code into memory and transfers control.

The entropy spike (typically > 7.0) is a side effect of this compression. The tool detects this spike.

### 7.2 Entropy Limitations

Entropy alone cannot:
- **Detect all malware** — some malware keeps low entropy by using simple XOR encoding with repetitive keys, or by embedding the payload in a resource section
- **Avoid false positives** — legitimate installers (NSIS, InnoSetup), compressed game assets, and crypto libraries all have high entropy
- **Classify malware type** — entropy tells you "something is unusual" but not what or why

### 7.3 The CSV Injection Concern

The CSV file is intended as a benign report. But if a filename contains `=CMD|...`, and the user opens `report.csv` in Excel without protection, Excel may execute the formula. The tool should wrap CSV fields in double quotes to prevent this.

### 7.4 TOCTOU Race Condition

Between when `scanner_scan` discovers a file and `entropy_calculate` reads it, an attacker could:
1. Replace the file with a different one
2. The report would show entropy for the replacement, not the original

This is a **Time-of-Check to Time-of-Use (TOCTOU)** vulnerability. For a static analysis tool, this is usually acceptable. For forensic use, you'd need to snapshot the filesystem first.

### 7.5 Access Control Bypass

`FindFirstFileA` may fail on directories the user doesn't have permission to read. The error is silently ignored (the function returns 0 and continues). This means the scan may miss files in secured directories. A malicious user could hide files in directories the scanner user can't read.

---

## 8. Risks & Edge Cases

### 8.1 Path Truncation

```c
snprintf(fi->filepath, MAX_PATH, "%s\\%s", root, fdata.cFileName);
```

If `root` + `\` + `filename` exceeds 259 characters, `snprintf` truncates the output. The file will appear in the report with a truncated path that may not exist. The user would see a file listed that can't be found.

Windows long paths (extended-length prefix `\\?\`) support up to 32,767 characters, but this would require wide-character API calls and dynamic path buffers.

### 8.2 Large File Memory

Entropy calculation streams the file in 64 KB chunks — no issue for large files. But every chunk is read from disk, and for very large files (100 MB+), the tool could be slow. A 10 GB file would read 10 GB from disk. The magnitude of this I/O is acceptable for a security scanner but worth knowing.

### 8.3 Stack Overflow via Deep Recursion

A pathological directory structure 10,000 folders deep could exhaust the 1 MB stack. Each `scanner_scan` frame is ~1.2 KB. At 850 nested folders, the stack would overflow. This is extremely rare in practice (Windows limits path depth), but an attacker could create a deeply nested directory tree as an anti-analysis measure.

### 8.4 Heap Exhaustion

With doubling growth, a scan of ~2³¹ files would attempt to allocate ~2³² × 528 bytes — vastly exceeding available memory. In practice, `grow_list` would fail when `realloc` returns NULL, and the file would be skipped. The tool would still produce a partial report.

### 8.5 Empty Files

An empty file has `total = 0`. The code returns `0.0` entropy — classified as "Normal." This is correct: an empty file has zero randomness.

### 8.6 Unicode Filenames

Using `FindFirstFileA` (ANSI) means filenames with non-ASCII characters (Cyrillic, CJK, emoji) may not display correctly. The kernel stores filenames as UTF-16; the A-API converts to the system's current code page, which may lose information.

---

## 9. Potential Optimizations

| Optimization | Current Behavior | Improved Behavior | Effort |
|-------------|-----------------|------------------|--------|
| Cache entropy results | Each file read 2× (console summary + details) + 1× (CSV) = 3 reads per file | Store entropy in FileInfo or parallel array after first calculation | Low |
| Read-ahead buffer | 64 KB chunk size | Tune to match disk cluster size (4 KB) × 16 for optimal alignment | Low |
| Parallel scanning | Single-threaded recursion | Thread pool with work-stealing for independent subtrees | High |
| Skip re-reading by section | Full file read | Parse PE header and read only .text section for faster entropy | Medium |
| Memory-mapped files | `fread` into stack buffer | `CreateFileMapping` + `MapViewOfFile` for zero-copy reads on large files | Medium |
| Skip inaccessible dirs early | `FindFirstFileA` fails, returns 0 | Retry with backup methods or log failures | Low |
| Stack → heap for large buffer | `buffer[65536]` on stack | `malloc` buffer to reduce stack pressure | Low |

### The Most Impactful Optimization

**Caching entropy results** is the single biggest win for v0.2. Currently, each file is read from disk three times (once per `report_console` table loop, once per `report_console` details loop, once per `report_csv`). For a 100 MB file, that's 300 MB of I/O. Calculating once and storing in `FileInfo` (adding a `double entropy` field) would reduce I/O by 66%.

```c
// In scanner.h:
typedef struct {
    char filename[MAX_PATH];
    char filepath[MAX_PATH];
    char extension[MAX_EXTENSION];
    DWORD size;
    double entropy;        // ← new field, -1.0 = not yet calculated
} FileInfo;
```

---

## 10. What I Learned

### As a First-Year CS Student

**1. C is close to the machine.**

Every allocation, every pointer, every byte matters. You can't just create an array — you think about where it lives (stack vs heap), how long it lives, and who frees it. This is the opposite of Python or JavaScript where memory is invisible.

**2. The operating system is not magic.**

`FindFirstFileA` is just a function that makes a system call. The kernel talks to the NTFS driver. The NTFS driver reads the MFT. The MFT is stored on a disk. Each step is a function call, an interrupt, a data structure. There's no magic — just layers of abstraction.

**3. Security tools are simple at their core.**

The entire scanner is ~250 lines of C. The entropy calculation is ~40 lines. The "AI" of tomorrow's malware detection starts with a histogram and a math formula. Complexity comes from integration, scale, and edge cases — not from the core algorithm.

**4. Error handling is not optional.**

Every `malloc`, `fopen`, `FindFirstFile` can fail. Ignoring failures produces crashes, data corruption, or security vulnerabilities. Half the code in professional C programs is error handling.

**5. Bitmasks are everywhere.**

Windows file attributes (`FILE_ATTRIBUTE_DIRECTORY = 0x10`) use individual bits to pack multiple boolean flags into one integer. The `&` operator checks a specific bit. This pattern appears in networking (socket flags), graphics (pixel formats), and every kernel API.

**6. The stack is not infinite.**

A 65 KB buffer on the stack works because we only have one `entropy_calculate` call active at a time. If it called itself recursively, we'd overflow. Understanding stack limits prevents mysterious crashes.

**7. Strings in C are painful.**

C strings are null-terminated arrays. No length prefix. No bounds checking. Every `strcpy` is a potential buffer overflow. Every missing null terminator is a potential crash. Modern languages (Rust, Go, even C++) have safer string types for a reason.

**8. Modular design pays off.**

Because scanner, entropy, and report are separate modules, you could:
- Replace the scanner with a database query (read files from a DB)
- Replace entropy with another analysis (YARA, signature check)
- Add a new report format (HTML, JSON)

Each change is isolated to one file.

---

## Suggested Diagrams (to draw yourself)

### Diagram 1: Memory Layout at Runtime

Draw a rectangle split horizontally:
- Top: Stack (growing down)
  - Label each frame: `main`, `scanner_scan (depth 1)`, `scanner_scan (depth 2)`, ..., `entropy_calculate`
- Middle: Heap
  - Label the large `FileInfo[...]` block
  - Show `list.files` pointer from stack pointing into this heap block
- Bottom: Code / Data sections
  - `.text`, `.rdata`, `.data`

### Diagram 2: Shannon Entropy Histogram

Draw a bar chart with 256 thin bars (x-axis: byte value 0–255, y-axis: count). Show:
- Annotate a "typical .exe" pattern (peaks at 0x00, 0x90, 0xFF, ASCII range)
- Annotate a "packed file" pattern (nearly flat — all bars same height)

### Diagram 3: Recursive Directory Traversal

```
Scanner/
├── src/           ← scanner_scan("Scanner/")
│   └── main.c     └→ scanner_scan("Scanner/src/")
├── include/           └→ (no .exe/.dll/.msi, skip)
│   └── scanner.h
└── reports/
    └── report.csv
```

Draw arrows showing the call stack growing and shrinking as each directory is entered and exited.

### Diagram 4: The fread System Call Path

```
[fread] → [msvcrt internal buffer] → [ReadFile] → [syscall] → [nt!NtReadFile]
    → [Cache Manager] → [cache hit? yes: copy to user, no: page fault] → [NTFS driver]
    → [disk IRP] → [NVMe/ATA command] → [data returns along same path]
```

Show this as a waterfall diagram with time on the y-axis.
