// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/AgentActivity.h"
#include "holonightd/agentd/ProviderNormalizer.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace {

void printUsage(const char* prog) {
  std::cerr << "Usage: " << (prog != nullptr ? prog : "holonight-agent-event")
            << " --provider <name> [--session-id <id>] [--cwd <path>] [--event <name>]\n";
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char** argv) {
  std::string provider = "claude";
  std::string session_id;
  std::string cwd;
  std::string event_type;

  std::span<char*> args(argv, static_cast<std::size_t>(argc));

  for (std::size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "--provider" && i + 1 < args.size()) {
      provider = args[++i];
    } else if (arg == "--session-id" && i + 1 < args.size()) {
      session_id = args[++i];
    } else if (arg == "--cwd" && i + 1 < args.size()) {
      cwd = args[++i];
    } else if (arg == "--event" && i + 1 < args.size()) {
      event_type = args[++i];
    } else if (arg == "--help") {
      printUsage(args[0]);
      return 0;
    }
  }

  std::ostringstream stream;
  stream << std::cin.rdbuf();
  std::string stdin_content = stream.str();

  holonightd::agent::AgentEvent event =
      holonightd::agent::NormalizerFactory::normalize(provider, stdin_content, session_id);

  if (cwd.empty() && event.project_path.empty()) {
    std::error_code err_code;
    auto current_p = std::filesystem::current_path(err_code);
    if (!err_code) {
      cwd = current_p.string();
    }
  }

  if (!cwd.empty()) {
    event.project_path = cwd;
  }
  if (!event_type.empty()) {
    auto parsed = holonightd::agent::agentStateFromString(event_type);
    if (parsed.has_value()) {
      event.state = *parsed;
    }
  }

#ifdef HOLONIGHTD_HAS_SYSTEMD
  sd_bus* bus_ptr = nullptr;
  int ret_code = sd_bus_open_user(&bus_ptr);
  if (ret_code < 0) {
    // Graceful exit if D-Bus user bus unavailable
    return 0;
  }
  std::unique_ptr<sd_bus, void (*)(sd_bus*)> bus_guard(bus_ptr, [](sd_bus* b_handle) {
    if (b_handle != nullptr) {
      sd_bus_unref(b_handle);
    }
  });

  sd_bus_message* reply_ptr = nullptr;
  sd_bus_error bus_error = SD_BUS_ERROR_NULL;

  std::string state_str = holonightd::agent::toString(event.state);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  ret_code = sd_bus_call_method(bus_ptr, "org.holonight.AgentActivity1", "/org/holonight/AgentActivity1",
                                "org.holonight.AgentActivity1", "PublishEvent", &bus_error, &reply_ptr, "sssssss",
                                event.session_id.c_str(), event.provider.c_str(), event.project_path.c_str(),
                                event_type.c_str(), state_str.c_str(), event.title.c_str(), event.message.c_str());

  if (ret_code < 0) {
    sd_bus_error_free(&bus_error);
    return 0;  // Graceful fallback
  }
  if (reply_ptr != nullptr) {
    sd_bus_message_unref(reply_ptr);
  }
  sd_bus_error_free(&bus_error);
#endif

  return 0;
}
