file(GLOB_RECURSE EMBED_FILES "assets/*")
#touch
idf_component_register(
    SRCS
    	EmbeddedFileHandler.cpp
    	EmbeddedFileTable.cpp
    INCLUDE_DIRS
        include
    REQUIRES
        common
        http
        logger
    EMBED_FILES
        ${EMBED_FILES}    
)
