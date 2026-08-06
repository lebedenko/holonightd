// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace {

void setTerminalTitle(const std::string& provider, const std::string& project_name) {
  // OSC 0 escape sequence to set window title
  std::cout << "\033]0;" << provider << " · " << project_name << "\007" << std::flush;
}

void restoreTerminalTitle(const std::string& project_name) {
  std::cout << "\033]0;" << project_name << "\007" << std::flush;
}

std::string getCwd() {
  std::error_code err_code;
  auto path_obj = std::filesystem::current_path(err_code);
  if (err_code) {
    return ".";
  }
  return path_obj.string();
}

std::string getProjectName(const std::string& cwd) {
  std::filesystem::path path_obj(cwd);
  if (path_obj.has_filename()) {
    return path_obj.filename().string();
  }
  return cwd;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: hn-agent-run <agent_command> [args...]\n";
    return 1;
  }

  std::span<char*> args(argv, static_cast<std::size_t>(argc));
  std::string provider = args[1];
  std::string cwd = getCwd();
  std::string project_name = getProjectName(cwd);
  auto pid = static_cast<uint32_t>(getpid());
  std::string session_id = provider + "-" + std::to_string(pid);

  setTerminalTitle(provider, project_name);

#ifdef HOLONIGHTD_HAS_SYSTEMD
  sd_bus* bus_ptr = nullptr;
  if (sd_bus_open_user(&bus_ptr) >= 0) {
    std::unique_ptr<sd_bus, void (*)(sd_bus*)> bus_guard(bus_ptr, [](sd_bus* b_handle) {
      if (b_handle != nullptr) {
        sd_bus_unref(b_handle);
      }
    });

    sd_bus_message* reply_ptr = nullptr;
    sd_bus_error bus_error = SD_BUS_ERROR_NULL;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    sd_bus_call_method(bus_ptr, "org.holonight.AgentActivity1", "/org/holonight/AgentActivity1",
                       "org.holonight.AgentActivity1", "RegisterSession", &bus_error, &reply_ptr, "ssuss",
                       provider.c_str(), session_id.c_str(), pid, cwd.c_str(), "{}");
    if (reply_ptr != nullptr) {
      sd_bus_message_unref(reply_ptr);
    }
    sd_bus_error_free(&bus_error);
  }
#endif

  pid_t child_pid = fork();
  if (child_pid < 0) {
    std::perror("fork failed");
    restoreTerminalTitle(project_name);
    return 1;
  }
  if (child_pid == 0) {
    // Child process: execute agent command
    std::vector<char*> exec_args;
    for (std::size_t i = 1; i < args.size(); ++i) {
      exec_args.push_back(args[i]);
    }
    exec_args.push_back(nullptr);

    execvp(exec_args[0], exec_args.data());
    std::perror("execvp failed");
    _exit(127);
  }

  int exit_status = 0;
  pid_t wait_result = waitpid(child_pid, &exit_status, 0);
  while (wait_result < 0 && errno == EINTR) {
    wait_result = waitpid(child_pid, &exit_status, 0);
  }
  if (wait_result < 0) {
    std::perror("waitpid failed");
    restoreTerminalTitle(project_name);
    return 1;
  }

#ifdef HOLONIGHTD_HAS_SYSTEMD
  if (sd_bus_open_user(&bus_ptr) >= 0) {
    std::unique_ptr<sd_bus, void (*)(sd_bus*)> bus_guard(bus_ptr, [](sd_bus* b_handle) {
      if (b_handle != nullptr) {
        sd_bus_unref(b_handle);
      }
    });

    sd_bus_message* reply_ptr = nullptr;
    sd_bus_error bus_error = SD_BUS_ERROR_NULL;

    std::string result = (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) ? "Completed" : "Failed";
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    sd_bus_call_method(bus_ptr, "org.holonight.AgentActivity1", "/org/holonight/AgentActivity1",
                       "org.holonight.AgentActivity1", "EndSession", &bus_error, &reply_ptr, "ss", session_id.c_str(),
                       result.c_str());
    if (reply_ptr != nullptr) {
      sd_bus_message_unref(reply_ptr);
    }
    sd_bus_error_free(&bus_error);
  }
#endif

  restoreTerminalTitle(project_name);
  return WIFEXITED(exit_status) ? WEXITSTATUS(exit_status) : 128 + WTERMSIG(exit_status);
}
