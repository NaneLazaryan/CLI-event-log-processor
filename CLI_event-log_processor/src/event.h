#pragma once

#include <string>

struct Event
{
	std::string id;  // unique id
	std::string timestamp;  // format example: "2026-10-01T12:34:56Z"
	std::string type;  // "login", "logout", "purchase"
	std::string username;

	std::string date() const
	{
		// Get "YYYY-MM-DD" from timestamp
		return timestamp.substr(0, 10);
	}
};