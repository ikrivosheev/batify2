cmake_minimum_required(VERSION 3.10)

project(batify VERSION 0.0.1 LANGUAGES C)

add_subdirectory(src)

include(GNUInstallDirs)
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    FILES "man/batify.1"
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)
