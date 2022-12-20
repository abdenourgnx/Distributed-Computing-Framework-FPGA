#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <utility>
#include <cassert>
#include <cstddef>
#include <ios>
#include <ostream>




namespace utils {

    struct DataChunk{
        uint32_t id;
        uint32_t size;
        uint8_t* data;  
    };

    std::string execCommand(const char* cmd);

    bool writeFile(const char* file, uint32_t* buffer, uint32_t size);

    std::pair<uint32_t*, int> readFile(const char* file);

    void printMatrix(uint32_t *matrix , uint32_t size);

}
