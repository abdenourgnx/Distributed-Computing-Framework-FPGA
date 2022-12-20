# SET (TEST_SRC 
#     main.cpp
#     FPGAconfig/alt_fpga_manager.c
#     FPGAconfig/alt_fpga_manager.h
#     FPGAconfig/hps.h
#     FPGAconfig/hwlib.h
#     FPGAconfig/socal.h
#     FPGAconfig/alt_fpgamgr.h
#     FPGAconfig/alt_printf.h
#     FPGAconfig/write_config.h
#     )

SET (TEST_SRC 
    main.cpp
    )

add_executable (try ${TEST_SRC})

target_link_libraries (try ${USED_LIBS})

