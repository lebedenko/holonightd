// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/Logger.h"
#include "holonightd/agentd/AgentActivity.h"
#include "holonightd/agentd/NotificationBridge.h"
#include "holonightd/agentd/ProviderNormalizer.h"
#include "holonightd/agentd/SessionRegistry.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_running{true};

struct AppContext {
  holonightd::agent::SessionRegistry registry;
  holonightd::Logger logger{holonightd::LogLevel::Info};
};

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    g_running = false;
  }
}

#ifdef HOLONIGHTD_HAS_SYSTEMD
int methodRegisterSession(sd_bus_message* msg_ptr, void* userdata, sd_bus_error* /*ret_error*/) {
  const char* provider = nullptr;
  const char* session_id = nullptr;
  const char* cwd = nullptr;
  const char* metadata_json = nullptr;
  uint32_t pid = 0;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  int ret_code = sd_bus_message_read(msg_ptr, "ssuss", &provider, &session_id, &pid, &cwd, &metadata_json);
  if (ret_code < 0) {
    return ret_code;
  }

  nlohmann::json meta;
  if (metadata_json != nullptr && !std::string_view(metadata_json).empty()) {
    try {
      meta = nlohmann::json::parse(metadata_json);
    } catch (...) {
    }
  }

  auto* ctx = static_cast<AppContext*>(userdata);
  std::string assigned =
      ctx->registry.registerSession(provider != nullptr ? provider : "", session_id != nullptr ? session_id : "", pid,
                                    cwd != nullptr ? cwd : "", meta);

  ctx->logger.info("Registered agent session: " + assigned + " (" + (provider != nullptr ? provider : "") + ")");

  sd_bus* bus_ptr = sd_bus_message_get_bus(msg_ptr);
  if (bus_ptr != nullptr) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    sd_bus_emit_signal(bus_ptr, "/org/holonight/AgentActivity1", "org.holonight.AgentActivity1", "SessionAdded", "sss",
                       assigned.c_str(), provider != nullptr ? provider : "", cwd != nullptr ? cwd : "");
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return sd_bus_reply_method_return(msg_ptr, "s", assigned.c_str());
}

int methodPublishEvent(sd_bus_message* msg_ptr, void* userdata, sd_bus_error* /*ret_error*/) {
  const char* session_id = nullptr;
  const char* provider = nullptr;
  const char* cwd = nullptr;
  const char* event_type = nullptr;
  const char* state_str = nullptr;
  const char* title = nullptr;
  const char* message = nullptr;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  int ret_code = sd_bus_message_read(msg_ptr, "sssssss", &session_id, &provider, &cwd, &event_type, &state_str, &title,
                                     &message);  // NOLINT(cppcoreguidelines-pro-type-vararg)

  if (ret_code < 0) {
    return ret_code;
  }

  auto* ctx = static_cast<AppContext*>(userdata);

  holonightd::agent::AgentEvent event;
  event.session_id = session_id != nullptr ? session_id : "";
  event.provider = provider != nullptr ? provider : "";
  event.project_path = cwd != nullptr ? cwd : "";
  event.title = title != nullptr ? title : "";
  event.message = message != nullptr ? message : "";

  auto parsed = holonightd::agent::agentStateFromString(state_str != nullptr ? state_str : "");
  if (parsed.has_value()) {
    event.state = *parsed;
  }

  bool is_success = ctx->registry.publishEvent(event);
  auto sess = ctx->registry.getSession(event.session_id);

  if (sess.has_value()) {
    std::uint32_t notif_id =
        holonightd::agent::NotificationBridge::sendNotification(*sess, event, sess->notification_id);
    ctx->registry.updateNotificationId(event.session_id, notif_id);

    sd_bus* bus_ptr = sd_bus_message_get_bus(msg_ptr);
    if (bus_ptr != nullptr) {
      std::string state_name = holonightd::agent::toString(event.state);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      sd_bus_emit_signal(bus_ptr, "/org/holonight/AgentActivity1", "org.holonight.AgentActivity1", "SessionChanged",
                         "ssss", event.session_id.c_str(), state_name.c_str(), event.title.c_str(),
                         event.message.c_str());

      if (event.state == holonightd::agent::AgentState::WaitingForApproval ||
          event.state == holonightd::agent::AgentState::WaitingForInput) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        sd_bus_emit_signal(bus_ptr, "/org/holonight/AgentActivity1", "org.holonight.AgentActivity1",
                           "AttentionRequested", "ssss", event.session_id.c_str(), sess->provider.c_str(),
                           event.title.c_str(), event.message.c_str());
      }
    }
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return sd_bus_reply_method_return(msg_ptr, "b", is_success ? 1 : 0);
}

