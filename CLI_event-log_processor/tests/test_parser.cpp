#include <gtest/gtest.h>

#include <string>

#include "parser.h"

// ---------------------------------------------------------------------------
// parse_event: one JSONL line -> std::optional<Event>
// ---------------------------------------------------------------------------

TEST(Parser, ValidLine) {
    auto e = parse_event(R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","type":"login","username":"alice"})");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->id, "e1");
    EXPECT_EQ(e->timestamp, "2026-10-01T12:00:00Z");
    EXPECT_EQ(e->type, "login");
    EXPECT_EQ(e->username, "alice");
    EXPECT_EQ(e->date(), "2026-10-01");
}

TEST(Parser, MalformedJson) {
    EXPECT_FALSE(parse_event("{not json").has_value());
}

TEST(Parser, MissingField) {
    EXPECT_FALSE(parse_event(R"({"id":"e1"})").has_value());
    // each of the four required fields is needed
    EXPECT_FALSE(parse_event(R"({"timestamp":"2026-10-01T12:00:00Z","type":"login","username":"alice"})"));  // no id
    EXPECT_FALSE(parse_event(R"({"id":"e1","type":"login","username":"alice"})"));                          // no timestamp
    EXPECT_FALSE(parse_event(R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","username":"alice"})"));      // no type
    EXPECT_FALSE(parse_event(R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","type":"login"})"));      // no username
}

TEST(Parser, WrongTypeOrEmptyFieldIsRejected) {
    EXPECT_FALSE(parse_event(R"({"id":5,"timestamp":"2026-10-01T12:00:00Z","type":"login","username":"alice"})"));
    EXPECT_FALSE(parse_event(R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","type":"login","username":null})"));
    EXPECT_FALSE(parse_event(R"({"id":"","timestamp":"2026-10-01T12:00:00Z","type":"login","username":"alice"})"));
}

TEST(Parser, NotASingleJsonObjectIsRejected) {
    EXPECT_FALSE(parse_event(""));
    EXPECT_FALSE(parse_event("[1,2,3]"));
    EXPECT_FALSE(parse_event(R"({"id":"e1","timestamp":"2026-10-01T12:00:00Z","type":"login","username":"alice"} xyz)"));
}

TEST(Parser, ExtraFieldsAndCarriageReturnAreTolerated) {
    // extra fields are ignored, and a trailing '\r' (Windows line ending) is fine
    auto e = parse_event("{\"id\":\"e1\",\"timestamp\":\"2026-10-01T12:00:00Z\",\"type\":\"login\",\"username\":\"alice\",\"age\":45}\r");
    EXPECT_TRUE(e.has_value());
}

TEST(Parser, InvalidTimestampMakesLineInvalid) {
    EXPECT_FALSE(parse_event(R"({"id":"e1","timestamp":"not-a-timestamp","type":"login","username":"alice"})"));
}

// ---------------------------------------------------------------------------
// is_valid_timestamp: UTC ISO-8601, YYYY-MM-DDTHH:MM:SS[.fff]Z
// ---------------------------------------------------------------------------

TEST(Timestamp, ValidForms) {
    EXPECT_TRUE(is_valid_timestamp("2026-10-01T12:34:56Z"));
    EXPECT_TRUE(is_valid_timestamp("2026-10-01T12:34:56.123Z"));   // fractional seconds
    EXPECT_TRUE(is_valid_timestamp("2026-12-31T23:59:59Z"));       // last second of a year
    EXPECT_TRUE(is_valid_timestamp("2024-02-29T00:00:00Z"));       // leap year
}

TEST(Timestamp, InvalidCalendarValues) {
    EXPECT_FALSE(is_valid_timestamp("2026-02-29T00:00:00Z"));      // 2026 is not a leap year
    EXPECT_FALSE(is_valid_timestamp("2026-04-31T00:00:00Z"));      // April has 30 days
    EXPECT_FALSE(is_valid_timestamp("2026-13-01T00:00:00Z"));      // month 13
    EXPECT_FALSE(is_valid_timestamp("2026-10-01T24:00:00Z"));      // hour 24
}

TEST(Timestamp, InvalidFormat) {
    EXPECT_FALSE(is_valid_timestamp(""));
    EXPECT_FALSE(is_valid_timestamp("2026-10-01T12:34:56"));       // no 'Z'
    EXPECT_FALSE(is_valid_timestamp("2026-10-01T12:34:56+02:00")); // offsets are not accepted
    EXPECT_FALSE(is_valid_timestamp("2026-10-01T12:34:56.Z"));     // '.' without digits
    // Regression: every character of every digit group must be checked,
    // not only the first one of each group.
    EXPECT_FALSE(is_valid_timestamp("20x6-10-01T12:00:00Z"));
    EXPECT_FALSE(is_valid_timestamp("2026-10-01T12:00:0xZ"));
}