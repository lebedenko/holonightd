// SPDX-License-Identifier: GPL-3.0-or-later
#include "holonightd/agentd/ShellActivationClient.h"

#include <cerrno>
#include <memory>

namespace holonightd::agent {

ActivationResult ShellActivationClient::classifyCallResult(int call_result, std::optional<bool> reply) {
  if (call_result == -ETIMEDOUT) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::Timeout};
  }
  if (call_result < 0) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::Unavailable};
  }
  if (!reply.has_value()) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::MalformedReply};
  }
  return {.accepted = *reply, .diagnostic = *reply ? ActivationDiagnostic::Accepted : ActivationDiagnostic::Rejected};
}

ActivationResult ShellActivationClient::activate(const ActivationDescriptor& descriptor) {
#ifndef HOLONIGHTD_HAS_SYSTEMD
  (void)descriptor;
  return {.accepted = false, .diagnostic = ActivationDiagnostic::Unavailable};
#else
  if (bus_ == nullptr || !descriptor.valid()) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::Unavailable};
  }
  sd_bus_message* request_raw = nullptr;
  int result = sd_bus_message_new_method_call(bus_, &request_raw, kDestination.data(), kPath.data(), kInterface.data(),
                                              kMember.data());
  if (result < 0) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::Unavailable};
  }
  std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> request(request_raw, sd_bus_message_unref);
  result = sd_bus_message_open_container(request.get(), 'a', "u");
  for (const auto pid : descriptor.process_lineage) {
    if (result >= 0) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      result = sd_bus_message_append(request.get(), "u", pid);
    }
  }
  if (result >= 0) {
    result = sd_bus_message_close_container(request.get());
  }
  if (result >= 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    result = sd_bus_message_append(request.get(), "s", descriptor.title_hint.c_str());
  }
  if (result < 0) {
    return {.accepted = false, .diagnostic = ActivationDiagnostic::Unavailable};
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply_raw = nullptr;
  result = sd_bus_call(bus_, request.get(), static_cast<std::uint64_t>(kTimeout.count()) * 1000, &error, &reply_raw);
  sd_bus_error_free(&error);
  std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> reply(reply_raw, sd_bus_message_unref);
  if (result < 0 || reply == nullptr) {
    return classifyCallResult(result, std::nullopt);
  }
  int accepted = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  if (sd_bus_message_read(reply.get(), "b", &accepted) <= 0) {
    return classifyCallResult(result, std::nullopt);
  }
  return classifyCallResult(result, accepted != 0);
#endif
}

}  // namespace holonightd::agent
