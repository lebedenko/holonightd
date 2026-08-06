// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/NotificationBridge.h"

#include "holonightd/Logger.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace holonightd::agent {

namespace {

std::string formatProviderName(const std::string& provider) {
  if (provider == "kiro") {
    return "Kiro CLI";
  }
  if (provider == "claude") {
    return "Claude Code";
  }
  if (provider == "codex") {
    return "Codex";
  }
  if (provider == "antigravity") {
    return "Antigravity";
  }
  if (provider.empty()) {
    return "Agent";
  }
  std::string result = provider;
  if (!result.empty()) {
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
  }
  return result;
}

std::string resolveProviderIcon(const std::string& provider) {
  const char* home_dir = std::getenv("HOME");
  if (home_dir != nullptr && !provider.empty()) {
    std::filesystem::path icon_dir = std::filesystem::path(home_dir) / ".local/share/icons/holonight";
    for (const auto& ext : {".png", ".svg", ".jpg"}) {
      auto icon_path = icon_dir / (provider + ext);
      std::error_code err_code;
      if (std::filesystem::exists(icon_path, err_code)) {
        return icon_path.string();
      }
    }
  }

  if (provider == "claude") {
    return "claude-code";
  }
  if (provider == "kiro") {
    return "kiro";
  }
  if (provider == "codex") {
    return "codex";
  }
  if (provider == "antigravity") {
    return "google-antigravity";
  }
  if (!provider.empty()) {
    return provider;
  }
  return "utilities-terminal";
}

std::string truncateMessage(std::string_view message, std::size_t max_len = 200) {
  if (message.empty()) {
    return "";
  }
  std::string cleaned;
  cleaned.reserve(std::min(message.length(), max_len + 5));

  bool last_was_space = false;
  for (char char_item : message) {
    if (char_item == '\n' || char_item == '\r' || char_item == '\t') {
      char_item = ' ';
    }
    if (char_item == ' ') {
      if (!last_was_space && !cleaned.empty()) {
        cleaned.push_back(' ');
        last_was_space = true;
      }
    } else {
      cleaned.push_back(char_item);
      last_was_space = false;
    }
    if (cleaned.length() >= max_len) {
      break;
    }
  }

  if (message.length() > max_len || cleaned.length() >= max_len) {
    if (cleaned.length() > max_len - 3) {
      cleaned.resize(max_len - 3);
    }
    cleaned += "...";
  }
  return cleaned;
}

}  // namespace

std::uint32_t NotificationBridge::sendNotification(const AgentSession& session, const AgentEvent& event,
                                                   std::uint32_t replaces_id) {
  holonightd::Logger logger(holonightd::LogLevel::Info);
#ifndef HOLONIGHTD_HAS_SYSTEMD
  // Fallback stdout log if systemd sd-bus is unavailable
  logger.info("Desktop Notification [" + session.provider + " - " + session.project_name + "]: " + event.title + " - " +
              event.message);
  return replaces_id;
#else
  sd_bus* bus_ptr = nullptr;
  if (sd_bus_open_user(&bus_ptr) < 0) {
    logger.warn("Could not connect to user D-Bus for desktop notification");
    return replaces_id;
  }
  std::unique_ptr<sd_bus, void (*)(sd_bus*)> bus_guard(bus_ptr, [](sd_bus* b_handle) {
    if (b_handle != nullptr) {
      sd_bus_unref(b_handle);
    }
  });

  sd_bus_message* msg_ptr = nullptr;
  int ret_code =
      sd_bus_message_new_method_call(bus_ptr, &msg_ptr, "org.freedesktop.Notifications",
                                     "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
  if (ret_code < 0) {
    return replaces_id;
  }
  std::unique_ptr<sd_bus_message, void (*)(sd_bus_message*)> msg_guard(msg_ptr, [](sd_bus_message* m_handle) {
    if (m_handle != nullptr) {
      sd_bus_message_unref(m_handle);
    }
  });

  std::string provider_display = formatProviderName(session.provider);
  std::string app_name = "HoloNight Agent (" + provider_display + ")";
  std::string app_icon = resolveProviderIcon(session.provider);

  std::string project_str = session.project_name.empty() ? "Global" : session.project_name;
  std::string title_str = event.title.empty() ? toString(event.state) : event.title;

  std::string summary = provider_display + " · " + project_str + " — " + title_str;
  std::string body = event.message.empty() ? toString(event.state) : truncateMessage(event.message, 200);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "susss", app_name.c_str(), replaces_id, app_icon.c_str(), summary.c_str(),
                        body.c_str());

  sd_bus_message_open_container(msg_ptr, 'a', "s");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "s", "open");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "s", "Open terminal");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "s", "dismiss");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "s", "Dismiss");
  sd_bus_message_close_container(msg_ptr);

  sd_bus_message_open_container(msg_ptr, 'a', "{sv}");
  sd_bus_message_close_container(msg_ptr);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_append(msg_ptr, "i", -1);

  sd_bus_message* reply_ptr = nullptr;
  sd_bus_error bus_error = SD_BUS_ERROR_NULL;
  ret_code = sd_bus_call(bus_ptr, msg_ptr, 0, &bus_error, &reply_ptr);
  if (ret_code < 0) {
    sd_bus_error_free(&bus_error);
    return replaces_id;
  }
  std::unique_ptr<sd_bus_message, void (*)(sd_bus_message*)> reply_guard(reply_ptr, [](sd_bus_message* m_handle) {
    if (m_handle != nullptr) {
      sd_bus_message_unref(m_handle);
    }
  });

  std::uint32_t assigned_id = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  sd_bus_message_read(reply_ptr, "u", &assigned_id);
  sd_bus_error_free(&bus_error);
  return assigned_id;
#endif
}

}  // namespace holonightd::agent
