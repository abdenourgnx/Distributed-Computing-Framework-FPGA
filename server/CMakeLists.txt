cmake_minimum_required(VERSION 3.0.0)
project(HPC VERSION 0.1.0)

set (CMAKE_CXX_STANDARD 20)

# Find boost library 
SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost COMPONENTS system coroutine)

IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)


SET (USED_LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_COROUTINE_LIBRARY})

add_executable (server main.cpp)
target_link_libraries (server ${USED_LIBS})
target_compile_options(server PUBLIC "-fcoroutines")

add_executable (test_user test_user.cpp)
target_link_libraries (test_user ${USED_LIBS} -luring)
target_compile_options(test_user PUBLIC "-fcoroutines")
target_compile_definitions(test_user PRIVATE BOOST_ASIO_DISABLE_EPOLL BOOST_ASIO_HAS_IO_URING)

add_executable (test_node test_node.cpp)
target_link_libraries (test_node ${USED_LIBS})
target_compile_options(test_node PUBLIC "-fcoroutines")