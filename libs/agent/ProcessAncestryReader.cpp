// SPDX-License-Identifier: GPL-3.0-or-later
#include "holonightd/agentd/ProcessAncestryReader.h"

#include <charconv>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace holonightd::agent {

namespace {
std::optional<std::string> readProcStat(std::uint32_t pid) {
  std::ifstream stream("/proc/" + std::to_string(pid) + "/stat");
  if (!stream) {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}
}  // namespace

ProcessAncestryReader::ProcessAncestryReader(StatReader reader) : reader_(reader ? std::move(reader) : readProcStat) {}

std::optional<std::uint32_t> ProcessAncestryReader::parseParentPid(const std::string& stat) {
  const auto command_end = stat.rfind(')');
  if (command_end == std::string::npos || command_end + 4 >= stat.size() || stat[command_end + 1] != ' ') {
    return std::nullopt;
  }
  const auto parent_begin = command_end + 4;  // ") <state> "
  const auto parent_end = stat.find(' ', parent_begin);
  const auto field = std::string_view(stat).substr(parent_begin, parent_end - parent_begin);
  std::uint32_t parent = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), parent);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (error != std::errc{} || end != field.data() + field.size()) {
    return std::nullopt;
  }
  return parent;
}

std::vector<std::uint32_t> ProcessAncestryReader::read(std::uint32_t pid) const {
  std::vector<std::uint32_t> lineage;
  std::unordered_set<std::uint32_t> visited;
  for (std::size_t count = 0; pid > 0 && count < 64 && !visited.contains(pid); ++count) {
    const auto stat = reader_(pid);
    if (!stat.has_value()) {
      break;
    }
    const auto parent = parseParentPid(*stat);
    if (!parent.has_value()) {
      break;
    }
    lineage.push_back(pid);
    visited.insert(pid);
    if (pid == 1) {
      break;
    }
    if (*parent == 0) {
      break;
    }
    pid = *parent;
  }
  return lineage;
}

}  // namespace holonightd::agent
