cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded_scaffold)

# After the build produces <project>.bin, create a versioned copy alongside it.
# PROJECT_VER is populated automatically from version.txt by ESP-IDF's project.cmake.
# Result: build/embedded_scaffold-0.0.2.bin  (the file you upload via OTA)
add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VER}.bin"
    VERBATIM
    COMMENT "OTA binary → ${PROJECT_NAME}-${PROJECT_VER}.bin"
)
