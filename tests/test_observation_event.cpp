#include "holonightd/ObservationEvent.h"

#include <gtest/gtest.h>

namespace holonightd {

TEST(ObservationEventTest, SeverityConversion) {
  EXPECT_EQ(severityToString(Severity::Debug), "debug");
  EXPECT_EQ(severityToString(Severity::Info), "info");
  EXPECT_EQ(severityToString(Severity::Warning), "warning");
  EXPECT_EQ(severityToString(Severity::Error), "error");
  EXPECT_EQ(severityToString(Severity::Critical), "critical");

  EXPECT_EQ(severityFromString("debug").value(), Severity::Debug);
  EXPECT_EQ(severityFromString("info").value(), Severity::Info);
  EXPECT_EQ(severityFromString("warning").value(), Severity::Warning);
  EXPECT_EQ(severityFromString("error").value(), Severity::Error);
  EXPECT_EQ(severityFromString("critical").value(), Severity::Critical);

  EXPECT_FALSE(severityFromString("unknown").has_value());
}

TEST(ObservationEventTest, JsonRoundTrip) {
  ObservationEvent event;
  event.id = 42;
  event.source = "systemd";
  event.category = "service";
  event.subject = "bluetooth.service";
  event.signal = "unit_failed";
  event.severity = Severity::Error;
  event.value = int64_t{1};
  event.attributes_json = R"({"exit_status":1,"restart_count":4})";

  const std::string json_str = event.toJson();
  const auto parsed_res = ObservationEvent::fromJson(json_str);

  ASSERT_TRUE(parsed_res.has_value());
  const auto& parsed = parsed_res.value();

  EXPECT_EQ(parsed.id, 42);
  EXPECT_EQ(parsed.source, "systemd");
  EXPECT_EQ(parsed.category, "service");
  EXPECT_EQ(parsed.subject, "bluetooth.service");
  EXPECT_EQ(parsed.signal, "unit_failed");
  EXPECT_EQ(parsed.severity, Severity::Error);
  EXPECT_EQ(std::get<int64_t>(parsed.value), 1);
}

TEST(ObservationEventTest, HandlesDifferentValueTypes) {
  ObservationEvent e_bool;
  e_bool.value = true;
  const auto res_bool = ObservationEvent::fromJson(e_bool.toJson());
  ASSERT_TRUE(res_bool.has_value());
  EXPECT_TRUE(std::get<bool>(res_bool->value));

  ObservationEvent e_double;
  e_double.value = 96.8;
  const auto res_double = ObservationEvent::fromJson(e_double.toJson());
  ASSERT_TRUE(res_double.has_value());
  EXPECT_DOUBLE_EQ(std::get<double>(res_double->value), 96.8);

  ObservationEvent e_str;
  e_str.value = std::string("oom");
  const auto res_str = ObservationEvent::fromJson(e_str.toJson());
  ASSERT_TRUE(res_str.has_value());
  EXPECT_EQ(std::get<std::string>(res_str->value), "oom");
}

TEST(ObservationEventTest, HandlesInvalidJson) {
  const auto res = ObservationEvent::fromJson("{invalid_json");
  EXPECT_FALSE(res.has_value());
}

}  // namespace holonightd
