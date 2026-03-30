idf_component_register(
    SRCS
        FrameworkContext.cpp

    INCLUDE_DIRS
        include

    REQUIRES
        wifi_manager
        credential_store
        core_api
        http
)