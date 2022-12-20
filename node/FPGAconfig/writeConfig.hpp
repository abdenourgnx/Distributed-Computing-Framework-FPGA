
#pragma once 

#include "alt_fpga_manager.h"
#include "hps.h"
#include "hwlib.h"



bool writeFPGAconfig(char* buf, int fsize)
{
	__VIRTUALMEM_SPACE_INIT();

	//	 init the FPGA Manager	 
	alt_fpga_init();

	// Take the right to controll the FPGA 
	alt_fpga_control_enable();

	ALT_FPGA_STATE_t stat = alt_fpga_state_get();


    // write new configurtion buffer
	ALT_STATUS_CODE status = alt_fpga_configure(buf, fsize);


	// Give the right to controll the FPGA
	alt_fpga_control_disable();

	// free the dynamic access memory
	__VIRTUALMEM_SPACE_DEINIT();

    return status == ALT_E_SUCCESS;

}