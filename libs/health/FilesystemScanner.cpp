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

#include "holonightd/FilesystemScanner.h"

#include <system_error>

namespace holonightd {

ScanSummary FilesystemScanner::scan(const std::filesystem::path& root) {
  ScanSummary summary;
  std::error_code error;

  if (!std::filesystem::exists(root, error)) {
    summary.errors = 1;
    return summary;
  }

  const std::filesystem::recursive_directory_iterator end;
  std::filesystem::recursive_directory_iterator iterator{
      root, std::filesystem::directory_options::skip_permission_denied, error};

  while (iterator != end) {
    if (error) {
      ++summary.errors;
      error.clear();
      iterator.increment(error);
      continue;
    }

    if (iterator->is_directory(error)) {
      ++summary.directories;
    } else if (iterator->is_regular_file(error)) {
      ++summary.files;
    }

    iterator.increment(error);
  }

  if (error) {
    ++summary.errors;
  }

  return summary;
}

}  // namespace holonightd
