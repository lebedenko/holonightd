#include "holonightd/ObservationEvent.h"

// NOLINTBEGIN
#include <nlohmann/json.hpp>
// NOLINTEND

#include <chrono>
#include <format>
#include <sstream>

namespace holonightd {

using json = nlohmann::json;

std::string severityToString(Severity severity) {
  switch (severity) {
    case Severity::Debug:
      return "debug";
    case Severity::Info:
      return "info";
    case Severity::Warning:
      return "warning";
    case Severity::Error:
      return "error";
    case Severity::Critical:
      return "critical";
  }
  return "info";
}

std::expected<Severity, std::string> severityFromString(std::string_view str) {
  if (str == "debug") {
    return Severity::Debug;
  }
  if (str == "info") {
    return Severity::Info;
  }
  if (str == "warning") {
    return Severity::Warning;
  }
  if (str == "error") {
    return Severity::Error;
  }
  if (str == "critical") {
    return Severity::Critical;
  }
  return std::unexpected(std::string("Unknown severity level: ") + std::string(str));
}

std::string ObservationEvent::toJson() const {
  json json_out;
  if (id != 0) {
    json_out["id"] = id;
  }

  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count();
  json_out["timestamp_us"] = micros;
  json_out["source"] = source;
  json_out["category"] = category;
  json_out["subject"] = subject;
  json_out["signal"] = signal;
  json_out["severity"] = severityToString(severity);

  std::visit(
      [&json_out](const auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          json_out["value"] = nullptr;
        } else {
          json_out["value"] = arg;
        }
      },
      value);

  try {
    json_out["attributes"] = json::parse(attributes_json.empty() ? "{}" : attributes_json);
  } catch (...) {
    json_out["attributes"] = json::object();
  }

  return json_out.dump();
}

static void parseEventStrings(const json& json_obj, ObservationEvent& event) {
  if (json_obj.contains("source") && json_obj["source"].is_string()) {
    event.source = json_obj["source"].get<std::string>();
  }
  if (json_obj.contains("category") && json_obj["category"].is_string()) {
    event.category = json_obj["category"].get<std::string>();
  }
  if (json_obj.contains("subject") && json_obj["subject"].is_string()) {
    event.subject = json_obj["subject"].get<std::string>();
  }
  if (json_obj.contains("signal") && json_obj["signal"].is_string()) {
    event.signal = json_obj["signal"].get<std::string>();
  }
}

static void parseEventValue(const json& json_obj, ObservationEvent& event) {
  if (!json_obj.contains("value")) {
    return;
  }
  const auto& val = json_obj["value"];
  if (val.is_boolean()) {
    event.value = val.get<bool>();
  } else if (val.is_number_integer()) {
    event.value = val.get<int64_t>();
  } else if (val.is_number_float()) {
    event.value = val.get<double>();
  } else if (val.is_string()) {
    event.value = val.get<std::string>();
  } else {
    event.value = std::monostate{};
  }
}

std::expected<ObservationEvent, std::string> ObservationEvent::fromJson(std::string_view json_str) {
  try {
    const auto json_obj = json::parse(json_str);
    ObservationEvent event;

    if (json_obj.contains("id") && json_obj["id"].is_number_integer()) {
      event.id = json_obj["id"].get<int64_t>();
    }

    if (json_obj.contains("timestamp_us") && json_obj["timestamp_us"].is_number()) {
      const auto timestamp_micros = json_obj["timestamp_us"].get<int64_t>();
      event.timestamp = std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp_micros));
    }

    parseEventStrings(json_obj, event);

    if (json_obj.contains("severity") && json_obj["severity"].is_string()) {
      const auto sev_res = severityFromString(json_obj["severity"].get<std::string>());
      if (sev_res.has_value()) {
        event.severity = sev_res.value();
      }
    }

    parseEventValue(json_obj, event);

    if (json_obj.contains("attributes") && json_obj["attributes"].is_object()) {
      event.attributes_json = json_obj["attributes"].dump();
    } else {
      event.attributes_json = "{}";
    }

    return event;
  } catch (const std::exception& ex) {
    return std::unexpected(std::string("Failed to parse ObservationEvent JSON: ") + ex.what());
  }
}

}  // namespace holonightd
