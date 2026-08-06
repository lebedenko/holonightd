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

cmake_minimum_required(VERSION 3.25)

project(holonightd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

find_package(SQLite3 REQUIRED)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(LIBSYSTEMD QUIET IMPORTED_TARGET libsystemd)
endif()


if(LIBSYSTEMD_FOUND)
  message(STATUS "libsystemd found. Enabling systemd journal logging sink.")
  add_compile_definitions(HOLONIGHTD_HAS_SYSTEMD)
else()
  message(STATUS "libsystemd not found. Falling back to stdout logging sink.")
endif()


option(HOLONIGHTD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(holonightd_project_options INTERFACE)
target_compile_features(holonightd_project_options INTERFACE cxx_std_23)

add_library(holonightd_project_warnings INTERFACE)
target_compile_options(
    holonightd_project_warnings
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion>
        $<$<AND:$<BOOL:${HOLONIGHTD_WARNINGS_AS_ERRORS}>,$<CXX_COMPILER_ID:GNU,Clang>>:-Werror>
)

add_subdirectory(libs)
add_subdirectory(apps)

# ============================================================
# Tests & Coverage
# ============================================================
option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_COVERAGE "Instrument for gcov/gcovr coverage reporting" OFF)

if(DEFINED BUILD_TESTS AND
   NOT "${BUILD_TESTS}" STREQUAL "ON" AND
   NOT "${BUILD_TESTS}" STREQUAL "OFF" AND
   NOT "${BUILD_TESTS}" STREQUAL "1" AND
   NOT "${BUILD_TESTS}" STREQUAL "0" AND
   NOT "${BUILD_TESTS}" STREQUAL "TRUE" AND
   NOT "${BUILD_TESTS}" STREQUAL "FALSE" AND
   NOT "${BUILD_TESTS}" STREQUAL "YES" AND
   NOT "${BUILD_TESTS}" STREQUAL "NO")
  message(FATAL_ERROR "BUILD_TESTS must be ON or OFF, got: '${BUILD_TESTS}'")
endif()

if(BUILD_TESTS)
  enable_testing()

  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND OR NOT TARGET GTest::gtest OR NOT TARGET GTest::gmock)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    if(TARGET gtest)
      set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endif()

  if(ENABLE_COVERAGE)
    find_program(GCOVR gcovr REQUIRED)
    add_custom_target(clean-coverage-counters
      COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
      COMMENT "Cleaning stale gcov counters"
      VERBATIM
    )
    target_compile_options(holonightd_core PRIVATE --coverage)
    target_link_options(holonightd_core PRIVATE --coverage)
    if(TARGET holonight-healthd)
      target_compile_options(holonight-healthd PRIVATE --coverage)
      target_link_options(holonight-healthd PRIVATE --coverage)
    endif()

    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --object-directory=${CMAKE_BINARY_DIR}
              --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
              -e '${CMAKE_SOURCE_DIR}/tests/'
              -e '${CMAKE_SOURCE_DIR}/third_party/'
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating coverage report"
      VERBATIM
    )
  endif()

  add_subdirectory(tests)
endif()

# ============================================================
# Installation
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/holonightd.example.toml")
  install(FILES config/holonightd.example.toml
          DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/holonight
          RENAME holonightd.conf.example)
endif()

if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
  set(SYSTEMD_WANTED_BY "multi-user.target")
else()
  set(SYSTEMD_WANTED_BY "default.target")
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/data/holonightd.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/data/holonightd.service
  @ONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/systemd/holonight-agentd.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/systemd/holonight-agentd.service
  @ONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/systemd/holonight-healthd.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/systemd/holonight-healthd.service
  @ONLY
)

if(NOT DEFINED SYSTEMD_UNIT_DIR)
  if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    if(PKG_CONFIG_FOUND)
      pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
    endif()
    if(SYSTEMD_SYSTEM_UNIT_DIR)
      set(SYSTEMD_UNIT_DIR "${SYSTEMD_SYSTEM_UNIT_DIR}")
    else()
      set(SYSTEMD_UNIT_DIR "lib/systemd/system")
    endif()
  else()
    set(SYSTEMD_UNIT_DIR "share/systemd/user")
  endif()

endif()

install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/data/holonightd.service
          ${CMAKE_CURRENT_BINARY_DIR}/systemd/holonight-agentd.service
          ${CMAKE_CURRENT_BINARY_DIR}/systemd/holonight-healthd.service
          ${CMAKE_CURRENT_SOURCE_DIR}/systemd/holonight-services.target
        DESTINATION ${SYSTEMD_UNIT_DIR})



# ============================================================
# Format & Tidy Targets
# ============================================================
find_program(CLANG_FORMAT clang-format)
find_program(CLANG_TIDY clang-tidy)
find_program(RUN_CLANG_TIDY run-clang-tidy)

file(GLOB_RECURSE ALL_SOURCE_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    include/*.h include/*.hpp apps/*.cpp apps/*.h apps/*.hpp libs/*.cpp libs/*.h libs/*.hpp tests/*.cpp tests/*.h tests/*.hpp
)

file(GLOB_RECURSE ALL_CXX_SRC_IMPL_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/*.cpp libs/*.cpp
)


file(GLOB_RECURSE ALL_CXX_TEST_IMPL_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    tests/*.cpp
)

set(ALL_CXX_IMPL_FILES ${ALL_CXX_SRC_IMPL_FILES} ${ALL_CXX_TEST_IMPL_FILES})

if(CLANG_FORMAT)
  add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format"
        VERBATIM
    )
  add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking clang-format"
        VERBATIM
    )
endif()

if(CLANG_TIDY)
  include(ProcessorCount)
  ProcessorCount(TIDY_JOBS)
  if(TIDY_JOBS EQUAL 0)
    set(TIDY_JOBS 1)
  endif()

  set(TIDY_HIDE_PROGRESS_FLAG "")
  if(RUN_CLANG_TIDY)
    execute_process(
      COMMAND ${RUN_CLANG_TIDY} --help
      OUTPUT_VARIABLE RUN_CLANG_TIDY_HELP
      ERROR_VARIABLE RUN_CLANG_TIDY_HELP
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(RUN_CLANG_TIDY_HELP MATCHES "-hide-progress")
      set(TIDY_HIDE_PROGRESS_FLAG "-hide-progress")
    endif()
  endif()

  function(add_holonight_tidy_target target_name target_sources)
    if(RUN_CLANG_TIDY)
      add_custom_target(${target_name}
            COMMAND ${RUN_CLANG_TIDY}
                -quiet
                ${TIDY_HIDE_PROGRESS_FLAG}
                -j ${TIDY_JOBS}
                -p ${CMAKE_BINARY_DIR}
                ${${target_sources}}
            COMMENT "Running ${target_name}"
            VERBATIM
        )
    else()
      add_custom_target(${target_name}
            COMMAND ${CLANG_TIDY} --quiet -p ${CMAKE_BINARY_DIR} ${${target_sources}}
            COMMENT "Running ${target_name}"
            VERBATIM
        )
    endif()
  endfunction()

  add_holonight_tidy_target(tidy-src ALL_CXX_SRC_IMPL_FILES)
  add_holonight_tidy_target(tidy-tests ALL_CXX_TEST_IMPL_FILES)
  add_holonight_tidy_target(tidy ALL_CXX_IMPL_FILES)

  if(NOT RUN_CLANG_TIDY)
    message(STATUS "run-clang-tidy was not found; tidy targets will use clang-tidy sequentially")
  endif()
endif()
