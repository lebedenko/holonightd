// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "holonightd/agentd/AgentActivity.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

namespace holonightd::agent {

class NotificationActionBindings {
 public:
  using Clock = std::chrono::steady_clock;
  using Now = std::function<Clock::time_point()>;

  explicit NotificationActionBindings(Now now = Clock::now);
  void replace(std::uint32_t old_id, std::uint32_t new_id, const ActivationDescriptor& descriptor);
  [[nodiscard]] std::optional<ActivationDescriptor> consume(std::uint32_t notification_id);
  void close(std::uint32_t notification_id);
  void prune();
  [[nodiscard]] std::size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    ActivationDescriptor descriptor;
    Clock::time_point created_at;
  };
  static constexpr std::size_t kCapacity = 256;
  static constexpr auto kLifetime = std::chrono::hours(24);
  Now now_;
  std::unordered_map<std::uint32_t, Entry> entries_;
};

}  // namespace holonightd::agent
