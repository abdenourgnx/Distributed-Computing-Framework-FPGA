
#include "Block.hpp"
#include "globals.hpp"
#include "utils.hpp"



void* lwh2f_virtual_base; //Light weight hps to fpga bridge virtual base address
void* h2f_virtual_base; //full hps to fpga bridge virtual base address

int fd; // Memory interface file descriptor



std::thread* Main_thread;

std::vector<Block> blocks;


utils::SafeQ<utils::DataChunk> sendQ; // Queue holding DataChunks from main thread to Blocks handlers
utils::SafeQ<utils::DataChunk> receiveQ; // Queue holding Data Chunks from Blocks Handlers to Main thread


utils::SafeQ<int> q; // Queue holding Data Chunks from Blocks Handlers to Main thread



//Establish connection
void connect()
{
}



//Initialize fd and virtual addresses
void initHps2Fpga()
{

	//open the file descriptor
	if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) < 0 )
	{
		std::cout << "ERROR: Failed to open memory driver!" << std::endl;
		return;
	}

	//Assign memory mapping to the H2F bridges
	lwh2f_virtual_base = mmap(NULL, LWH2F_RANGE , (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LWHPSFPGA_OFST);
	h2f_virtual_base = mmap(NULL, H2F_RANGE , (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HPSFPGA_OFST);

	if( lwh2f_virtual_base == MAP_FAILED || h2f_virtual_base == MAP_FAILED) 
	{
		std::cout << "ERROR: Accesing the virtual memory failed!" << std::endl;
		close( fd );
		return;
	}
 
}

// Handler Thread 
void mainThread()
{
}



int main() {

    initHps2Fpga();    

	return 0 ;
}















