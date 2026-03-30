idf_component_register(
    SRCS
        CredentialApiHandler.cpp
        ProvisioningApiHandler.cpp
        WiFiApiHandler.cpp

    INCLUDE_DIRS
        "include"

    REQUIRES
        credential_store
        wifi_manager
        http
        esp_http_server
        cjson
)