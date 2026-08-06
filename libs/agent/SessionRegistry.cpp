// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/SessionRegistry.h"

#include <filesystem>
#include <mutex>

namespace holonightd::agent {

namespace {

std::string extractProjectName(const std::string& path_str) {
  std::string target = path_str;
  if (target.empty()) {
    std::error_code err_code;
    auto current_p = std::filesystem::current_path(err_code);
    if (!err_code) {
      target = current_p.string();
    }
  }

  if (target.empty()) {
    return "Global";
  }

  std::filesystem::path path_obj(target);
  if (path_obj.has_filename()) {
    return path_obj.filename().string();
  }
  return target;
}

}  // namespace

std::string SessionRegistry::registerSession(const std::string& provider, const std::string& session_id,
                                             std::uint32_t pid, const std::string& cwd,
                                             const nlohmann::json& metadata) {
  std::scoped_lock lock(mutex_);
  std::string assigned_id = session_id.empty() ? (provider + "-" + std::to_string(pid)) : session_id;

  AgentSession session;
  session.session_id = assigned_id;
  session.provider = provider;
  session.project_path = cwd;
  session.project_name = extractProjectName(cwd);
  session.pid = pid;
  session.current_state = AgentState::Starting;
  session.start_time = std::chrono::system_clock::now();
  session.last_update_time = session.start_time;

  if (metadata.contains("window_address") && metadata["window_address"].is_string()) {
    session.window_address = metadata["window_address"].get<std::string>();
  }

  sessions_[assigned_id] = session;
  return assigned_id;
}

bool SessionRegistry::publishEvent(const AgentEvent& event) {
  std::scoped_lock lock(mutex_);
  auto iter = sessions_.find(event.session_id);
  if (iter == sessions_.end()) {
    // Auto-register session if missing
    AgentSession session;
    session.session_id = event.session_id;
    session.provider = event.provider;
    session.project_path = event.project_path;
    session.project_name = extractProjectName(event.project_path);
    session.current_state = event.state;
    session.last_title = event.title;
    session.last_message = event.message;
    session.start_time = std::chrono::system_clock::now();
    session.last_update_time = session.start_time;
    sessions_[event.session_id] = session;
    return true;
  }

  iter->second.current_state = event.state;
  if (!event.provider.empty()) {
    iter->second.provider = event.provider;
  }
  if (!event.project_path.empty()) {
    iter->second.project_path = event.project_path;
    iter->second.project_name = extractProjectName(event.project_path);
  }
  if (!event.title.empty()) {
    iter->second.last_title = event.title;
  }
  if (!event.message.empty()) {
    iter->second.last_message = event.message;
  }
  iter->second.last_update_time = std::chrono::system_clock::now();
  return true;
}

bool SessionRegistry::endSession(const std::string& session_id, const std::string& /*result*/) {
  std::scoped_lock lock(mutex_);
  return sessions_.erase(session_id) > 0;
}

std::optional<AgentSession> SessionRegistry::getSession(const std::string& session_id) const {
  std::scoped_lock lock(mutex_);
  auto iter = sessions_.find(session_id);
  if (iter != sessions_.end()) {
    return iter->second;
  }
  return std::nullopt;
}

std::vector<AgentSession> SessionRegistry::listSessions() const {
  std::scoped_lock lock(mutex_);
  std::vector<AgentSession> list;
  list.reserve(sessions_.size());
  for (const auto& [sess_id, sess] : sessions_) {
    list.push_back(sess);
  }
  return list;
}

void SessionRegistry::updateNotificationId(const std::string& session_id, std::uint32_t notif_id) {
  std::scoped_lock lock(mutex_);
  auto iter = sessions_.find(session_id);
  if (iter != sessions_.end()) {
    iter->second.notification_id = notif_id;
  }
}

void SessionRegistry::pruneStaleSessions(std::chrono::seconds max_age) {
  std::scoped_lock lock(mutex_);
  auto now = std::chrono::system_clock::now();
  for (auto iter = sessions_.begin(); iter != sessions_.end();) {
    if (now - iter->second.last_update_time > max_age) {
      iter = sessions_.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace holonightd::agent
