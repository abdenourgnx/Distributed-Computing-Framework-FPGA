
SET (UTILS_SRCS 
    src/Block.cpp
    src/Port.cpp
    src/utils.cpp
    )


add_library (utils STATIC ${UTILS_SRCS})
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
