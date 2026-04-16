file(GLOB_RECURSE EMBED_ASSETS "assets/*")
#touch
idf_component_register(
    SRCS
    	StaticFileHandler.cpp
    	EmbeddedAssetTable.cpp
    INCLUDE_DIRS
        include
    REQUIRES
        common
        http
        logger
    EMBED_FILES
        ${EMBED_ASSETS}    
)
