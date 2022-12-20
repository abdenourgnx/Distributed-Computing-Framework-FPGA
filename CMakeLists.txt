cmake_minimum_required(VERSION 3.1.0)
project(HPC VERSION 0.1.0)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")

#use c++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find boost library 
SET(Boost_USE_STATIC_LIBS ON)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost REQUIRED COMPONENTS system thread)



IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

SET (USED_LIBS utils FPGAconfig ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})

add_subdirectory(utils)

add_subdirectory(node)

add_subdirectory(server)

add_subdirectory(try)




