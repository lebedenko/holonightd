#pragma once

#include "holonightd/ObservationEvent.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace holonightd {

/// Thread-safe SQLite event store for storing and querying diagnostic observation events.
class EventStore {
 public:
  explicit EventStore(const std::filesystem::path& db_path);
  ~EventStore();

  EventStore(const EventStore&) = delete;
  EventStore& operator=(const EventStore&) = delete;
  EventStore(EventStore&&) noexcept;
  EventStore& operator=(EventStore&&) noexcept;

  /// Inserts a single observation event into the store.
  [[nodiscard]] std::expected<int64_t, std::string> insert(const ObservationEvent& event);

  /// Inserts a batch of observation events within a single database transaction.
  [[nodiscard]] std::expected<size_t, std::string> insertBatch(const std::vector<ObservationEvent>& events);

  /// Queries observation events using the specified filter criteria.
  [[nodiscard]] std::expected<std::vector<ObservationEvent>, std::string> query(const EventQuery& filter) const;

  /// Deletes observation events older than the specified time point.
  [[nodiscard]] std::expected<size_t, std::string> pruneEvents(std::chrono::system_clock::time_point older_than);

  /// Deletes observation events older than the specified time point.
  [[nodiscard]] std::expected<size_t, std::string> pruneEventsByAge(std::chrono::system_clock::time_point older_than);

  /// Prunes events down to 90% of capacity limits if thresholds are exceeded.
  /// Evicts lowest severity (Debug/Info) and oldest events first.
  [[nodiscard]] std::expected<size_t, std::string> pruneEventsByCapacity(size_t max_bytes, size_t max_events);

  /// Executes PRAGMA wal_checkpoint(TRUNCATE) to commit WAL to DB and truncate WAL file size.
  [[nodiscard]] std::expected<void, std::string> checkpointWal();

  /// Exports events matching the filter to a JSONL file atomically via temporary file staging.
  [[nodiscard]] std::expected<size_t, std::string> exportToJsonl(const std::filesystem::path& dest_path,
                                                                 const EventQuery& filter) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace holonightd
