
SET (NODE_SRCS 
    main.cpp
    )


add_executable (node-agent ${NODE_SRCS})
target_link_libraries (node-agent ${USED_LIBS})

add_subdirectory(FPGAconfig)

