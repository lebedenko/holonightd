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

#pragma once

#include <string>

namespace holonightd {

class LlmClient {
 public:
  LlmClient() = default;
  virtual ~LlmClient() = default;
  LlmClient(const LlmClient&) = delete;
  LlmClient& operator=(const LlmClient&) = delete;
  LlmClient(LlmClient&&) = delete;
  LlmClient& operator=(LlmClient&&) = delete;

  [[nodiscard]] virtual std::string summarize(const std::string& input) const = 0;
};

class LocalSummaryClient final : public LlmClient {
 public:
  LocalSummaryClient() = default;
  ~LocalSummaryClient() override = default;
  LocalSummaryClient(const LocalSummaryClient&) = delete;
  LocalSummaryClient& operator=(const LocalSummaryClient&) = delete;
  LocalSummaryClient(LocalSummaryClient&&) = delete;
  LocalSummaryClient& operator=(LocalSummaryClient&&) = delete;

  [[nodiscard]] std::string summarize(const std::string& input) const override;
};

}  // namespace holonightd
