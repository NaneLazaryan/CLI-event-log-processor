# logproc: C++ event-log processor

A command-line tool that reads a JSONL event log, removes duplicate events and prints
statistics plus a daily summary. It works in a single streaming pass.

## Build

Requirements: a C++17 compiler (developed with Visual Studio / MSVC) and CMake 3.14+.
The first configure needs internet access: CMake downloads nlohmann/json 3.11.3 and
GoogleTest 1.14.0 with `FetchContent`.

**Visual Studio:** `File > Open > Folder...`, select the project root, choose `logproc.exe`
as the startup item and build.

**Command line:**

```
cmake -S . -B build
cmake --build build --config Release
```

On Linux/macOS add `-DCMAKE_BUILD_TYPE=Release` to the first command. The program is
`build/Release/logproc.exe` with the Visual Studio generator, and `build/logproc` elsewhere.

## Usage

```
logproc --input <file> [--output <file>]
```

| Option | Description |
|---|---|
| `-i`, `--input <file>` | input JSONL file (required) |
| `-o`, `--output <file>` | write the report to a file instead of the console |
| `-h`, `--help` | show help |

**Exit codes:** `0` success, `1` the input file cannot be opened or the output file cannot be
created or written, `2` invalid command-line arguments. Errors go to `stderr`.

Output for `data/example.jsonl` (13 lines: 7 unique events, 2 duplicates, 3 invalid, 1 blank):

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

## Input format and rules

The task description did not specify the details, so these are my decisions. Each rule is
stated only here.

One JSON object per line:

```json
{"id": "e001", "timestamp": "2026-10-01T08:15:00Z", "type": "login", "username": "alice"}
```

- **Fields.** `id`, `timestamp`, `type` and `username` are required, must be non-empty
  strings, and extra fields are ignored. Windows line endings (`\r\n`) are accepted.
- **Timestamp.** UTC ISO-8601: `YYYY-MM-DDTHH:MM:SSZ`, with optional fractional seconds.
  Calendar rules are checked (month length, leap years, hour < 24, ...). Offsets such as
  `+02:00` are rejected.
- **Invalid line.** Not valid JSON, not a JSON object, a missing / non-string / empty
  required field, or an invalid timestamp. It is skipped and counted.
- **Blank line.** Empty or only spaces/tabs. It is skipped and counted separately from invalid lines.
- **Duplicate.** Two events with the same `id`, whatever their other fields are. The first
  occurrence is kept; later ones are dropped and do not affect the summary.
- **Daily summary.** Grouped by the UTC date of the timestamp. "Unique users" is counted per
  day. Days are printed in date order and event types alphabetically.

## Design

| File | Responsibility |
|---|---|
| `src/event.h` | `Event` struct and `date()` |
| `src/parser.h/.cpp` | `parse_event()` (one line to `std::optional<Event>`) and `is_valid_timestamp()` |
| `src/event_processor.h/.cpp` | `process()` (read, dedup, aggregate) and `report()` (print) |
| `src/main.cpp` | command-line handling only |

The logic is in a library target (`logproc_lib`) used by both the executable and the tests.
Tests are in `tests/`, sample data in `data/example.jsonl`.

**Data structures** (what and why):

| Purpose | Choice | Why |
|---|---|---|
| Ids already seen | `std::unordered_set<std::string>` | one call tests and records an id in expected O(1); exact; the ids need no ordering (`std::set` would also work) |
| Days | `std::map<std::string, DaySummary>` | few days; ISO dates sort correctly as text, so the report is in date order without a sort |
| Events per type | `std::map<std::string, size_t>` | few types; alphabetical output for free |
| Users per day | `std::unordered_set<std::string>` | exact distinct count, expected O(1) per event |

**Other decisions:**

- `process(std::istream&)` takes a stream, not a file name, so tests need no files.
- `json::parse` is called with `allow_exceptions = false`: a bad line is counted, not thrown.
- The timestamp is validated by hand: no locale, no stream, no allocation, and real calendar rules.
- Events are never stored. Each line is parsed, counted and forgotten, so memory depends on the
  number of unique ids, not on the file size. The `id` and `username` strings are moved into
  the containers instead of copied.

## Tests

20 unit tests (GoogleTest), using in-memory input:

| Suite | Tests | Covers |
|---|---|---|
| `Parser` | 7 | valid line, malformed JSON, missing / wrong-type / empty fields, non-object JSON, extra fields and `\r`, bad timestamp |
| `Timestamp` | 3 | valid forms, invalid calendar values, invalid formats |
| `Processor` | 8 | empty input, blank vs invalid lines, first duplicate wins, day boundary, sorted days, daily totals / types / users, CRLF and missing final newline, hand-computed sample |
| `Report` | 2 | empty result, statistics and daily lines |

Run them from Visual Studio **Test Explorer**, or with
`ctest --test-dir build -C Release --output-on-failure` (omit `-C Release` outside Visual Studio).

## Performance

### Complexity

`N` = number of lines, `L` = line length, `U` = unique ids, `D` = days, `T` = event types.

- **Time: O(N · L).** Every line is read and parsed once, and parsing is the largest cost.
  Duplicate check: O(len(id)) expected. Summary update: O(log D + log T + len(username)),
  effectively constant because `D` and `T` are small.
- **Memory: O(U + distinct (day, user) pairs).** The file, the events, the duplicates and the
  invalid lines are not stored. Roughly 75 bytes per unique id for ids up to 15 characters
  and 125 bytes for 36-character ids (64-bit libstdc++), so about 75-125 MB per million
  unique ids.

### Measurements

Release build, synthetic data with about 10% duplicate ids.

| Events | Wall time | Throughput | Peak memory |
|---|---|---|---|
| 100,000 | TBD | TBD | TBD |
| 1,000,000 | TBD | TBD | TBD |

### Environment

- CPU / RAM: TBD
- OS: TBD
- Compiler and build configuration: TBD

## Limitations and possible improvements

- Only UTC timestamps ending in `Z` are accepted; deduplication is by `id` only (first wins).
- All unique ids are kept in memory; the program is single-threaded; the report is plain text.
- Ideas: JSON output, a `--keep last` option, a faster JSON parser (e.g. simdjson),
  `reserve()` on the id set, parallel parsing of file chunks.
