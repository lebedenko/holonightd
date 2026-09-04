// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace holonightd::agent {

class ProcessAncestryReader {
 public:
  using StatReader = std::function<std::optional<std::string>(std::uint32_t)>;

  explicit ProcessAncestryReader(StatReader reader = {});
  [[nodiscard]] std::vector<std::uint32_t> read(std::uint32_t pid) const;
  [[nodiscard]] static std::optional<std::uint32_t> parseParentPid(const std::string& stat);

 private:
  StatReader reader_;
};

}  // namespace holonightd::agent
