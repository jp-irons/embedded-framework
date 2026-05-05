file(GLOB_RECURSE APP_EMBED_FILES "app/files/*")

idf_component_register(
    SRCS
        app_main.cpp
        ApplicationContext.cpp
        app/AppFileTable.cpp
        app/TemperatureHandler.cpp
    REQUIRES
        auth
        credential_store
        embedded_files
        esp_driver_tsens
        framework
        http
        logger
        ota
        wifi_manager
    INCLUDE_DIRS
        .
        app
    EMBED_FILES
        ${APP_EMBED_FILES}
)
