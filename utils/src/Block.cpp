#include "Block.hpp"
#include <boost/thread/pthread/thread_data.hpp>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <ostream>





Block::Block(uint8_t* cra, SafeQ<DataChunk>* inputQ, SafeQ<DataChunk>* outputQ ):
    _input_q(inputQ) , _output_q(outputQ) , _cr_addr(cra) 
{
        // std::cout << "init worker " << id << std::endl; 
        state = ready ;
        _block_thread = boost::thread(&Block::work, this);
}

Block::~Block()
{
}


inline void Block::start()
{
    uint8_t startbit = 1; 
    writeAddress<uint8_t>(_cr_addr + 0x8, &startbit);
}

inline bool Block::isDone()
{
    return ( ( *(_cr_addr + 0x18) & 0x02) >> 1 == 1 ) ;

}

void Block::setInputs(uint8_t* address, uint32_t length)
{
    _input_port = {address, length};
}


void Block::setOutputs(uint8_t* address, uint32_t length)
{
    _output_port = {address, length};
}

template <typename T> inline void Block::writeAddress(T* address, T* data)
{
    *(address) = *(data);  
}



template <typename T> inline void Block::readAddress(T* address, T *data)
{
    *(data) = *(address);
}


void Block::writeRange128(uint32_t* destination, uint32_t* data, uint32_t dataSize)
{
    // calculate the divisiblity of the data size over 128bit 
    int rest = dataSize % 4;


    if(rest == 0)  
    {
        //if the data size is divisable write to the fpga by 128bit at a time
        for (int i = 0; i < dataSize/4 ; i++) {
            writeAddress<uint128_t>((uint128_t*) destination + i, (uint128_t*)data + i);
        }
        
    }else {

        // if not divisable write to the fpga by the possible number of data by 128 bit
        for (int i = 0; i < (dataSize - rest) / 4 ; i++) {
            writeAddress<uint128_t>((uint128_t*) destination + i, (uint128_t*)data + i);
        }
        
        // write the rest of the data by 32bit at a time
        for (int i= dataSize - rest; i < dataSize; i++) {
            writeAddress<uint32_t>(destination + i, data + i);
        }
    } 

}


inline void Block::writeRange32(uint8_t* destination, uint8_t* data, uint32_t dataSize)
{
    // for (int i = 0; i < dataSize/4; i++) {
    //     writeAddress<uint32_t>((uint32_t*) destination + i, (uint32_t*)data + i);
    // }
    std::memcpy(destination, data, dataSize * sizeof(uint8_t));
}

void Block::work()
{

    DataChunk comingData, goingData;
    // std::cout<< "Block id: " << ID << " working" << std::endl;
    std::cout<< "Block working" << std::endl;

    while (true) {

        while(state == stopped) boost::this_thread::yield() ;

        state = busy;

        //get queue data set
        auto opt = _input_q->pop(); 

        if (!opt.has_value()) {

            //yield processor to other thread to wrok
            boost::this_thread::yield();

        }else {

            //getting data from the optional
            comingData = opt.value();

            //Create new output dataset with same size of input dataset
            //allocate memory for output
            goingData.data = new uint8_t[comingData.size * _output_port.size ];
            goingData.size = comingData.size;
            goingData.id = comingData.id;
            
            //
            // For data in dataset work 
            for (uint32_t a = 0 ; a< comingData.size; a++) {


                // writeRange32(
                //     goingData.data + (a * _output_port.size),
                //     comingData.data + (a * _input_port.size),
                //     _output_port.size
                // );


                // auto beg = std::chrono::high_resolution_clock::now();
                // write one data to fpga 
                writeRange32(
                    _input_port.address,
                    comingData.data + (a * _input_port.size),
                    _input_port.size
                );


                start();
            

                // wait till fpga return result 
                while (!isDone()) {

                    //yield processor to other threads to work
                    boost::this_thread::yield();

                }



                writeRange32(
                    goingData.data + (a * _output_port.size),
                    _output_port.address,
                    _output_port.size
                );

                // auto end = std::chrono::high_resolution_clock::now();
                // int duration = std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count();

                // std::cout << duration << std::endl;
                // printf("%d \n", duration);


            }

            // de-allocate the coming data buffer
            delete [] comingData.data;


            //Push the dataset back to the Recieve queue 
            _output_q->push(goingData);


        }

        state = done;

    }

    return;
}
