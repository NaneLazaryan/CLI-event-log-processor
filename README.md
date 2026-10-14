# logproc: C++ event-log processor

A command-line tool that reads a JSONL event log, removes duplicate events and prints
statistics plus a daily summary.

- Streaming, single pass over the input
- Duplicate removal by event `id` (the first occurrence is kept)
- Daily summary per UTC day: number of events, events per type, number of unique users
- Invalid lines are skipped and counted, they never crash the program
- 20 unit tests (GoogleTest)

## Requirements

- A C++17 compiler (developed with Visual Studio / MSVC)
- CMake 3.14 or newer (3.20+ if you want to use `ctest --test-dir`)
- Internet access on the first configure: CMake downloads
  [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 and
  [GoogleTest](https://github.com/google/googletest) 1.14.0 with `FetchContent`

## Build

**Visual Studio:** `File > Open > Folder...`, select the project root and wait for the
CMake configure step to finish. Choose `logproc.exe` as the startup item and build.

**Command line (Windows, Visual Studio generator):**

```
cmake -S . -B build
cmake --build build --config Release
```

**Command line (Linux / macOS):**

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Usage

```
logproc --input <file> [--output <file>]
```

| Option | Description |
|---|---|
| `-i`, `--input <file>` | input JSONL file (required) |
| `-o`, `--output <file>` | write the report to a file instead of the console |
| `-h`, `--help` | show help |

Example (Windows path; on Linux/macOS use `./build/logproc`):

```
.\build\Release\logproc.exe --input data\example.jsonl
```

Output for the included `data/example.jsonl` (13 lines: 7 unique valid events,
2 duplicates, 3 invalid lines and 1 blank line):

```
Processing statistics
  Lines read:         13
  Empty lines:        1
  Invalid lines:      3
  Duplicates removed: 2
  Unique events:      7

Daily summary (UTC)
2026-10-01 events: 3 unique users: 2
    login       1
    logout      1
    purchase    1
2026-10-02 events: 3 unique users: 3
    login       2
    purchase    1
2026-10-03 events: 1 unique users: 1
    logout      1
```

Every line is counted exactly once:
`lines read = empty lines + invalid lines + duplicates + unique events`.

**Exit codes:** `0` success, `1` the input file cannot be opened or the output file cannot
be created or written, `2` invalid command-line arguments. Errors go to `stderr`, the
report goes to `stdout` (or to the `--output` file).

## Input format and assumptions

The task description did not specify the details, so I made the following decisions:

**Input.** One JSON object per line:

```json
{"id": "e001", "timestamp": "2026-10-01T08:15:00Z", "type": "login", "username": "alice"}
```

- All four fields (`id`, `timestamp`, `type`, `username`) are required, must be strings
  and must not be empty. Extra fields are ignored.
- `timestamp` must be UTC ISO-8601: `YYYY-MM-DDTHH:MM:SSZ`, optionally with fractional
  seconds (`...56.123Z`). Calendar rules are checked (month length, leap years, hour < 24,
  and so on). Time-zone offsets such as `+02:00` are rejected, because all input is
  assumed to be UTC.
- Windows line endings (`\r\n`) are accepted.

**Invalid line.** A non-empty line is counted as invalid if it is not valid JSON, is not a
JSON object, has a missing, non-string or empty required field, or has an invalid timestamp.
Invalid lines are skipped.

**Blank lines** (empty or only spaces/tabs) are skipped and counted separately, not as invalid.

**Duplicates.** Two events are duplicates if they have the same `id`, whatever their other
fields are. The **first** occurrence is kept and later ones are dropped, so a dropped
duplicate does not affect the summary.

**Daily summary.** The day is the UTC date of the timestamp. "Unique users" is counted per
day, so the same user on two different days counts once on each day. Days are printed in
date order and event types alphabetically.

## Design

| File | Responsibility |
|---|---|
| `src/event.h` | `Event` struct (`id`, `timestamp`, `type`, `username`) and `date()` |
| `src/parser.h/.cpp` | `parse_event()`: one line to `std::optional<Event>`; `is_valid_timestamp()` |
| `src/event_processor.h/.cpp` | `process()`: read, dedup and aggregate; `report()`: print the results |
| `src/main.cpp` | command-line handling only |

The logic is in a library target (`logproc_lib`) that both the executable and the tests link against.

Main decisions:

- **`process(std::istream&)` instead of a file name.** The same code works for files and
  for in-memory text, so the tests need no files.
- **No exceptions on bad data.** `json::parse` is called with `allow_exceptions = false`,
  so a malformed line returns a "discarded" value that is counted, not thrown.
- **Hand-written timestamp validation.** It needs no locale, no stream and no allocation,
  and it checks real calendar rules. The format is fixed, so a small validator is enough.
- **One pass, little copying.** A single `std::string` is reused for every line, and the
  `id` and `username` strings are moved into the containers instead of copied.
- **Sorted containers where order matters.** `std::map` keyed by `YYYY-MM-DD` gives date
  order for free, because ISO dates sort correctly as text. `std::unordered_set` is used
  for the seen ids and the users, where order does not matter.

**Complexity.** Time is O(total input size): each line is parsed once, and set/map
operations are O(1) average / O(log days). Memory is O(unique ids + distinct
(day, type, user) combinations); it does not grow with the number of duplicate or
invalid lines.

## Tests

20 unit tests in four suites:

| Suite | Tests | Covers |
|---|---|---|
| `Parser` | 7 | valid line, malformed JSON, missing / wrong-type / empty fields, non-object JSON, extra fields, `\r`, bad timestamp |
| `Timestamp` | 3 | valid forms (fractional seconds, leap day), invalid calendar values, invalid formats |
| `Processor` | 8 | empty input, blank vs invalid lines, duplicates (first wins), day boundary, sorted days, daily totals / types / unique users, CRLF and missing final newline, and a hand-computed sample |
| `Report` | 2 | empty result, statistics and daily lines |

Run them from Visual Studio **Test Explorer**, or from the command line:

```
ctest --test-dir build -C Release --output-on-failure
```

(`-C Release` is needed for the Visual Studio generator; on Linux/macOS omit it.)
The `HandComputedSample` test uses numbers calculated by hand from the input, not copied
from the program output.

## Performance analysis

**Method.** Release build (`-O2` / `/O2`). Test files with 100,000 and 1,000,000 events
and about 10% duplicate ids were generated with `tools/gen_data.py`. Wall time was
measured with `Measure-Command` (PowerShell), peak memory with Task Manager / `/usr/bin/time -v`.

**Machine:** TBD (CPU, RAM, OS, compiler version)

| Events | Duplicates | Wall time | Throughput | Peak memory |
|---|---|---|---|---|
| 100,000 | ~10% | TBD | TBD | TBD |
| 1,000,000 | ~10% | TBD | TBD | TBD |

**Bottleneck:** TBD (expected: JSON parsing of every line)

**Optimizations tried:** TBD (before / after numbers)

## Limitations

- Only UTC timestamps ending in `Z` are accepted.
- Deduplication is by `id` only and always keeps the first occurrence.
- All unique ids are kept in memory, so memory use grows with the number of unique events.
- The report is plain text only (no JSON output).
- The program is single-threaded.
- Event types and usernames are compared case-sensitively and are not normalized.

## Possible improvements

- JSON / CSV output and a `--keep last` option
- A faster JSON parser (e.g. simdjson) or a specialized parser for the fixed schema
- `reserve()` on the id set when the input size is known
- Parallel parsing of file chunks, with a merge step
- Support for time-zone offsets in timestamps

## Project layout

```
.
├── CMakeLists.txt
├── README.md
├── src/
│   ├── event.h
│   ├── parser.h / parser.cpp
│   ├── event_processor.h / event_processor.cpp
│   └── main.cpp
├── tests/
│   ├── test_parser.cpp
│   └── test_processor.cpp
├── data/
│   └── example.jsonl
└── tools/
    └── gen_data.py
```
