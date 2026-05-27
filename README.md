# SentinelScan

Lightweight executable scanning tool written in C. Recursively scans directories for Windows installer and executable files and performs static analysis.

## Features

- Recursive directory scanning (`.exe`, `.dll`, `.msi`)
- Shannon entropy calculation
- Suspicion classification (Normal / Moderate / Suspicious)
- CSV report export

## Build

Requires MinGW GCC.

```
make
```

## Usage

```
sentinel.exe [directory]
```

Scans the current directory by default.

## Output

Console report with entropy scores and a `report.csv` file.

## License

Apache 2.0
