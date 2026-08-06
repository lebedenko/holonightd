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

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

TEST(FilesystemScannerTest, CountsFilesAndDirectories) {
  const auto root = std::filesystem::temp_directory_path() / "holonightd-scan-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "nested");
  {
    std::ofstream output{root / "file.txt"};
    output << "data";
  }
  {
    std::ofstream output{root / "nested" / "other.txt"};
    output << "data";
  }

  const auto result = holonightd::FilesystemScanner::scan(root);

  EXPECT_EQ(result.files, std::uintmax_t{2});
  EXPECT_EQ(result.directories, std::uintmax_t{1});
  EXPECT_EQ(result.errors, std::uintmax_t{0});

  std::filesystem::remove_all(root);
}
