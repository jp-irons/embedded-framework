file(GLOB_RECURSE APP_EMBED_FILES "files/*")

idf_component_register(
    SRCS
        app_main.cpp
        AppFileTable.cpp
        ApplicationContext.cpp
    REQUIRES
        auth
        credential_store
        embedded_files
        framework
        http
        logger
        ota
        wifi_manager
    INCLUDE_DIRS
        .
    EMBED_FILES
        ${APP_EMBED_FILES}
)
