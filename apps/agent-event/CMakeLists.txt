# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(holonight-agent-event
    main.cpp
)

target_link_libraries(holonight-agent-event
    PRIVATE
        holonightd_project_options
        holonightd_project_warnings
        holonight_agent
)

if(TARGET PkgConfig::LIBSYSTEMD)
    target_link_libraries(holonight-agent-event PRIVATE PkgConfig::LIBSYSTEMD)
endif()

install(TARGETS holonight-agent-event
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
