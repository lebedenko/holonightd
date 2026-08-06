// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "holonightd/CommandRunner.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace holonightd {

namespace {

constexpr int kCommandFailedToStart = 127;
constexpr int kCommandTimedOut = 124;
constexpr int kCommandCancelled = 125;
constexpr auto kTerminationGracePeriod = std::chrono::milliseconds{250};

void appendBounded(std::string& output, const char* data, std::size_t size, std::size_t limit, bool& truncated) {
  const auto remaining = output.size() < limit ? limit - output.size() : 0;
  const auto accepted = std::min(size, remaining);
  output.append(data, accepted);
  truncated = truncated || accepted < size;
}

int normalizedExitCode(int status) {
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return kCommandFailedToStart;
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
CommandResult CommandRunner::run(const std::string& command, CommandOptions options) {
  std::array<char, 4096> buffer{};
  CommandResult result;
  result.command = command;
  std::array<int, 2> pipe_fds{};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  if (pipe(pipe_fds.data()) != 0) {
    throw std::runtime_error{"failed to create command pipe: " + std::string(std::strerror(errno))};
  }

  const pid_t child_pid = fork();
  if (child_pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    throw std::runtime_error{"failed to fork command: " + std::string(std::strerror(errno))};
  }
  if (child_pid == 0) {
    (void)setpgid(0, 0);
    close(pipe_fds[0]);
    (void)dup2(pipe_fds[1], STDOUT_FILENO);
    (void)dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);
    std::array<char, 3> shell_name{'s', 'h', '\0'};
    std::array<char, 3> command_flag{'-', 'c', '\0'};
    std::vector<char> command_text(command.begin(), command.end());
    command_text.push_back('\0');
    std::array<char*, 4> shell_args{shell_name.data(), command_flag.data(), command_text.data(), nullptr};
    execv("/bin/sh", shell_args.data());
    _exit(kCommandFailedToStart);
  }

  (void)setpgid(child_pid, child_pid);
  close(pipe_fds[1]);
  const int current_flags = fcntl(pipe_fds[0], F_GETFL, 0);
  if (current_flags >= 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    (void)fcntl(pipe_fds[0], F_SETFL, current_flags | O_NONBLOCK);
  }

  const auto deadline = std::chrono::steady_clock::now() + options.timeout;
  int status = 0;
  bool child_finished = false;
  bool pipe_finished = false;

  while (!child_finished || !pipe_finished) {
    const auto bytes_read = read(pipe_fds[0], buffer.data(), buffer.size());
    if (bytes_read > 0) {
      appendBounded(result.output, buffer.data(), static_cast<std::size_t>(bytes_read), options.max_output_bytes,
                    result.output_truncated);
    } else if (bytes_read == 0) {
      pipe_finished = true;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
      pipe_finished = true;
    }

    if (!child_finished) {
      const pid_t wait_result = waitpid(child_pid, &status, WNOHANG);
      child_finished = wait_result == child_pid;
      if (wait_result < 0 && errno != EINTR) {
        close(pipe_fds[0]);
        throw std::runtime_error{"failed to wait for command: " + std::string(std::strerror(errno))};
      }
    }

    const bool cancellation_requested =
        options.stop_requested != nullptr && options.stop_requested->load(std::memory_order_relaxed);
    if ((!child_finished || !pipe_finished) &&
        (cancellation_requested || std::chrono::steady_clock::now() >= deadline)) {
      result.cancelled = cancellation_requested;
      result.timed_out = !cancellation_requested;
      (void)kill(-child_pid, SIGTERM);
      const auto grace_deadline = std::chrono::steady_clock::now() + kTerminationGracePeriod;
      while (std::chrono::steady_clock::now() < grace_deadline) {
        if (waitpid(child_pid, &status, WNOHANG) == child_pid) {
          child_finished = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      if (!child_finished) {
        (void)kill(-child_pid, SIGKILL);
        while (waitpid(child_pid, &status, 0) < 0 && errno == EINTR) {
        }
        child_finished = true;
      }
    }

    if (!child_finished || !pipe_finished) {
      pollfd descriptor{.fd = pipe_fds[0], .events = POLLIN, .revents = 0};
      (void)poll(&descriptor, 1, 10);
    }
  }

  close(pipe_fds[0]);
  result.exit_code = normalizedExitCode(status);
  if (result.timed_out) {
    result.exit_code = kCommandTimedOut;
  }
  if (result.cancelled) {
    result.exit_code = kCommandCancelled;
  }
  return result;
}

}  // namespace holonightd
