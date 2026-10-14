#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "event_processor.h"

namespace {

    // Runs the processor on an in-memory text, so tests need no files.
    Result run(const std::string& text) {
        std::istringstream in(text);
        return process(in);
    }

    // One valid JSONL line (with trailing newline) for the given values.
    std::string event_line(const std::string& id, const std::string& ts,
        const std::string& type = "login",
        const std::string& user = "alice") {
        return R"({"id":")" + id + R"(","timestamp":")" + ts + R"(","type":")" +
            type + R"(","username":")" + user + "\"}\n";
    }

} 

TEST(Processor, EmptyInput) {
    const Result r = run("");

    EXPECT_EQ(r.cnts.lines_read, 0u);
    EXPECT_EQ(r.cnts.unique_events, 0u);
    EXPECT_TRUE(r.days.empty());
}

TEST(Processor, BlankAndInvalidLinesAreCountedSeparately) {
    const Result r = run(
        "   \n"                                                                          // blank
        "{this is not json\n"                                                              // malformed JSON
        R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","type":"login"})" "\n"           // missing username
        R"({"id":"e2","timestamp":"not-a-timestamp","type":"login","username":"bob"})" "\n"   // bad timestamp
        + event_line("e3", "2026-10-01T12:00:00Z"));                                       // one good line

    EXPECT_EQ(r.cnts.lines_read, 5u);
    EXPECT_EQ(r.cnts.empty_lines, 1u);
    EXPECT_EQ(r.cnts.invalid, 3u);
    EXPECT_EQ(r.cnts.unique_events, 1u);
}

TEST(Processor, DuplicateIdKeepsTheFirstOccurrence) {
    // Same id, but different day, type and username: only the first line counts.
    const Result r = run(event_line("a", "2026-01-01T10:00:00Z", "login", "u1") +
        event_line("a", "2026-01-02T10:00:00Z", "logout", "u2"));

    EXPECT_EQ(r.cnts.duplicates, 1u);
    EXPECT_EQ(r.cnts.unique_events, 1u);
    ASSERT_EQ(r.days.size(), 1u);
    const auto& day = r.days.at("2026-01-01");
    EXPECT_EQ(day.total, 1u);
    EXPECT_EQ(day.users.size(), 1u);
    EXPECT_EQ(day.by_type.count("logout"), 0u);
}

TEST(Processor, DayBoundaryIsRespected) {
    const Result r = run(event_line("e1", "2026-03-01T23:59:59Z") +
        event_line("e2", "2026-03-02T00:00:00Z"));

    ASSERT_EQ(r.days.size(), 2u);
    EXPECT_EQ(r.days.at("2026-03-01").total, 1u);
    EXPECT_EQ(r.days.at("2026-03-02").total, 1u);
}

TEST(Processor, DaysAreSortedEvenWhenInputIsOutOfOrder) {
    const Result r = run(event_line("e1", "2026-03-03T10:00:00Z") +
        event_line("e2", "2026-03-01T10:00:00Z") +
        event_line("e3", "2026-03-02T10:00:00Z"));

    std::string order;
    for (const auto& entry : r.days) order += entry.first + " ";
    EXPECT_EQ(order, "2026-03-01 2026-03-02 2026-03-03 ");
}

TEST(Processor, DailyTotalsTypesAndUniqueUsers) {
    const Result r = run(event_line("e1", "2026-03-01T01:00:00Z", "login", "alice") +
        event_line("e2", "2026-03-01T02:00:00Z", "logout", "alice") +
        event_line("e3", "2026-03-01T03:00:00Z", "login", "bob"));

    const auto& day = r.days.at("2026-03-01");
    EXPECT_EQ(day.total, 3u);
    EXPECT_EQ(day.users.size(), 2u);            // alice appears twice but counts once
    EXPECT_EQ(day.by_type.at("login"), 2u);
    EXPECT_EQ(day.by_type.at("logout"), 1u);
}

