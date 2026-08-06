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

#include "holonightd/LlmClient.h"

#include <algorithm>

namespace holonightd {

std::string LocalSummaryClient::summarize(const std::string& input) const {
  const auto failureCount = std::ranges::count(input, std::string::value_type{'$'});
  if (input.contains("exit=0") && !input.contains("exit=1")) {
    return "local summary: maintenance commands completed successfully";
  }

  return "local summary: reviewed " + std::to_string(failureCount) +
         " command blocks; inspect non-zero exits and tool output";
}

}  // namespace holonightd
