# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(holonight-agentd
    main.cpp
)

target_link_libraries(holonight-agentd
    PRIVATE
        holonightd_project_options
        holonightd_project_warnings
        holonight_agent
        holonight_logging
)

if(TARGET PkgConfig::LIBSYSTEMD)
    target_link_libraries(holonight-agentd PRIVATE PkgConfig::LIBSYSTEMD)
endif()

install(TARGETS holonight-agentd
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
