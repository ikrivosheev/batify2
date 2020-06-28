cmake_minimum_required(VERSION 3.5)

project(batify VERSION 0.0.1)

add_subdirectory(src)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
)
install(    
    FILES "man/batify.1"
    DESTINATION share/man/man1
)