TEST(Processor, WindowsLineEndingsAndMissingFinalNewline) {
    const Result r = run(
        "{\"id\":\"e1\",\"timestamp\":\"2026-10-01T12:00:00Z\",\"type\":\"login\",\"username\":\"alice\"}\r\n"
        "{\"id\":\"e2\",\"timestamp\":\"2026-10-01T13:00:00Z\",\"type\":\"login\",\"username\":\"bob\"}");  // no '\n'

    EXPECT_EQ(r.cnts.lines_read, 2u);
    EXPECT_EQ(r.cnts.invalid, 0u);
    EXPECT_EQ(r.cnts.unique_events, 2u);
}

// The same content computed by hand.
TEST(Processor, HandComputedSample) {
    const Result r = run(R"JSONL({"id":"e001","timestamp":"2026-10-01T08:15:00Z","type":"login","username":"alice"}
{"id":"e002","timestamp":"2026-10-01T09:30:12Z","type":"purchase","username":"bob"}
{"id":"e003","timestamp":"2026-10-01T23:59:59Z","type":"logout","username":"alice"}
{"id":"e004","timestamp":"2026-10-02T00:00:00Z","type":"login","username":"carol"}
{"id":"e002","timestamp":"2026-10-01T09:30:12Z","type":"purchase","username":"bob"}
{"id":"e005","timestamp":"2026-10-02T10:05:41Z","type":"purchase","username":"alice"}
{"id":"e006","timestamp":"2026-10-02T11:20:00Z","type":"login","username":"bob"}
{this is not valid json
{"id":"e007","timestamp":"2026-10-02T12:00:00Z","type":"login"}

{"id":"e008","timestamp":"2026-10-03T07:45:30Z","type":"logout","username":"carol"}
{"id":"e004","timestamp":"2026-10-02T00:00:00Z","type":"login","username":"carol"}
{"id":"e009","timestamp":"not-a-timestamp","type":"login","username":"dave"})JSONL");

    EXPECT_EQ(r.cnts.lines_read, 13u);
    EXPECT_EQ(r.cnts.empty_lines, 1u);
    EXPECT_EQ(r.cnts.invalid, 3u);
    EXPECT_EQ(r.cnts.duplicates, 2u);
    EXPECT_EQ(r.cnts.unique_events, 7u);
    // every line ends up in exactly one bucket
    EXPECT_EQ(r.cnts.lines_read, r.cnts.empty_lines + r.cnts.invalid +
        r.cnts.duplicates + r.cnts.unique_events);

    ASSERT_EQ(r.days.size(), 3u);

    const auto& d1 = r.days.at("2026-10-01");
    EXPECT_EQ(d1.total, 3u);
    EXPECT_EQ(d1.users.size(), 2u);  // alice, bob

    const auto& d2 = r.days.at("2026-10-02");
    EXPECT_EQ(d2.total, 3u);
    EXPECT_EQ(d2.users.size(), 3u);  // carol, alice, bob
    EXPECT_EQ(d2.by_type.at("login"), 2u);

    const auto& d3 = r.days.at("2026-10-03");
    EXPECT_EQ(d3.total, 1u);
    EXPECT_EQ(d3.users.size(), 1u);
}

TEST(Report, EmptyResultSaysNoValidEvents) {
    std::ostringstream out;
    report(run(""), out);

    EXPECT_NE(out.str().find("(no valid events)"), std::string::npos);
}

TEST(Report, ContainsStatisticsAndDailyLines) {
    std::ostringstream out;
    report(run(event_line("e1", "2026-10-01T10:00:00Z", "login", "alice") +
        event_line("e1", "2026-10-01T10:00:00Z", "login", "alice")),
        out);

    const std::string text = out.str();
    EXPECT_NE(text.find("Duplicates removed: 1"), std::string::npos);
    EXPECT_NE(text.find("2026-10-01 events: 1 unique users: 1"), std::string::npos);
}
