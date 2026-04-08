idf_component_register(
    SRCS
        FrameworkContext.cpp

    INCLUDE_DIRS
        include

    REQUIRES
        credential_store
        http
        static_assets
        wifi_manager
        shared
)