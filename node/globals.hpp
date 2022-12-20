
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <boost/asio.hpp>
#include <stdint.h>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <boost/asio/ip/address.hpp>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <ostream>
#include <boost/optional.hpp>


#include "Block.hpp"
#include "safeQ.hpp"
#include "utils.hpp"



#define LWHPSFPGA_OFST  0xff200000 // LWHPS2FPGA Bridge 
#define HPSFPGA_OFST    0xC0000000 // HPS2FPGA Bridge 
#define MPU_OFSET		0x0        // MPU (HPS Address space)

// Bridge interface End address 
#define LWHPSFPGA_END   0xFF3FFFFF
#define HPSFPGA_END     0xFBFFFFFF
#define MPU_END         0xFFFFFFFF

// Bridge interface range (allowed input offset)
#define LWH2F_RANGE    (LWHPSFPGA_END - LWHPSFPGA_OFST)
#define H2F_RANGE      (HPSFPGA_END - HPSFPGA_OFST)
#define MPU_RANGE      (MPU_END - MPU_OFSET)

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)


//tracking node state
typedef enum 
{
    STOPED,
    WORKING,
    CONNECTING,
    CONFIGURING 

} State;


