# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(healthd)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agentd/CMakeLists.txt")
    add_subdirectory(agentd)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agent-event/CMakeLists.txt")
    add_subdirectory(agent-event)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agent-run/CMakeLists.txt")
    add_subdirectory(agent-run)
endif()
