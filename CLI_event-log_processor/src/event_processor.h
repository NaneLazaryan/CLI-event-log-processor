#pragma once

#include <string>
#include <unordered_set>
#include <map>

struct DaySummery
{
	std::size_t total = 0;  // total number of unique events
	std::map<std::string, std::size_t> by_type;  // event type -> count
	std::unordered_set<std::string> users;  // distinct users
};

struct Counters
{
	std::size_t lines_read = 0;
	std::size_t invalid = 0;  // invalid JSON object, missing field, ...
	std::size_t duplicates = 0;
	std::size_t empty_lines = 0;
	std::size_t unique_events = 0;
};

struct Result
{
	Counters cnts;
	std::map<std::string, DaySummery> days;  // key "YYYY-MM-DD"
};

Result process(std::istream& in);
void report(const Result& result, std::ostream& out);