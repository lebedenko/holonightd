#include "holonightd/EventStore.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace holonightd {

struct EventStore::Impl {
  sqlite3* db{nullptr};
  mutable std::mutex db_mutex;

  explicit Impl(const std::filesystem::path& db_path) {
    if (db_path.has_parent_path()) {
      std::filesystem::create_directories(db_path.parent_path());
    }

    const int open_result = sqlite3_open(db_path.c_str(), &db);
    if (open_result != SQLITE_OK) {
      std::string err_msg = (db != nullptr) ? sqlite3_errmsg(db) : "Unknown SQLite error";
      if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
      }
      throw std::runtime_error("Failed to open SQLite database at " + db_path.string() + ": " + err_msg);
    }

    execPragmasAndSchema();
  }

  ~Impl() {
    if (db != nullptr) {
      sqlite3_close(db);
      db = nullptr;
    }
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) noexcept = delete;
  Impl& operator=(Impl&&) noexcept = delete;

  void execPragmasAndSchema() const {
    char* err_msg = nullptr;
    const char* sql = R"(
            PRAGMA journal_mode=WAL;
            PRAGMA busy_timeout=5000;
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
            CREATE INDEX IF NOT EXISTS idx_severity_timestamp ON observation_events(severity, timestamp_us);
        )";

    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      std::string err = (err_msg != nullptr) ? err_msg : "Schema error";
      sqlite3_free(err_msg);
      throw std::runtime_error("Failed to initialize database schema: " + err);
    }
  }

  [[nodiscard]] size_t getCount() const {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM observation_events;", -1, &stmt, nullptr) != SQLITE_OK) {
      return 0;
    }
    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const auto row_cnt = sqlite3_column_int64(stmt, 0);
      count = (row_cnt > 0) ? static_cast<size_t>(row_cnt) : 0;
    }
    sqlite3_finalize(stmt);
    return count;
  }

  [[nodiscard]] size_t getSizeBytes() const {
    sqlite3_stmt* stmt = nullptr;
    size_t page_count = 0;
    size_t page_size = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA page_count;", -1, &stmt, nullptr) == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto page_cnt = sqlite3_column_int64(stmt, 0);
        page_count = (page_cnt > 0) ? static_cast<size_t>(page_cnt) : 0;
      }
      sqlite3_finalize(stmt);
    }
    if (sqlite3_prepare_v2(db, "PRAGMA page_size;", -1, &stmt, nullptr) == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto page_sz = sqlite3_column_int64(stmt, 0);
        page_size = (page_sz > 0) ? static_cast<size_t>(page_sz) : 0;
      }
      sqlite3_finalize(stmt);
    }
    return page_count * page_size;
  }
};

EventStore::EventStore(const std::filesystem::path& db_path) : pimpl_(std::make_unique<Impl>(db_path)) {}

EventStore::~EventStore() = default;
EventStore::EventStore(EventStore&&) noexcept = default;
EventStore& EventStore::operator=(EventStore&&) noexcept = default;

static std::string readColumnString(sqlite3_stmt* stmt, int col) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
  return (ptr != nullptr) ? std::string(ptr) : std::string();
}

static EventValue parseValuePayload(const std::string& val_type, const std::string& val_text) {
  if (val_type == "bool") {
    return (val_text == "1" || val_text == "true");
  }
  if (val_type == "int") {
    try {
      return std::stoll(val_text);
    } catch (...) {
      return int64_t{0};
    }
  }
  if (val_type == "double") {
    try {
      return std::stod(val_text);
    } catch (...) {
      return 0.0;
    }
  }
  if (val_type == "string") {
    return val_text;
  }
  return std::monostate{};
}