int methodEndSession(sd_bus_message* msg_ptr, void* userdata, sd_bus_error* /*ret_error*/) {
  const char* session_id = nullptr;
  const char* result = nullptr;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  int ret_code = sd_bus_message_read(msg_ptr, "ss", &session_id, &result);
  if (ret_code < 0) {
    return ret_code;
  }

  auto* ctx = static_cast<AppContext*>(userdata);
  bool is_success =
      ctx->registry.endSession(session_id != nullptr ? session_id : "", result != nullptr ? result : "Completed");

  ctx->logger.info("Ended agent session: " + std::string(session_id != nullptr ? session_id : ""));

  sd_bus* bus_ptr = sd_bus_message_get_bus(msg_ptr);
  if (bus_ptr != nullptr) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    sd_bus_emit_signal(bus_ptr, "/org/holonight/AgentActivity1", "org.holonight.AgentActivity1", "SessionRemoved", "ss",
                       session_id != nullptr ? session_id : "", result != nullptr ? result : "Completed");
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return sd_bus_reply_method_return(msg_ptr, "b", is_success ? 1 : 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
const sd_bus_vtable agent_activity_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("RegisterSession", "ssuss", "s", methodRegisterSession, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("PublishEvent", "sssssss", "b", methodPublishEvent, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("EndSession", "ss", "b", methodEndSession, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("SessionAdded", "sss", 0),
    SD_BUS_SIGNAL("SessionChanged", "ssss", 0),
    SD_BUS_SIGNAL("SessionRemoved", "ss", 0),
    SD_BUS_SIGNAL("AttentionRequested", "ssss", 0),
    SD_BUS_VTABLE_END};
#endif

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
  AppContext context;
  context.logger.info("Starting holonight-agentd (AI Agent Activity Daemon v0.1.0)...");

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

#ifdef HOLONIGHTD_HAS_SYSTEMD
  sd_bus* bus_ptr = nullptr;
  int ret_code = sd_bus_open_user(&bus_ptr);
  if (ret_code < 0) {
    context.logger.error("Failed to connect to D-Bus user session bus: " + std::string(std::strerror(-ret_code)));
    return 1;
  }
  std::unique_ptr<sd_bus, void (*)(sd_bus*)> bus_guard(bus_ptr, [](sd_bus* b_handle) {
    if (b_handle != nullptr) {
      sd_bus_unref(b_handle);
    }
  });

  ret_code = sd_bus_add_object_vtable(bus_ptr, nullptr, "/org/holonight/AgentActivity1", "org.holonight.AgentActivity1",
                                      static_cast<const sd_bus_vtable*>(agent_activity_vtable), &context);

  if (ret_code < 0) {
    context.logger.error("Failed to register D-Bus vtable: " + std::string(std::strerror(-ret_code)));
    return 1;
  }

  ret_code = sd_bus_request_name(bus_ptr, "org.holonight.AgentActivity1", 0);
  if (ret_code < 0) {
    context.logger.warn("Could not request name org.holonight.AgentActivity1 on user bus: " +
                        std::string(std::strerror(-ret_code)));
  } else {
    context.logger.info("Registered D-Bus service org.holonight.AgentActivity1 on user bus");
  }

  auto next_prune = std::chrono::steady_clock::now() + std::chrono::minutes{1};
  while (g_running) {
    ret_code = sd_bus_process(bus_ptr, nullptr);
    if (ret_code < 0) {
      context.logger.error("D-Bus processing error: " + std::string(std::strerror(-ret_code)));
      break;
    }
    if (ret_code > 0) {
      continue;
    }
    if (std::chrono::steady_clock::now() >= next_prune) {
      context.registry.pruneStaleSessions();
      next_prune = std::chrono::steady_clock::now() + std::chrono::minutes{1};
    }
    (void)sd_bus_wait(bus_ptr, 250000);  // 250ms
  }
#else
  context.logger.info("systemd sd-bus disabled. holonight-agentd running in standalone mode");
  auto next_prune = std::chrono::steady_clock::now() + std::chrono::minutes{1};
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (std::chrono::steady_clock::now() >= next_prune) {
      context.registry.pruneStaleSessions();
      next_prune = std::chrono::steady_clock::now() + std::chrono::minutes{1};
    }
  }
#endif

  context.logger.info("holonight-agentd daemon shutting down cleanly.");
  return 0;
}
