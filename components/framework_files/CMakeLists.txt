# Compress web assets at build time using ESP-IDF's bundled Python.
# This works on all platforms including Windows, where the gzip CLI is absent.
#
# ESP-IDF runs component CMakeLists.txt in script mode first (to gather
# requirements) where add_custom_command/add_custom_target are not available.
# The COMPRESSED_FILES list is built unconditionally so EMBED_FILES is always
# correct; the build commands are guarded against script mode.
idf_build_get_property(python PYTHON)

set(GZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/files_gz")
file(GLOB_RECURSE SOURCE_FILES "files/*")

set(COMPRESSED_FILES "")
foreach(SRC ${SOURCE_FILES})
    get_filename_component(FNAME ${SRC} NAME)
    set(GZ_FILE "${GZ_DIR}/${FNAME}.gz")
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        add_custom_command(
            OUTPUT  ${GZ_FILE}
            COMMAND ${python} ${CMAKE_CURRENT_SOURCE_DIR}/compress.py ${SRC} ${GZ_FILE}
            DEPENDS ${SRC} ${CMAKE_CURRENT_SOURCE_DIR}/compress.py
            VERBATIM
        )
    endif()
    list(APPEND COMPRESSED_FILES ${GZ_FILE})
endforeach()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_custom_target(compress_framework_files DEPENDS ${COMPRESSED_FILES})
endif()

idf_component_register(
    SRCS
        EmbeddedFileHandler.cpp
        EmbeddedFileTable.cpp
    INCLUDE_DIRS
        include
    REQUIRES
        common
        http
        logger
    EMBED_FILES
        ${COMPRESSED_FILES}
)

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_dependencies(${COMPONENT_LIB} compress_framework_files)
endif()
