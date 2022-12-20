#pragma once
#include <cstdint>
#include <stdint.h>
#include <boost/thread.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/thread.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>

#include "safeQ.hpp"
#include "utils.hpp"


using namespace boost::multiprecision;
using namespace utils;


struct Port
{
    uint8_t* address;
    uint32_t size; 
};


class Block{

private:

    Port _input_port;
    Port _output_port;

    boost::thread _block_thread ;

    SafeQ<DataChunk>* _input_q;
    SafeQ<DataChunk>* _output_q;

public:

    uint8_t* _cr_addr; 


    typedef enum {
        busy,
        done, 
        stopped,
        ready
    } State;

    State state;

    
    Block(uint8_t* cra, SafeQ<DataChunk>* inputQ, SafeQ<DataChunk>* outputQ );

    ~Block();  

    /**
     * @brief Set the input address and length of the block
     * 
     * @param address 
     * @param length 
     */
    void setInputs(uint8_t* address, uint32_t length);
    
    /**
     * @brief Set the output base address and length of the block
     * 
     * @param address 
     * @param length 
     */
    void setOutputs(uint8_t* address, uint32_t length);


private:

    /**
     * @brief Main function of the Block handler send and 
             recieve data to and from fpga
     * 
     */
    void work();

    /**
     * @brief Write to start bit in fpga block
     * 
     */
    inline void start();

    /**
     * @brief check if fpga block is done working 
     * 
     * @return true done
     * @return false still working  
     */
    inline bool isDone();


    /**
     * @brief write to an address in the FPGA span
     * 
     * @tparam T the type of memory access (32 bit or 64 bit or 128 bit)
     * @param address 
     * @param data 
     */
    template <typename T> inline void writeAddress(T* address, T *data);

    /**
     * @brief Read from an address in the FPGA address span
     * 
     * @tparam T the type of memory access (32 bit or 64 bit or 128 bit)
     * @param address 
     * @param data 
     */
    template <typename T> inline void readAddress(T* address, T *data);


    /**
     * @brief This function is used to write into a range of addresses
        created specialy to use the full performance 128 bit H2F bridge
     * 
     * @param address pointer to start writing address 
     * @param dataAddress pointer to start data address 
     * @param dataSize Range 
     */
    void writeRange128(uint32_t* destination, uint32_t* data, uint32_t dataSize);

    inline void writeRange32(uint8_t* destination, uint8_t* data, uint32_t dataSize);


};

