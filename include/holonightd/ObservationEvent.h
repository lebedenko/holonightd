#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace holonightd {

/// Severity levels for observation events.
enum class Severity : std::uint8_t { Debug, Info, Warning, Error, Critical };

/// Converts a Severity enum value to a lowercase string representation.
[[nodiscard]] std::string severityToString(Severity severity);

/// Parses a lowercase string representation into a Severity enum value.
[[nodiscard]] std::expected<Severity, std::string> severityFromString(std::string_view str);

/// Payload value carried by an observation event.
using EventValue = std::variant<std::monostate, bool, int64_t, double, std::string>;

/// Represents a normalized diagnostic observation event emitted by system collectors.
struct ObservationEvent {
  int64_t id{0};
  std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
  std::string source;
  std::string category;
  std::string subject;
  std::string signal;
  EventValue value;
  Severity severity{Severity::Info};
  std::string attributes_json{"{}"};

  /// Serializes the event to a JSON string.
  [[nodiscard]] std::string toJson() const;

  /// Deserializes an event from a JSON string.
  [[nodiscard]] static std::expected<ObservationEvent, std::string> fromJson(std::string_view json_str);
};

/// Criteria for filtering observation events from the EventStore.
struct EventQuery {
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
  std::optional<std::string> source;
  std::optional<std::string> category;
  std::optional<std::string> subject;
  std::optional<Severity> min_severity;
  std::optional<size_t> limit;
};

}  // namespace holonightd
