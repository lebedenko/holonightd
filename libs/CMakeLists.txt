# SPDX-License-Identifier: GPL-3.0-or-later
# Shared libraries configuration for holonight services

# 1. Config Library
add_library(holonight_config
    config/Application.cpp
)
target_include_directories(holonight_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)
target_link_libraries(holonight_config
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
)

# 2. Logging Library
add_library(holonight_logging
    logging/Logger.cpp
)
target_include_directories(holonight_logging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(holonight_logging
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
)
if(TARGET PkgConfig::LIBSYSTEMD)
    target_link_libraries(holonight_logging PRIVATE PkgConfig::LIBSYSTEMD)
endif()

# 3. Process Library
add_library(holonight_process
    process/CommandRunner.cpp
)
target_include_directories(holonight_process
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(holonight_process
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
)

# 4. Storage Library
add_library(holonight_storage
    storage/EventStore.cpp
    storage/ObservationEvent.cpp
)
target_include_directories(holonight_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)
target_link_libraries(holonight_storage
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
        SQLite3::SQLite3
        holonight_logging
)

# 5. Health Library
add_library(holonight_health
    health/Daemon.cpp
    health/FilesystemScanner.cpp
    health/HealthCheckJob.cpp
    health/LlmClient.cpp
    health/MemoryCollector.cpp
    health/PacmanCollector.cpp
    health/RuleEngine.cpp
    health/StorageCollector.cpp
    health/SystemdCollector.cpp
)
target_include_directories(holonight_health
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)
target_link_libraries(holonight_health
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
        holonight_config
        holonight_logging
        holonight_process
        holonight_storage
)
if(TARGET PkgConfig::LIBSYSTEMD)
    target_link_libraries(holonight_health PRIVATE PkgConfig::LIBSYSTEMD)
endif()

# 6. Agent Activity Library
add_library(holonight_agent
    agent/AgentActivity.cpp
    agent/ProviderNormalizer.cpp
    agent/SessionRegistry.cpp
    agent/NotificationBridge.cpp
)

target_include_directories(holonight_agent
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)
target_link_libraries(holonight_agent
    PUBLIC
        holonightd_project_options
        holonightd_project_warnings
        holonight_logging
)

# Core aggregation library for backwards-compatibility in tests & builds
add_library(holonightd_core INTERFACE)
target_link_libraries(holonightd_core INTERFACE
    holonight_config
    holonight_logging
    holonight_process
    holonight_storage
    holonight_health
    holonight_agent
)

