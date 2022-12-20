
set(CONFIG_SRC
    alt_fpga_manager.c
)


add_library(FPGAconfig STATIC ${CONFIG_SRC})
target_include_directories(FPGAconfig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})