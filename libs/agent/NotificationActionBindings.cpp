// SPDX-License-Identifier: GPL-3.0-or-later
#include "holonightd/agentd/NotificationActionBindings.h"

#include <algorithm>

namespace holonightd::agent {

NotificationActionBindings::NotificationActionBindings(Now now) : now_(std::move(now)) {}

void NotificationActionBindings::prune() {
  const auto cutoff = now_() - kLifetime;
  std::erase_if(entries_, [cutoff](const auto& item) { return item.second.created_at < cutoff; });
}

void NotificationActionBindings::replace(std::uint32_t old_id, std::uint32_t new_id,
                                         const ActivationDescriptor& descriptor) {
  prune();
  if (old_id != 0) {
    entries_.erase(old_id);
  }
  if (new_id == 0 || !descriptor.valid()) {
    return;
  }
  if (!entries_.contains(new_id) && entries_.size() >= kCapacity) {
    const auto oldest = std::ranges::min_element(entries_, {}, [](const auto& item) { return item.second.created_at; });
    entries_.erase(oldest);
  }
  entries_.insert_or_assign(new_id, Entry{.descriptor = descriptor, .created_at = now_()});
}

std::optional<ActivationDescriptor> NotificationActionBindings::consume(std::uint32_t notification_id) {
  prune();
  const auto iter = entries_.find(notification_id);
  if (iter == entries_.end()) {
    return std::nullopt;
  }
  auto descriptor = std::move(iter->second.descriptor);
  entries_.erase(iter);
  return descriptor;
}

void NotificationActionBindings::close(std::uint32_t notification_id) { entries_.erase(notification_id); }

}  // namespace holonightd::agent
