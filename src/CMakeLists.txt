add_executable(batify main.c)
add_library(battery battery.c)

set_target_properties(batify battery PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS OFF
)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(LIBNOTIFY REQUIRED libnotify)

target_compile_definitions(batify PRIVATE G_LOG_DOMAIN="batify")
target_compile_options(batify PRIVATE -Wall -Wextra)
target_compile_options(battery PRIVATE -Wall -Wextra)

target_link_libraries(batify
    battery
    ${GLIB_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
)

target_include_directories(
    batify
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)

target_include_directories(
    battery
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
)
