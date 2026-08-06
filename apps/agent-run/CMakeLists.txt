# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(hn-agent-run
    main.cpp
)

target_link_libraries(hn-agent-run
    PRIVATE
        holonightd_project_options
        holonightd_project_warnings
)

if(TARGET PkgConfig::LIBSYSTEMD)
    target_link_libraries(hn-agent-run PRIVATE PkgConfig::LIBSYSTEMD)
endif()

install(TARGETS hn-agent-run
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
