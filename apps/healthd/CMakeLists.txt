# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(holonight-healthd
    main.cpp
)

target_link_libraries(holonight-healthd
    PRIVATE
        holonightd_project_options
        holonightd_project_warnings
        holonight_config
        holonight_logging
        holonight_health
)

# Create holonightd symlink for backwards compatibility
add_custom_target(holonightd_symlink ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:holonight-healthd> ${CMAKE_CURRENT_BINARY_DIR}/holonightd
    DEPENDS holonight-healthd
    COMMENT "Creating holonightd binary symlink to holonight-healthd"
)

install(TARGETS holonight-healthd
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
