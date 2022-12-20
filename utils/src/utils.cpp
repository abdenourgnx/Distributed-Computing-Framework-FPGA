#include "utils.hpp"

std::string utils::execCommand(const char* cmd)
{
    std::array<char, 200> buffer;
    std::string result;
    std::unique_ptr< FILE, decltype(&pclose) >  pipe(popen(cmd, "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void utils::printMatrix(uint32_t *matrix , uint32_t size)
{
    printf("\n");
    for (int i = 0; i < (size); i++)
    {
        printf("%d   ", *(matrix + i));
        if ((i + 1) % 4 == 0)
        {
            printf("\n");
        }
    }
}


bool utils::writeFile(const char* file, uint32_t* buffer, uint32_t size)
{
   std::ofstream myFile;
   myFile.open(file, std::ios::out | std::ios::binary);
//    if(myFile.is_open())
//    {
//         std::cout << "couldn't open file" << std::endl;
//         myFile.close();
//         return false;
//    }else {
    myFile.write((char*)buffer, size); 
    myFile.close();
    return true;
//    }

//    myFile.close();
//    return false;
}

std::pair<uint32_t*, int> utils::readFile(const char *file)
{
    std::ifstream is(file, std::ifstream::binary) ;

    assert(is);
    is.seekg (0, is.end);
    int length = is.tellg();
    is.seekg (0, is.beg);

    char buffer[length]; 

    is.read(buffer, length);
    is.close();

    return std::make_pair((uint32_t*) buffer, length);


}