# SPDX-License-Identifier: GPL-3.0-or-later
# holonightd - Lightweight daemon for routine maintenance automation.
# Copyright (C) 2026 holonightd contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

add_executable(holonightd_tests
    test_application.cpp
    test_command_runner.cpp
    test_daemon.cpp
    test_event_store.cpp
    test_filesystem_scanner.cpp
    test_health_check_job.cpp
    test_llm_client.cpp
    test_logger.cpp
    test_observation_event.cpp
    test_storage_collector.cpp
    test_systemd_collector.cpp
    test_memory_collector.cpp
    test_pacman_collector.cpp
    test_rule_engine.cpp
    test_agent_activity.cpp
)



target_link_libraries(holonightd_tests
    PRIVATE
        holonightd_core
        GTest::gtest_main
)

# Mark GTest headers as system includes to suppress warnings from GTest internals
get_target_property(gtest_includes GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(holonightd_tests SYSTEM PRIVATE "${gtest_includes}")

# GTest macros generate code that would produce false positives under clang-tidy
set_target_properties(holonightd_tests PROPERTIES CXX_CLANG_TIDY "")

include(GoogleTest)
gtest_discover_tests(holonightd_tests)