static ObservationEvent parseRow(sqlite3_stmt* stmt) {
  ObservationEvent event;
  event.id = sqlite3_column_int64(stmt, 0);

  const auto timestamp_us = sqlite3_column_int64(stmt, 1);
  event.timestamp = std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp_us));

  event.source = readColumnString(stmt, 2);
  event.category = readColumnString(stmt, 3);
  event.subject = readColumnString(stmt, 4);
  event.signal = readColumnString(stmt, 5);

  const std::string val_type = readColumnString(stmt, 6);
  const std::string val_text = readColumnString(stmt, 7);
  event.value = parseValuePayload(val_type, val_text);

  const std::string sev_str = readColumnString(stmt, 8);
  const auto sev_res = severityFromString(sev_str);
  event.severity = sev_res.value_or(Severity::Info);

  event.attributes_json = readColumnString(stmt, 9);
  if (event.attributes_json.empty()) {
    event.attributes_json = "{}";
  }

  return event;
}

std::expected<int64_t, std::string> EventStore::insert(const ObservationEvent& event) {
  const std::scoped_lock lock(pimpl_->db_mutex);

  const char* sql = R"(
        INSERT INTO observation_events 
        (timestamp_us, source, category, subject, signal, value_type, value_text, severity, attributes_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(pimpl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(std::string("Failed to prepare INSERT statement: ") + sqlite3_errmsg(pimpl_->db));
  }

  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(event.timestamp.time_since_epoch()).count();

  std::string val_type = "none";
  std::string val_text;

  std::visit(
      [&val_type, &val_text](const auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
          val_type = "bool";
          val_text = arg ? "1" : "0";
        } else if constexpr (std::is_same_v<T, int64_t>) {
          val_type = "int";
          val_text = std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
          val_type = "double";
          val_text = std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
          val_type = "string";
          val_text = arg;
        }
      },
      event.value);

  const std::string sev_str = severityToString(event.severity);

  sqlite3_bind_int64(stmt, 1, micros);
  sqlite3_bind_text(stmt, 2, event.source.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, event.category.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, event.subject.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, event.signal.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, val_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, val_text.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, sev_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, event.attributes_json.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string err = sqlite3_errmsg(pimpl_->db);
    sqlite3_finalize(stmt);
    return std::unexpected("Failed to execute INSERT: " + err);
  }

  sqlite3_finalize(stmt);
  return sqlite3_last_insert_rowid(pimpl_->db);
}

