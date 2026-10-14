# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(hn-agent-run
    main.cpp
)

target_link_libraries(hn-agent-run
    PRIVATE
        nlohmann_json::nlohmann_json
        holonightd_project_options
        holonightd_project_warnings
)

if(BUILD_SYSTEMD)
    target_link_libraries(hn-agent-run PRIVATE PkgConfig::LIBSYSTEMD)
endif()

install(TARGETS hn-agent-run
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
