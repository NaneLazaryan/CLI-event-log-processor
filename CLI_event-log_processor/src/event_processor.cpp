#include "event_processor.h"
#include <istream>
#include <ostream>
#include <iomanip>
#include "parser.h"

bool is_space(const std::string& line)
{
	for (const char c : line)
		if (c != ' ' && c != '\t' && c != '\r') return false;

	return true;
}

Result process(std::istream& in)
{
	Result result;
	Counters& cnts = result.cnts;

	std::unordered_set<std::string> seen_ids;  // for removing events with duplicate id
	std::string line;

	while (std::getline(in, line)) {
		cnts.lines_read++;

		if (is_space(line)) {
			cnts.empty_lines++;
			continue;
		}

		std::optional<Event> parsed = parse_event(line);
		if (!parsed) {
			cnts.invalid++;
			continue;
		}

		Event& event = *parsed;

		if (!seen_ids.insert(std::move(event.id)).second) {
			cnts.duplicates++;
			continue;
		}

		cnts.unique_events++;

		DaySummery& day = result.days[event.date()];
		day.total++;
		day.by_type[event.type]++;
		day.users.insert(std::move(event.username));
	}

	return result;
}

void report(const Result& result, std::ostream& out)
{
	const Counters& cnts = result.cnts;

	out << "Processing statistics\n"
		<< "  Lines read:         " << cnts.lines_read << '\n'
		<< "  Empty lines:        " << cnts.empty_lines << '\n'
		<< "  Invalid lines:      " << cnts.invalid << '\n'
		<< "  Duplicates removed: " << cnts.duplicates << '\n'
		<< "  Unique events:      " << cnts.unique_events << '\n';

	out << "\nDaily summary (UTC)\n";
	if(result.days.empty()) {
		out << "  (no valid events)\n";
		return;
	}

	for (const auto& [date, day] : result.days) {
		out << date << " events: " << day.total
			<< " unique users: " << day.users.size() << '\n';

		for (const auto& [type, count] : day.by_type) {
			out << "    " << std::left << std::setw(12) << type << std::right << count << '\n';
		}
	}
}