#include "parser.h"
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace utility 
{
	bool is_digit(char c) 
	{
		return c >= '0' && c <= '9';
	}

	// All chars are digits from pos to pos + count
	bool all_digits(std::string_view s, std::size_t pos, std::size_t count)
	{
		if (pos + count > s.size()) return false;
		for(std::size_t i = pos; i < pos + count; i++) 
			if (!is_digit(s[i])) return false;

		return true;
	}

	// Convert string (from pos to pos + count) to int
	int to_int(std::string_view s, std::size_t pos, std::size_t count)
	{
		int res = 0;
		for (std::size_t i = pos; i < pos + count; i++) {
			res = res * 10 + (s[i] - '0');
		}
		return res;
	}

	bool is_leap(int year)
	{
		return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
	}

	int days_in_month(int year, int month)
	{
		static constexpr int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (is_leap(year) && month == 2) return 29;
		return days[month - 1];
	}

	// Reads obj[key] as a non-empty string into out.
	bool read_str_field(const json& obj, const char* key, std::string& out)
	{
		const auto it = obj.find(key);
		if (it == obj.end() || !it->is_string()) return false;

		out = it->get_ref<const std::string&>();
		return !out.empty();
	}
}

std::optional<Event> parse_event(const std::string& line)
{
	// allow_exceptions = false: invalid JSON yields a "discarded"
	const auto json = json::parse(line.begin(), line.end(), nullptr, false);
	if (json.is_discarded() || !json.is_object()) return std::nullopt;

	Event event;
	if (!utility::read_str_field(json, "id", event.id) ||
		!utility::read_str_field(json, "timestamp", event.timestamp) ||
		!utility::read_str_field(json, "type", event.type) ||
		!utility::read_str_field(json, "username", event.username))
		return std::nullopt;

	if (!is_valid_timestamp(event.timestamp)) return std::nullopt;

	return event;
}

bool is_valid_timestamp(std::string_view ts)
{
	// Shortest valid form: "YYYY-MM-DDTHH:MM:SSZ" (20 characters).
	if (ts.size() < 20) return false;

	// Checking fixed oparators
	if (ts[4] != '-' || ts[7] != '-' || ts[10] != 'T' || ts[13] != ':' || ts[16] != ':') return false;

	// Checking year, month, day, hour, minute, second
	if (!utility::all_digits(ts, 0, 4) || !utility::all_digits(ts, 5, 2) ||
		!utility::all_digits(ts, 8, 2) || !utility::all_digits(ts, 11, 2) ||
		!utility::all_digits(ts, 14, 2) || !utility::all_digits(ts, 17, 2)) 
		return false;

	// Ranges
	const int year = utility::to_int(ts, 0, 4);
	const int month = utility::to_int(ts, 5, 2);
	const int day = utility::to_int(ts, 8, 2);
	const int hour = utility::to_int(ts, 11, 2);
	const int minute = utility::to_int(ts, 14, 2);
	const int sec = utility::to_int(ts, 17, 2);

	if (month == 0 || month > 12 ||
		day == 0 || day > utility::days_in_month(year, month) ||
		hour > 23 || minute > 59 || sec > 59)
		return false;
	
	size_t pos = 19;
	if (ts[pos] == '.') {
		pos++;
		const std::size_t start = pos;
		while (pos < ts.size() && utility::is_digit(ts[pos])) pos++;
		if (pos == start) return false; // "." with no digits
	}

	return pos + 1 == ts.size() && ts[pos] == 'Z';
}