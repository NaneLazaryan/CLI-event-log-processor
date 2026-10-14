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
- **Streaming.** The file is never loaded as a whole and events are never stored. Each line
  is parsed, counted and forgotten; only ids and the summary counters are kept.

## Algorithms and data structures

### Processing pipeline

For every line of the input, `process()` does:

1. `std::getline` into one reused buffer.
2. Blank line? Count it and continue.
3. `parse_event()`: parse the JSON, check the four fields, validate the timestamp.
   Failure means "invalid line": count it and continue.
4. **Duplicate removal:** insert the `id` into a hash set. Already present? Count a duplicate and continue.
5. **Daily summary:** update the counters of the event's day.

Let `N` be the number of lines, `L` the length of a line, `U` the number of unique ids,
`D` the number of distinct days and `T` the number of distinct event types.

### Duplicate removal

**Algorithm.** A `std::unordered_set<std::string>` holds every id seen so far. For each valid
event, `seen_ids.insert(id)` both tests and records the id in one operation. If the insert
fails, the id was already there and the event is a duplicate.

**Why it fits this problem:**

- **Exact.** No event is ever dropped by mistake and no duplicate gets through, so the counts
  in the summary can be trusted.
- **"First occurrence wins" comes for free.** The decision is made at the moment the event is
  read, in file order, so no second pass or tie-breaking is needed.
- **Streaming.** Only the ids are stored, not the events. A duplicate or invalid line costs
  no memory at all.
- **Expected O(1) per event.** The cost is hashing the id (`O(len(id))`) plus one bucket lookup.
  The ids need no ordering, so there is no reason to pay for a sorted structure.

**Cost.** Time `O(N · len(id))` expected; memory `O(U)` (see "Memory usage" below). The set grows
by rehashing, which is amortized O(1) per insert but can cause short pauses.
The worst case is `O(U)` per operation if many ids collide. `std::hash` is not designed to resist
deliberately crafted input; for a log file from a trusted source this is not a concern.

**Alternatives considered:**

| Approach | Time | Memory | Why not chosen |
|---|---|---|---|
| Linear search in a `vector` of seen ids | O(N · U) | O(U) | far too slow for millions of events |
| Load everything, sort by id, remove neighbours | O(N log N) | O(N) (all events) | needs the whole file in memory, and a stable sort to keep the first occurrence |
| `std::set<std::string>` | O(N log U) | similar to the hash set | correct and simple, but its ordering is never used; the hash set has better complexity |
| Store only 64-bit hashes of the ids | O(N) | much smaller (8 bytes per hash plus table overhead) | two different ids could collide and a valid event would be silently dropped (probability about 3 in 100 million at 1M ids, but not zero) |
| Bloom filter | O(N) | very small | false positives would drop valid events, so the counts would be wrong |
| **`std::unordered_set<std::string>`** | **O(N) expected** | **about 75-125 bytes per unique id** | **exact, streaming, simple** |

If memory ever became the limit, the 64-bit hash variant would be the first thing to try, with
the collision risk documented.

### Daily summary calculation

**Algorithm.** The summary is built on the fly, in the same pass, from counters. No event is
kept. For each unique event:

```
DaySummary& day = days[event.date()];   // "YYYY-MM-DD", the first 10 characters
day.total++;                            // events that day
day.by_type[event.type]++;              // events per type
day.users.insert(event.username);       // distinct users that day
```

| Data | Structure | Reason |
|---|---|---|
| days | `std::map<std::string, DaySummary>` | `D` is small (days, not events). ISO dates sort correctly as text, so iterating the map gives the report in date order with no extra sort. Lookup is O(log D) on 10-character keys. |
| events per type | `std::map<std::string, size_t>` | `T` is tiny (a handful of types); the output is alphabetical for free. |
| distinct users per day | `std::unordered_set<std::string>` | Exact distinct count in O(1) per event. `users.size()` is the answer. |

**Why it fits this problem:**

- One pass and no stored events: memory depends on the number of distinct days, types and
  users, not on the number of events.
- Every number in the report is exact.
- Out-of-order input is not a problem, because each event is added to its own day regardless
  of where it appears in the file.
- The day comes from the already validated timestamp, so `date()` can safely take its first 10
  characters. This string is short enough to avoid a heap allocation.

**Alternatives considered:**

| Approach | Why not chosen |
|---|---|
| Keep all events, then group them by day | `O(N)` memory for nothing; the report needs only counters |
| `std::unordered_map` for days, sort at the end | same cost in practice for a small `D`, but needs an extra sort step |
| Sort a vector of usernames per day and remove duplicates | has to store one username per event (`O(N)`), where the set stores only the distinct ones |
| Approximate distinct counting (e.g. HyperLogLog) | tiny memory, but the result is only approximate; exact counts were preferred and the user sets are small compared with the id set |

**Cost.** Per event: `O(log D + log T + len(username))`. Since `D` and `T` are small, this is
effectively constant.

### Time complexity

| Step | Cost per line |
|---|---|
| Read line | O(L) |
| Parse JSON and check fields | O(L), the dominant cost (builds a small JSON tree and allocates) |
| Validate timestamp | O(1): at most a few dozen characters, no allocation |
| Duplicate check | O(len(id)) expected |
| Summary update | O(log D + log T + len(username)) |

The total is **O(N · L)**: linear in the size of the input. JSON parsing is expected to be the
bottleneck, because it allocates for every field of every line; the profiling results below
confirm or correct this. At large `U`, the hash set also becomes slower per operation because
its nodes are scattered in memory (cache misses).

### Memory usage

The program never stores the file, the events, the duplicates or the invalid lines.

| Component | Size |
|---|---|
| Line buffer | O(longest line), one string |
| JSON tree of the current line | O(L), temporary: freed before the next line is read |
| **`seen_ids`** | **O(U), the dominant part** |
| Users sets (all days) | O(distinct (day, user) pairs); the same user is stored once for each day they appear |
| `days` and `by_type` | O(D · T): a few kilobytes |

Approximate memory for `seen_ids` (64-bit build with libstdc++; other standard libraries use
somewhat different node layouts, so check your own measurement):

| Unique ids | Short ids (up to 15 characters, e.g. `e0000001`) | 36-character ids (e.g. UUID) |
|---|---|---|
| 100,000 | about 8 MB | about 12 MB |
| 1,000,000 | about 75 MB | about 125 MB |
| 10,000,000 | about 750 MB | about 1.2 GB |

Ids of 15 characters or fewer fit inside the `std::string` object itself (small-string
optimization). Longer ids need an extra heap allocation, which is why they cost more.
Each set element also carries a node pointer, a cached hash and a bucket entry.

The users sets add on top of that. For example, 10,000 active users over 30 days is at most
300,000 entries, about 20 MB for short usernames.

Memory therefore grows with the number of **unique** events (and the number of distinct
users per day), not with the file size: a file with 90% duplicates uses a fraction of the memory
that a file of the same size with only unique ids needs. The measured peak memory is listed in
the performance section below and can be compared with this table.

### Why this fits the problem

The task needs exact counts, "first occurrence wins" semantics and a day-by-day report. A single
streaming pass with a hash set for ids and small counters per day gives all of that in linear
time, with memory proportional to the unique ids, which is the minimum any exact deduplicator
has to remember.

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

**Bottleneck:** TBD (expected: JSON parsing of every line; compare the measured peak memory with the table in "Memory usage")

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
