file(GLOB_RECURSE APP_EMBED_FILES "app_files/files/*")

idf_component_register(
    SRCS
        app_main.cpp
        ApplicationContext.cpp
        app/TemperatureHandler.cpp
        app_files/AppFileTable.cpp
    REQUIRES
        auth
        network_store
        framework_files
        esp_driver_tsens
        framework
        http
        logger
        ota
        wifi_manager
    INCLUDE_DIRS
        .
        app
        app_files
    EMBED_FILES
        ${APP_EMBED_FILES}
)
