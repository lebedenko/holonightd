#include "holonightd/EventStore.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace holonightd {

class EventStoreTest : public ::testing::Test {
 protected:
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  std::filesystem::path test_db_path;

  void SetUp() override {
    test_db_path =
        std::filesystem::temp_directory_path() /
        ("holonightd_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".db");
    if (std::filesystem::exists(test_db_path)) {
      std::filesystem::remove(test_db_path);
    }
  }

  void TearDown() override {
    if (std::filesystem::exists(test_db_path)) {
      std::filesystem::remove(test_db_path);
    }
    std::filesystem::remove(test_db_path.string() + "-wal");
    std::filesystem::remove(test_db_path.string() + "-shm");
  }
};

TEST_F(EventStoreTest, InsertsAndQueriesSingleEvent) {
  EventStore store(test_db_path);

  ObservationEvent event;
  event.source = "statvfs";
  event.category = "storage";
  event.subject = "/";
  event.signal = "space_pressure";
  event.severity = Severity::Warning;
  event.value = 96.8;
  event.attributes_json = R"({"used_percent":96.8})";

  const auto insert_res = store.insert(event);
  ASSERT_TRUE(insert_res.has_value());
  EXPECT_GT(insert_res.value(), 0);

  EventQuery filter;
  filter.source = "statvfs";

  const auto query_res = store.query(filter);
  ASSERT_TRUE(query_res.has_value());
  ASSERT_EQ(query_res->size(), 1U);

  const auto& retrieved = query_res->front();
  EXPECT_EQ(retrieved.source, "statvfs");
  EXPECT_EQ(retrieved.category, "storage");
  EXPECT_EQ(retrieved.subject, "/");
  EXPECT_EQ(retrieved.signal, "space_pressure");
  EXPECT_EQ(retrieved.severity, Severity::Warning);
  EXPECT_DOUBLE_EQ(std::get<double>(retrieved.value), 96.8);
}

TEST_F(EventStoreTest, InsertBatchAndQueryLimits) {
  EventStore store(test_db_path);

  std::vector<ObservationEvent> batch;
  for (int i = 0; i < 5; ++i) {
    ObservationEvent event;
    event.source = "systemd";
    event.category = "service";
    event.subject = "service_" + std::to_string(i);
    event.signal = "unit_failed";
    event.severity = (i % 2 == 0) ? Severity::Error : Severity::Info;
    batch.push_back(event);
  }

  const auto batch_res = store.insertBatch(batch);
  ASSERT_TRUE(batch_res.has_value());
  EXPECT_EQ(batch_res.value(), 5U);

  EventQuery filter;
  filter.source = "systemd";
  filter.limit = 3;

  const auto query_res = store.query(filter);
  ASSERT_TRUE(query_res.has_value());
  EXPECT_EQ(query_res->size(), 3U);
}

TEST_F(EventStoreTest, AppliesSeverityFilterBeforeLimit) {
  EventStore store(test_db_path);
  const auto now = std::chrono::system_clock::now();

  ObservationEvent error_event;
  error_event.timestamp = now;
  error_event.source = "systemd";
  error_event.category = "service";
  error_event.subject = "failed.service";
  error_event.signal = "unit_failed";
  error_event.severity = Severity::Error;
  ASSERT_TRUE(store.insert(error_event).has_value());

  for (int index = 1; index <= 3; ++index) {
    ObservationEvent info_event = error_event;
    info_event.timestamp = now + std::chrono::seconds(index);
    info_event.subject = "healthy_" + std::to_string(index) + ".service";
    info_event.severity = Severity::Info;
    ASSERT_TRUE(store.insert(info_event).has_value());
  }

  EventQuery filter;
  filter.min_severity = Severity::Error;
  filter.limit = 1;
  const auto result = store.query(filter);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ(result->front().subject, "failed.service");
}

TEST_F(EventStoreTest, PrunesOldEvents) {
  EventStore store(test_db_path);

  const auto now = std::chrono::system_clock::now();
  const auto old_time = now - std::chrono::hours(48);

  ObservationEvent old_event;
  old_event.timestamp = old_time;
  old_event.source = "kernel";
  old_event.category = "memory";
  old_event.subject = "system";
  old_event.signal = "oom_kill";

  ObservationEvent new_event;
  new_event.timestamp = now;
  new_event.source = "kernel";
  new_event.category = "memory";
  new_event.subject = "system";
  new_event.signal = "oom_kill";

  ASSERT_TRUE(store.insert(old_event).has_value());
  ASSERT_TRUE(store.insert(new_event).has_value());

  const auto cutoff = now - std::chrono::hours(24);
  const auto prune_res = store.pruneEvents(cutoff);

  ASSERT_TRUE(prune_res.has_value());
  EXPECT_EQ(prune_res.value(), 1U);

  const auto remaining = store.query(EventQuery{});
  ASSERT_TRUE(remaining.has_value());
  ASSERT_EQ(remaining->size(), 1U);
  EXPECT_EQ(std::chrono::time_point_cast<std::chrono::microseconds>(remaining->front().timestamp),
            std::chrono::time_point_cast<std::chrono::microseconds>(now));
}

TEST_F(EventStoreTest, PrunesEventsByAge) {
  EventStore store(test_db_path);

  const auto now = std::chrono::system_clock::now();
  const auto old_time = now - std::chrono::hours(48);

  ObservationEvent old_event;
  old_event.timestamp = old_time;
  old_event.source = "kernel";
  old_event.category = "memory";
  old_event.subject = "system";
  old_event.signal = "oom_kill";

  ObservationEvent new_event;
  new_event.timestamp = now;
  new_event.source = "kernel";
  new_event.category = "memory";
  new_event.subject = "system";
  new_event.signal = "oom_kill";

  ASSERT_TRUE(store.insert(old_event).has_value());
  ASSERT_TRUE(store.insert(new_event).has_value());

  const auto cutoff = now - std::chrono::hours(24);
  const auto prune_res = store.pruneEventsByAge(cutoff);

  ASSERT_TRUE(prune_res.has_value());
  EXPECT_EQ(prune_res.value(), 1U);

  const auto remaining = store.query(EventQuery{});
  ASSERT_TRUE(remaining.has_value());
  ASSERT_EQ(remaining->size(), 1U);
}

TEST_F(EventStoreTest, PrunesEventsByCapacityCountAndSeverityPreservation) {
  EventStore store(test_db_path);

  const auto now = std::chrono::system_clock::now();
  for (int i = 0; i < 8; ++i) {
    ObservationEvent event;
    event.timestamp = now + std::chrono::seconds(i);
    event.source = "test";
    event.category = "debug";
    event.subject = "sub";
    event.signal = "sig";
    event.severity = Severity::Debug;
    ASSERT_TRUE(store.insert(event).has_value());
  }
  for (int i = 0; i < 2; ++i) {
    ObservationEvent event;
    event.timestamp = now + std::chrono::seconds(10 + i);
    event.source = "test";
    event.category = "error";
    event.subject = "sub";
    event.signal = "sig";
    event.severity = Severity::Error;
    ASSERT_TRUE(store.insert(event).has_value());
  }

  const auto prune_res = store.pruneEventsByCapacity(0, 8);
  ASSERT_TRUE(prune_res.has_value());
  EXPECT_EQ(prune_res.value(), 3U);

  const auto remaining = store.query(EventQuery{});
  ASSERT_TRUE(remaining.has_value());
  ASSERT_EQ(remaining->size(), 7U);

  int error_count = 0;
  for (const auto& event_item : *remaining) {
    if (event_item.severity == Severity::Error) {
      ++error_count;
    }
  }
  EXPECT_EQ(error_count, 2);
}

TEST_F(EventStoreTest, CheckpointsWal) {
  EventStore store(test_db_path);

  ObservationEvent event;
  event.source = "test";
  event.category = "cat";
  event.subject = "sub";
  event.signal = "sig";
  ASSERT_TRUE(store.insert(event).has_value());

  const auto checkpoint_res = store.checkpointWal();
  EXPECT_TRUE(checkpoint_res.has_value());
}

TEST_F(EventStoreTest, ExportsToJsonlAtomically) {
  EventStore store(test_db_path);

  ObservationEvent event1;
  event1.source = "sys";
  event1.category = "cat1";
  event1.subject = "sub1";
  event1.signal = "sig1";
  event1.severity = Severity::Info;

  ObservationEvent event2;
  event2.source = "sys";
  event2.category = "cat2";
  event2.subject = "sub2";
  event2.signal = "sig2";
  event2.severity = Severity::Error;

  ASSERT_TRUE(store.insert(event1).has_value());
  ASSERT_TRUE(store.insert(event2).has_value());

  const auto export_path = std::filesystem::temp_directory_path() / "holonightd_export_test.jsonl";
  if (std::filesystem::exists(export_path)) {
    std::filesystem::remove(export_path);
  }

  EventQuery filter;
  filter.source = "sys";

  const auto export_res = store.exportToJsonl(export_path, filter);
  ASSERT_TRUE(export_res.has_value());
  EXPECT_EQ(export_res.value(), 2U);

  EXPECT_TRUE(std::filesystem::exists(export_path));
  EXPECT_FALSE(std::filesystem::exists(export_path.string() + ".tmp"));

  std::ifstream file_stream{export_path};
  std::string line;
  int line_count = 0;
  while (std::getline(file_stream, line)) {
    if (!line.empty()) {
      const auto json_res = ObservationEvent::fromJson(line);
      EXPECT_TRUE(json_res.has_value());
      ++line_count;
    }
  }
  EXPECT_EQ(line_count, 2);

  std::filesystem::remove(export_path);
}

}  // namespace holonightd
