idf_component_register(
    SRCS
        HttpRequest.cpp
        HttpResponse.cpp

    INCLUDE_DIRS
        include

    REQUIRES
        esp_http_server
)