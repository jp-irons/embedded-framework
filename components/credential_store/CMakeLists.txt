idf_component_register(
    SRCS "CredentialStore.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES nvs_flash
)