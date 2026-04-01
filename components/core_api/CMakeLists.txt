idf_component_register(
    SRCS
        CredentialApiHandler.cpp
        WiFiApiHandler.cpp

    INCLUDE_DIRS
        "include"

    REQUIRES
        credential_store
        wifi_manager
        http
        cjson
)