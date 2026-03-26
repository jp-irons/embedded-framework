idf_component_register(
    SRCS
        app_main.cpp
        ApplicationContext.cpp
    REQUIRES wifi_manager credential_store cjson
    INCLUDE_DIRS
        .
   EMBED_FILES
        ui/favicon.ico
        ui/provision/index.html
        ui/provision/provision.js
        ui/provision/tailwind.css
)