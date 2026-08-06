# Technical Design: Observation Event Schema & Persistence Layer

**Feature ID:** `01-observation-event-schema`  
**Date:** 2026-07-30  
**Status:** Proposed  

---

## 1. Architectural Overview

The Observation Event subsystem forms the factual foundation of `holonightd`. Collectors across systemd, kernel, memory, storage, pacman, and desktop sessions normalize raw system telemetry into `ObservationEvent` instances. 

```
┌──────────────────────────────────────────────────────────────┐
│                    System Collectors                         │
│  systemd / journal / statvfs / kernel / pacman / pipewire    │
└──────────────────────────────┬───────────────────────────────┘
                               │ ObservationEvent
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                 ObservationEvent C++23 API                   │
│   ToJson() / FromJson() / Severity / EventValue (std::variant)│
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                    EventStore (SQLite3)                      │
│   insert() / insertBatch() / query(filter) / pruneEvents()   │
└──────────────────────────────┬───────────────────────────────┘
                               │ SQLite DB file
                               ▼
┌──────────────────────────────────────────────────────────────┐
│             ~/.local/share/holonight/events.db               │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Component Design & Interfaces

### 2.1 Observation Event Types (`include/holonightd/ObservationEvent.h`)

`ObservationEvent` represents a single factual observation from any collector.

```cpp
#pragma once

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace holonightd {

enum class Severity {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

[[nodiscard]] std::string severityToString(Severity severity);
[[nodiscard]] std::expected<Severity, std::string> severityFromString(std::string_view str);

using EventValue = std::variant<std::monostate, bool, int64_t, double, std::string>;

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

    [[nodiscard]] std::string toJson() const;
    [[nodiscard]] static std::expected<ObservationEvent, std::string> fromJson(std::string_view json_str);
};

struct EventQuery {
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<std::string> source;
    std::optional<std::string> category;
    std::optional<std::string> subject;
    std::optional<Severity> min_severity;
    std::optional<size_t> limit;
};

} // namespace holonightd
```

---

### 2.2 EventStore Interface (`include/holonightd/EventStore.h`)

The `EventStore` encapsulates SQLite operations behind RAII and `std::expected`.

```cpp
#pragma once

#include "holonightd/ObservationEvent.h"
#include <filesystem>
#include <memory>

namespace holonightd {

class EventStore {
public:
    explicit EventStore(const std::filesystem::path& db_path);
    ~EventStore();

    EventStore(const EventStore&) = delete;
    EventStore& operator=(const EventStore&) = delete;
    EventStore(EventStore&&) noexcept;
    EventStore& operator=(EventStore&&) noexcept;

    [[nodiscard]] std::expected<int64_t, std::string> insert(const ObservationEvent& event);
    [[nodiscard]] std::expected<size_t, std::string> insertBatch(const std::vector<ObservationEvent>& events);
    [[nodiscard]] std::expected<std::vector<ObservationEvent>, std::string> query(const EventQuery& filter) const;
    [[nodiscard]] std::expected<size_t, std::string> pruneEvents(std::chrono::system_clock::time_point older_than);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace holonightd
```

---

## 3. Database Schema Design

The SQLite database uses Write-Ahead Logging (WAL) for concurrency and fast transactions.

```sql
CREATE TABLE IF NOT EXISTS observation_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_us INTEGER NOT NULL,
    source TEXT NOT NULL,
    category TEXT NOT NULL,
    subject TEXT NOT NULL,
    signal TEXT NOT NULL,
    value_type TEXT NOT NULL,
    value_text TEXT,
    severity TEXT NOT NULL,
    attributes_json TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_events_timestamp ON observation_events(timestamp_us);
CREATE INDEX IF NOT EXISTS idx_events_source_cat ON observation_events(source, category);
CREATE INDEX IF NOT EXISTS idx_events_severity ON observation_events(severity);
```

### Column Mappings

| Column | Type | Description |
|---|---|---|
| `id` | `INTEGER PRIMARY KEY` | Auto-incrementing unique ID |
| `timestamp_us` | `INTEGER` | Microseconds since Unix epoch |
| `source` | `TEXT` | Collector name (`systemd`, `statvfs`, etc.) |
| `category` | `TEXT` | Domain category (`service`, `storage`, etc.) |
| `subject` | `TEXT` | Entity name (`bluetooth.service`, `/dev/nvme0n1`) |
| `signal` | `TEXT` | Signal identifier (`unit_failed`, `space_pressure`) |
| `value_type` | `TEXT` | Variant type (`none`, `bool`, `int`, `double`, `string`) |
| `value_text` | `TEXT` | String representation of `value` |
| `severity` | `TEXT` | Lowercase severity string (`info`, `error`, etc.) |
| `attributes_json` | `TEXT` | JSON string containing key-value metadata |

---

## 4. CMake & Dependencies

1. Add `find_package(SQLite3 REQUIRED)` or `target_link_libraries(holonightd_core PUBLIC SQLite::SQLite3)` in CMake configuration.
2. Add `nlohmann_json` (or single-header JSON parser in `third_party/nlohmann/json.hpp`) for JSON handling.

---

## 5. Error Handling & Edge Cases

- **File System / Permissions**: Parent directories for `events.db` (e.g. `~/.local/share/holonight/`) are created if missing.
- **Corrupted DB**: If SQLite opening fails, `EventStore` constructor or methods return explicit error messages via `std::expected`.
- **Invalid JSON**: Deserialization of invalid JSON returns `std::unexpected("invalid JSON format: ...")`.
