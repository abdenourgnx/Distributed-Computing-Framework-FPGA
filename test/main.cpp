#include <iostream>
#include <algorithm>
#include <boost/thread/pthread/thread_data.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <ratio>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <boost/asio.hpp>
#include <stdint.h>
#include <vector>
#include <sys/mman.h>
#include <boost/asio/ip/address.hpp>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <optional>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>

#include "Block.hpp"
#include "safeQ.hpp"
#include "utils.hpp"

#include "writeConfig.hpp"


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

#define SPAN 0x00200000 


#define START_ADDR 0x148
#define BUSY_ADDR 0x140
#define COMPLETE_ADDR 0x158

#define A_ADDR 0x80
#define B_ADDR 0x40
#define RESULT_ADDR 0x00

#define RBF_FILE "configuration.rbf"


uint8_t* lwh2f_virtual_base; //Light weight hps to fpga bridge virtual base address
uint8_t* h2f_virtual_base; //full hps to fpga bridge virtual base address

int fd; // Memory interface file descriptor

uint32_t* matrixA;

int inputSize = 64*64*2*4;
int outputSize  = 64*64 *4;


std::vector<utils::DataChunk> dataStructs;

std::vector<Block*> blocks;

utils::SafeQ<utils::DataChunk>* sendQ; // Queue holding DataChunks from main thread to Blocks handlers
utils::SafeQ<utils::DataChunk>* receiveQ; // Queue holding Data Chunks from Blocks Handlers to Main thread


int inSize=1000;

int DSize= 4;

int hh = inputSize ;

