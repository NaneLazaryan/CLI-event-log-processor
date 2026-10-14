# CLI-event-log-processor

Project Structure
```
event-log-processor/
├── CMakeLists.txt            # build config: libraries, targets, tests
├── README.md                 # build/run steps, assumptions, design, perf results
│
├── src/
│   ├── event.h             # Event struct (id, timestamp, type, user)
│   ├── parser.h            # parse_line(), date extraction, timestamp validation
│   ├── parser.cpp
│   ├── processor.h         # DaySummary, Stats, Result, process(std::istream&)
│   ├── processor.cpp       # read loop, dedup, daily aggregation
│   └── main.cpp            # CLI only: argv parsing, file opening, printing
│
├── tests/
│   ├── test_parser.cpp       # valid line, malformed JSON, missing field, bad timestamp
│   └── test_processor.cpp    # duplicates, day boundaries, unique users, empty input
│
├── data/
│   ├── sample.jsonl          # small hand-made file with all edge cases
│   └── expected_output.txt   # optional: the output you verified by hand
│__
```
