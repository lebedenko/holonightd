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

#include "holonightd/Application.h"
#include "holonightd/Daemon.h"
#include "holonightd/Logger.h"

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <span>
#include <string>

namespace {

std::atomic_bool& stopSignal() {
  static std::atomic_bool instance{false};
  return instance;
}

void handleSignal(int /*signal*/) { stopSignal().store(true); }

void printUsage(std::ostream& output) { output << "Usage: holonightd [--once] [--status] [--debug] [--config PATH]\n"; }

holonightd::CliOptions parseArgs(int argc, char** argv) {
  const std::span<char* const> args{argv, static_cast<std::size_t>(argc)};
  holonightd::CliOptions options;
  auto arg_it = args.begin();
  if (arg_it != args.end()) {
    ++arg_it;
  }
  while (arg_it != args.end()) {
    const std::string arg{*arg_it};
    ++arg_it;
    if (arg == "--once") {
      options.run_once = true;
    } else if (arg == "--status" || arg == "-s") {
      options.status = true;
    } else if (arg == "--debug" || arg == "-d") {
      options.debug = true;
    } else if (arg == "--config") {
      if (arg_it == args.end()) {
        throw std::runtime_error{"--config requires a path"};
      }
      options.config_path = *arg_it;
      ++arg_it;
    } else if (arg == "--help" || arg == "-h") {
      printUsage(std::cout);
      std::exit(0);
    } else {
      throw std::runtime_error{"unknown argument: " + arg};
    }
  }
  return options;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  try {
    const auto options = parseArgs(argc, argv);
    const auto config_path = holonightd::resolveConfigPath(options.config_path);
    auto config = holonightd::Config::fromFile(config_path);
    const auto log_level = holonightd::resolveLogLevel(options, config);
    holonightd::Logger logger{log_level, options.debug};

    const auto db_path = holonightd::resolveDatabasePath(config.database.path);
    holonightd::Daemon daemon{std::move(config), logger, db_path, options.debug};

    if (options.status) {
      return daemon.runStatusCheck();
    }

    daemon.run(stopSignal(), options.run_once ? holonightd::RunMode::Once : holonightd::RunMode::Loop);
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    printUsage(std::cerr);
    return 1;
  }

  return 0;
}
