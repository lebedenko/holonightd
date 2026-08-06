// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#pragma once

#include "holonightd/agentd/AgentActivity.h"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace holonightd::agent {

class ProviderNormalizer {
 public:
  ProviderNormalizer() = default;
  virtual ~ProviderNormalizer() = default;

  ProviderNormalizer(const ProviderNormalizer&) = default;
  ProviderNormalizer& operator=(const ProviderNormalizer&) = default;
  ProviderNormalizer(ProviderNormalizer&&) = default;
  ProviderNormalizer& operator=(ProviderNormalizer&&) = default;

  [[nodiscard]] virtual AgentEvent normalize(std::string_view raw_json_or_text,
                                             std::string_view default_session_id = "") const = 0;
};

class ClaudeNormalizer final : public ProviderNormalizer {
 public:
  [[nodiscard]] AgentEvent normalize(std::string_view raw_json_or_text,
                                     std::string_view default_session_id = "") const override;
};

class CodexNormalizer final : public ProviderNormalizer {
 public:
  [[nodiscard]] AgentEvent normalize(std::string_view raw_json_or_text,
                                     std::string_view default_session_id = "") const override;
};

class KiroNormalizer final : public ProviderNormalizer {
 public:
  [[nodiscard]] AgentEvent normalize(std::string_view raw_json_or_text,
                                     std::string_view default_session_id = "") const override;
};

class AntigravityNormalizer final : public ProviderNormalizer {
 public:
  [[nodiscard]] AgentEvent normalize(std::string_view raw_json_or_text,
                                     std::string_view default_session_id = "") const override;
};

class NormalizerFactory {
 public:
  [[nodiscard]] static AgentEvent normalize(std::string_view provider, std::string_view raw_json_or_text,
                                            std::string_view default_session_id = "");
};

}  // namespace holonightd::agent