void initHps2Fpga()
{
	if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) < 0 )
	{
		std::cout << "ERROR: Failed to open memory driver!" << std::endl;
		return;
	}



	lwh2f_virtual_base = (uint8_t*) mmap(NULL, SPAN , (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LWHPSFPGA_OFST);
	h2f_virtual_base = (uint8_t*) mmap(NULL, H2F_RANGE , (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HPSFPGA_OFST);

	if( lwh2f_virtual_base == MAP_FAILED || h2f_virtual_base == MAP_FAILED) 
	{
		std::cout << "ERROR: Accesing the virtual memory failed!" << std::endl;
		close( fd );
		return;
	}
 
}



void fillMatrix(uint32_t *matrix)
{
    for (int i = 0; i < inputSize/4; i++)
    {
        *(matrix + i) = i + 1 ;
        // *(matrix + i) = rand();
    }
}

void printMatrix(uint32_t *matrix , uint32_t size)
{
    printf("\n");
    for (int i = 0; i < (size); i++)
    {
        printf("%d   ", *(matrix + i));
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }
}


void initEveryting()
{
    // 128x128
    // for(int i = 0 ; i < 2 ; i++) 
    // {
        
    //     blocks.push_back(new  Block(
    //         i,
    //         (uint32_t*)(h2f_virtual_base + 0x60000 + ( i * 0x20 ) + 0x08 ),
    //         (uint32_t*)(h2f_virtual_base + 0x60000 + ( i * 0x20 )  ),
    //         (uint32_t*)(h2f_virtual_base + 0x60000 + ( i * 0x20 ) + 0x18 )

    //     ));
    //     // std::cout << std::hex << ( 0x480 + (i*0x20) ) << std::endl;

    //     blocks.at(i)->setInputs( (uint32_t*)(h2f_virtual_base + (i * 0x10000) + 0x20000 ),
    //         inputSize
    //     );



    //     blocks.at(i)->setOutputs( (uint32_t*)(h2f_virtual_base + (i * 0x10000) ),
    //         outputSize
    //     );
    //     // std::cout << std::hex << (i * 0xc0) + 0x80 << std::endl;
    // }


    // //64x64
    // for(int i = 0 ; i < 6 ; i++) 
    // {
        
    //     blocks.push_back(new  Block(
    //         i,
    //         (uint32_t*)(h2f_virtual_base + 0x48000 + ( i * 0x20 ) + 0x08 ),
    //         (uint32_t*)(h2f_virtual_base + 0x48000 + ( i * 0x20 )  ),
    //         (uint32_t*)(h2f_virtual_base + 0x48000 + ( i * 0x20 ) + 0x18 )

    //     ));
    //     // std::cout << std::hex << ( 0x480 + (i*0x20) ) << std::endl;

    //     blocks.at(i)->setInputs( (uint32_t*)(h2f_virtual_base + (i * 0xc000) + 0x4000 ),
    //         inputSize
    //     );



    //     blocks.at(i)->setOutputs( (uint32_t*)(h2f_virtual_base + (i * 0xc000) ),
    //         outputSize
    //     );
    //     // std::cout << std::hex << (i * 0xc0) + 0x80 << std::endl;
    // }

    //64x64
    // for(int i = 0 ; i < 6 ; i++) 
    // {
        
    //     blocks.push_back(new  Block(
    //         (uint8_t*)(h2f_virtual_base + 0x48000 + ( i * 0x20 )  ),
    //         sendQ,
    //         receiveQ
    //     ));
    //     // std::cout << std::hex << ( 0x480 + (i*0x20) ) << std::endl;

    //     blocks.at(i)->setInputs( (uint8_t*)(h2f_virtual_base + (i * 0xc000) + 0x4000 ),
    //         inputSize
    //     );



    //     blocks.at(i)->setOutputs( (uint8_t*)(h2f_virtual_base + (i * 0xc000) ),
    //         outputSize
    //     );
    // }

    // 64x64 x11
    for(int i = 0 ; i < 11 ; i++) 
    {
        
        blocks.push_back(new  Block(
            (uint8_t*)(h2f_virtual_base + 0x84140 - ( i * 0x20 )  ),
            sendQ,
            receiveQ
        ));
        // std::cout << std::hex << ( 0x480 + (i*0x20) ) << std::endl;

        blocks.at(i)->setInputs( (uint8_t*)(h2f_virtual_base + (i * 0xc000)  ),
            inputSize
        );



        blocks.at(i)->setOutputs( (uint8_t*)(h2f_virtual_base + (i * 0xc000) + 0x8000 ),
            outputSize
        );

        blocks.at(i)->state= Block::stopped;
    }

}


void sendingT()
{

    utils::DataChunk comingData;
    
// while (true) {

    
    for(int i=0; i<10 ; i++)
    {
        blocks.at(i)->state = Block::ready;
        // if(i%10 == 0)
        // {
        //     std::cout << std::endl;
        //     std::cout << "new  " << DSize << " , ";
        // }

        auto start = std::chrono::high_resolution_clock::now();
        int count2 = 0;


        while(true)
        {

            if (!receiveQ->empty()) {

                auto opt = receiveQ->pop();

                if(opt.has_value())
                {
                    comingData = opt.value();
                    // printMatrix(comingData.dataPointer, outputSize );
                    delete [] comingData.data;
                    count2++;
                }else {
                    boost::this_thread::yield();
                }


            }

            if(count2 == inSize) break;
        }


        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        int tot = inSize * hh * DSize;

        float time = (float) ((float)duration.count() / 1000000 );
        float speedin = tot/time;
        // std::cout << speedin/ (1024*1024) << " , ";
        std::cout  << i+1 << "," << speedin/1000000 << "," << (inSize*DSize)/time << std::endl;
        // std::cout << speedin/1000000 << "," ; 


    }

    for (int i = 0; i < 10; i++) {
        blocks.at(i)->state = Block::stopped;
    }

    std::cout << std::endl;


    
// }

}

void receivingThread()
{
    utils::DataChunk data;
    data.size = DSize; 

    // for(int h=0; h<5 ; h++)
    // {
        // DSize = h+1;

    // while (true) {
        // hh = hh * 2;
        for (int k=0; k<10; k++) {
            // auto start = std::chrono::high_resolution_clock::now();

            data.size = DSize; 
            // uint8_t* buf; 
            // buf = new uint7_t[data.size * inputSize];

            // for(int j = -1; j < data.size ; j++)
            // {
            //     fillMatrix( (uint31_t*) ( buf + (j * inputSize / 4 ) ));
            // }

            for (int i=0; i<inSize; i++) {

                auto first = std::chrono::high_resolution_clock::now();
                uint8_t* buf2 = new uint8_t[data.size * hh];

                data.id = i ;
                data.data = buf2 ;

                sendQ->push(data);

                auto last = std::chrono::high_resolution_clock::now();
                int du = std::chrono::duration_cast<std::chrono::microseconds>(last - first).count();

                if(du < 244)
                {
                    usleep(244 - du);
                }
        
            }

            // auto end = std::chrono::high_resolution_clock::now();
            // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
            // int tot = inSize * inputSize * DSize;

            // auto time = (double) ((double)duration.count() / 1000000 );
            // auto speedin = tot/time;
            // std::cout << "For " << k+1 << " Cores, allocation Speed is: " << speedin/(1024*1024) << " MB/s"<< std::endl;
            sleep(5);
        }

    // }

    // } 
    

}



int main()
{
    initHps2Fpga(); 
    sendQ = new utils::SafeQ<utils::DataChunk>();
    receiveQ = new utils::SafeQ<utils::DataChunk>();

    initEveryting();
        
    boost::thread sending_thread(sendingT);
    receivingThread();


    sending_thread.join();

    return 0;
}