std::expected<size_t, std::string> EventStore::insertBatch(const std::vector<ObservationEvent>& events) {
  const std::scoped_lock lock(pimpl_->db_mutex);

  char* err_msg = nullptr;
  if (sqlite3_exec(pimpl_->db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string err = (err_msg != nullptr) ? err_msg : "Transaction start error";
    sqlite3_free(err_msg);
    return std::unexpected("Failed to begin transaction: " + err);
  }

  const char* sql = R"(
        INSERT INTO observation_events 
        (timestamp_us, source, category, subject, signal, value_type, value_text, severity, attributes_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(pimpl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(pimpl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return std::unexpected(std::string("Failed to prepare INSERT statement: ") + sqlite3_errmsg(pimpl_->db));
  }

  size_t inserted_count = 0;
  for (const auto& event : events) {
    const auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(event.timestamp.time_since_epoch()).count();

    std::string val_type = "none";
    std::string val_text;

    std::visit(
        [&val_type, &val_text](const auto& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, bool>) {
            val_type = "bool";
            val_text = arg ? "1" : "0";
          } else if constexpr (std::is_same_v<T, int64_t>) {
            val_type = "int";
            val_text = std::to_string(arg);
          } else if constexpr (std::is_same_v<T, double>) {
            val_type = "double";
            val_text = std::to_string(arg);
          } else if constexpr (std::is_same_v<T, std::string>) {
            val_type = "string";
            val_text = arg;
          }
        },
        event.value);

    const std::string sev_str = severityToString(event.severity);

    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, micros);
    sqlite3_bind_text(stmt, 2, event.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, event.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, event.subject.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, event.signal.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, val_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, val_text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, sev_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, event.attributes_json.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      sqlite3_exec(pimpl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
      return std::unexpected(std::string("Failed batch insert step: ") + sqlite3_errmsg(pimpl_->db));
    }
    ++inserted_count;
  }

  sqlite3_finalize(stmt);

  if (sqlite3_exec(pimpl_->db, "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string err = (err_msg != nullptr) ? err_msg : "Commit error";
    sqlite3_free(err_msg);
    return std::unexpected("Failed to commit transaction: " + err);
  }

  return inserted_count;
}

std::expected<std::vector<ObservationEvent>, std::string> EventStore::query(const EventQuery& filter) const {
  const std::scoped_lock lock(pimpl_->db_mutex);

  std::string sql = R"(
        SELECT id, timestamp_us, source, category, subject, signal, value_type, value_text, severity, attributes_json
        FROM observation_events WHERE 1=1
    )";

  if (filter.start_time.has_value()) {
    sql += " AND timestamp_us >= ?";
  }
  if (filter.end_time.has_value()) {
    sql += " AND timestamp_us <= ?";
  }
  if (filter.source.has_value()) {
    sql += " AND source = ?";
  }
  if (filter.category.has_value()) {
    sql += " AND category = ?";
  }
  if (filter.subject.has_value()) {
    sql += " AND subject = ?";
  }
  if (filter.min_severity.has_value()) {
    sql += R"( AND CASE severity
      WHEN 'debug' THEN 0
      WHEN 'info' THEN 1
      WHEN 'warning' THEN 2
      WHEN 'error' THEN 3
      WHEN 'critical' THEN 4
      ELSE 1 END >= ?)";
  }

  sql += " ORDER BY timestamp_us DESC";

  if (filter.limit.has_value()) {
    sql += " LIMIT ?";
  }

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(pimpl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(std::string("Failed to prepare query statement: ") + sqlite3_errmsg(pimpl_->db));
  }

  int bind_idx = 1;
  if (filter.start_time.has_value()) {
    const auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(filter.start_time->time_since_epoch()).count();
    sqlite3_bind_int64(stmt, bind_idx++, micros);
  }
  if (filter.end_time.has_value()) {
    const auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(filter.end_time->time_since_epoch()).count();
    sqlite3_bind_int64(stmt, bind_idx++, micros);
  }
  if (filter.source.has_value()) {
    sqlite3_bind_text(stmt, bind_idx++, filter.source->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (filter.category.has_value()) {
    sqlite3_bind_text(stmt, bind_idx++, filter.category->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (filter.subject.has_value()) {
    sqlite3_bind_text(stmt, bind_idx++, filter.subject->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (filter.min_severity.has_value()) {
    sqlite3_bind_int(stmt, bind_idx++, static_cast<int>(*filter.min_severity));
  }
  if (filter.limit.has_value()) {
    sqlite3_bind_int64(stmt, bind_idx++, static_cast<int64_t>(*filter.limit));
  }

  std::vector<ObservationEvent> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto event = parseRow(stmt);
    results.push_back(std::move(event));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::expected<size_t, std::string> EventStore::pruneEvents(std::chrono::system_clock::time_point older_than) {
  const std::scoped_lock lock(pimpl_->db_mutex);

  const char* sql = "DELETE FROM observation_events WHERE timestamp_us < ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(pimpl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(std::string("Failed to prepare DELETE statement: ") + sqlite3_errmsg(pimpl_->db));
  }

  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(older_than.time_since_epoch()).count();
  sqlite3_bind_int64(stmt, 1, micros);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string err = sqlite3_errmsg(pimpl_->db);
    sqlite3_finalize(stmt);
    return std::unexpected("Failed to prune events: " + err);
  }

  sqlite3_finalize(stmt);
  return static_cast<size_t>(sqlite3_changes(pimpl_->db));
}

std::expected<size_t, std::string> EventStore::pruneEventsByAge(std::chrono::system_clock::time_point older_than) {
  return pruneEvents(older_than);
}

std::expected<size_t, std::string> EventStore::pruneEventsByCapacity(size_t max_bytes, size_t max_events) {
  const std::scoped_lock lock(pimpl_->db_mutex);

  size_t current_count = pimpl_->getCount();
  size_t current_bytes = pimpl_->getSizeBytes();

  const bool exceed_count = (max_events > 0 && current_count > max_events);
  const bool exceed_bytes = (max_bytes > 0 && current_bytes > max_bytes);

  if (!exceed_count && !exceed_bytes) {
    return 0;
  }

  const size_t target_count = (max_events > 0) ? (max_events * 9 / 10) : current_count;
  const size_t target_bytes = (max_bytes > 0) ? (max_bytes * 9 / 10) : current_bytes;

  size_t total_deleted = 0;

  const char* delete_sql = R"(
    DELETE FROM observation_events WHERE id IN (
      SELECT id FROM observation_events 
      ORDER BY CASE severity 
        WHEN 'debug' THEN 0 
        WHEN 'info' THEN 1 
        WHEN 'warning' THEN 2 
        WHEN 'error' THEN 3 
        WHEN 'critical' THEN 4 
        ELSE 1 END ASC, timestamp_us ASC 
      LIMIT ?
    );
  )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(pimpl_->db, delete_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::unexpected(std::string("Failed to prepare capacity prune statement: ") + sqlite3_errmsg(pimpl_->db));
  }

  while (true) {
    current_count = pimpl_->getCount();
    current_bytes = pimpl_->getSizeBytes();

    if (current_count == 0) {
      break;
    }

    size_t to_delete = 0;
    if (max_events > 0 && current_count > target_count) {
      to_delete = current_count - target_count;
    } else if (max_bytes > 0 && current_bytes > target_bytes) {
      to_delete = std::min<size_t>(100, current_count);
    }

    if (to_delete == 0) {
      break;
    }

    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(to_delete));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::string err = sqlite3_errmsg(pimpl_->db);
      sqlite3_finalize(stmt);
      return std::unexpected("Failed to execute capacity prune deletion: " + err);
    }

    const int changes = sqlite3_changes(pimpl_->db);
    if (changes == 0) {
      break;
    }
    total_deleted += static_cast<size_t>(changes);
  }

  sqlite3_finalize(stmt);
  return total_deleted;
}

std::expected<void, std::string> EventStore::checkpointWal() {
  const std::scoped_lock lock(pimpl_->db_mutex);

  char* err_msg = nullptr;
  if (sqlite3_exec(pimpl_->db, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string err = (err_msg != nullptr) ? err_msg : "WAL checkpoint error";
    sqlite3_free(err_msg);
    return std::unexpected("Failed WAL checkpoint: " + err);
  }

  return {};
}

std::expected<size_t, std::string> EventStore::exportToJsonl(const std::filesystem::path& dest_path,
                                                             const EventQuery& filter) const {
  auto events_res = query(filter);
  if (!events_res.has_value()) {
    return std::unexpected(events_res.error());
  }

  if (dest_path.has_parent_path()) {
    std::filesystem::create_directories(dest_path.parent_path());
  }

  const std::filesystem::path tmp_path = dest_path.string() + ".tmp";
  std::ofstream out{tmp_path, std::ios::out | std::ios::trunc};
  if (!out) {
    return std::unexpected("Failed to open temporary file for export: " + tmp_path.string());
  }

  size_t exported_count = 0;
  for (const auto& event : *events_res) {
    out << event.toJson() << "\n";
    if (!out) {
      out.close();
      std::filesystem::remove(tmp_path);
      return std::unexpected("Failed writing event JSON to " + tmp_path.string());
    }
    ++exported_count;
  }

  out.flush();
  out.close();
  if (!out) {
    std::filesystem::remove(tmp_path);
    return std::unexpected("Failed closing temporary export file " + tmp_path.string());
  }

  std::error_code rename_ec;
  std::filesystem::rename(tmp_path, dest_path, rename_ec);
  if (rename_ec) {
    std::filesystem::remove(tmp_path);
    return std::unexpected("Failed atomic rename to destination file " + dest_path.string() + ": " +
                           rename_ec.message());
  }

  return exported_count;
}

}  // namespace holonightd
