# Compress web assets at build time using ESP-IDF's bundled Python.
# compress.py lives in the framework_files component and is shared by both
# the framework and the app.
idf_build_get_property(python PYTHON)
set(COMPRESS_SCRIPT "${CMAKE_SOURCE_DIR}/main/app_files/compress.py")

set(GZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/app_files_gz")
file(GLOB_RECURSE APP_SOURCE_FILES "app_files/files/*")

set(APP_COMPRESSED_FILES "")
foreach(SRC ${APP_SOURCE_FILES})
    get_filename_component(FNAME ${SRC} NAME)
    set(GZ_FILE "${GZ_DIR}/${FNAME}.gz")
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        add_custom_command(
            OUTPUT  ${GZ_FILE}
            COMMAND ${python} ${COMPRESS_SCRIPT} ${SRC} ${GZ_FILE}
            DEPENDS ${SRC} ${COMPRESS_SCRIPT}
            VERBATIM
        )
    endif()
    list(APPEND APP_COMPRESSED_FILES ${GZ_FILE})
endforeach()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_custom_target(compress_app_files DEPENDS ${APP_COMPRESSED_FILES})
endif()

idf_component_register(
    SRCS
        app_main.cpp
        app/ApplicationContext.cpp
        LoggingConfig.cpp
        PersistentLogConfig.cpp
        app/TemperatureHandler.cpp
        app_files/AppFileTable.cpp
    REQUIRES
        auth
        device
        esp_platform
        network_store
        framework_files
        esp_driver_tsens
        framework
        http
        logger
        ota
        persistent_log
        wifi_manager
    INCLUDE_DIRS
        .
        app
        app_files
    EMBED_FILES
        ${APP_COMPRESSED_FILES}
)

if(NOT CMAKE_SCRIPT_MODE_FILE)
    add_dependencies(${COMPONENT_LIB} compress_app_files)
endif()
