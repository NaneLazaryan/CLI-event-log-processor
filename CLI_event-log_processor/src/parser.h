#pragma once

#include <optional>
#include <string_view>
#include "event.h"

std::optional<Event> parse_event(const std::string& line);

bool is_valid_timestamp(std::string_view ts);