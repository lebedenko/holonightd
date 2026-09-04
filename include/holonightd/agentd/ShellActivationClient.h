// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "holonightd/agentd/AgentActivity.h"

#include <chrono>
#include <optional>
#include <string_view>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace holonightd::agent {

enum class ActivationDiagnostic : std::uint8_t { Accepted, Rejected, Unavailable, MalformedReply, Timeout };

struct ActivationResult {
  bool accepted{false};
  ActivationDiagnostic diagnostic{ActivationDiagnostic::Rejected};
};

class ActivationClient {
 public:
  ActivationClient() = default;
  virtual ~ActivationClient() = default;
  ActivationClient(const ActivationClient&) = delete;
  ActivationClient& operator=(const ActivationClient&) = delete;
  ActivationClient(ActivationClient&&) = delete;
  ActivationClient& operator=(ActivationClient&&) = delete;
  [[nodiscard]] virtual ActivationResult activate(const ActivationDescriptor& descriptor) = 0;
};

class ShellActivationClient final : public ActivationClient {
 public:
  static constexpr std::string_view kDestination = "org.holonight.Shell";
  static constexpr std::string_view kPath = "/org/holonight/Shell";
  static constexpr std::string_view kInterface = "org.holonight.Shell.WindowActivation1";
  static constexpr std::string_view kMember = "RequestWindowActivation";
  static constexpr std::string_view kInputSignature = "aus";
  static constexpr auto kTimeout = std::chrono::milliseconds(500);

#ifdef HOLONIGHTD_HAS_SYSTEMD
  explicit ShellActivationClient(sd_bus* bus) : bus_(bus) {}
#else
  explicit ShellActivationClient(void* = nullptr) {}
#endif
  [[nodiscard]] ActivationResult activate(const ActivationDescriptor& descriptor) override;
  [[nodiscard]] static ActivationResult classifyCallResult(int call_result, std::optional<bool> reply);

 private:
#ifdef HOLONIGHTD_HAS_SYSTEMD
  sd_bus* bus_;
#endif
};

}  // namespace holonightd::agent
