#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>


const char hex_lut[] = {'0', '1', '2', '3',
                        '4', '5', '6', '7',
                        '8', '9', 'A', 'B',
                        'C', 'D', 'E', 'F'};

template <typename T>
std::string to_hex(T val)
{
    std::string out = "";

    for (int i=0; i < sizeof(T) * 2; i++)
    {
        out += hex_lut[val & 0xF];
        val = val >> 4;
    }

    std::reverse(out.begin(), out.end());
    
    return out;
}

std::string bytes_to_hex(uint8_t* buf, std::size_t count, std::string pre ="0x", std::string sep =" ")
{
    std::string out ="";

    for (int i=0; i < count; i++)
    {
        if (i)
            out += sep;

        out += pre;
        out += to_hex(*(buf + i));
    }

    return out;
}


void print_bytes(uint8_t* data, std::size_t length, uint bytes_per_line =4, std::string pre ="\t")
{
    if (length > 8 * bytes_per_line)
    {
        for (int i=0; i < (4 * bytes_per_line); i+=bytes_per_line)
            std::cout << pre << bytes_to_hex(data + i, bytes_per_line) << std::endl;
    
        std::cout << pre << "..." << std::endl;

        for (int i=(length - 4 * bytes_per_line); i < length; i+=bytes_per_line)
            std::cout << pre << bytes_to_hex(data + i, bytes_per_line) << std::endl;

    }else
    {
        for (int i=0; i < length; i+=bytes_per_line)
        {
            std::cout << pre;

            if (i + bytes_per_line < length)
                std::cout << bytes_to_hex(data + i, bytes_per_line);
            else
                std::cout << bytes_to_hex(data + i, length - i);

            std::cout << std::endl;
        }
    }
}


template <typename T>
std::string to_hex_as_bytes(T val, std::string pre ="0x", std::string sep =" ")
{
    return bytes_to_hex((uint8_t*)&val, sizeof(T), pre=pre, sep=sep);
}

// Set structures alignement to 1 byte (avoid padding)
#pragma pack(push)
#pragma pack(1)

namespace Message
{
    enum class MessageType: uint8_t
    {
        UNDEFINED = 0U,
        CONFIG,
        CONFIG_RESP,
        DATA
    };

    enum class ConfigRespStatus: uint8_t
    {
        SUCCESS = 0U,
        ERROR
    };

    struct block_info_s
    {
        uint32_t control_base;
        uint32_t input_base;
        uint32_t output_base;
    };

    struct alloc_info_s
    { 
        unsigned long alloc_size;
    };

    union header_u
    {
        uint8_t as_bytes[0];

        struct
        {
            MessageType type;
            uint8_t _reserved[sizeof(uint32_t) - sizeof(MessageType)];
            uint32_t payload_size;
        };
        
        inline bool is_valid()
        {
            return ((type == MessageType::CONFIG ||
                     type == MessageType::CONFIG_RESP ||
                     type == MessageType::DATA) &&
                    payload_size > 0);
        }
    };

    union payload_u
    {
        uint8_t as_bytes[0];
        
        struct config_s
        {
            uint32_t input_size;
            uint32_t output_size;
            uint32_t num_blocks;
            uint8_t data[0];
        }as_config;

        struct config_resp_s
        {
            ConfigRespStatus status;
        }as_config_resp;

        struct data_s
        {
            uint32_t id;
            uint8_t data[0];
        }as_data;
    };
    

    struct message_s
    {

    private:
        alloc_info_s alloc_info;

    public:
        union
        {
            uint8_t as_bytes[0];

            struct
            {
                header_u header;
                payload_u payload;
            };
        };

        inline uint32_t get_message_size()
        {
            return sizeof(header_u) + header.payload_size;
        }

        inline bool has_valid_payload_size()
        {
            return alloc_info.alloc_size == sizeof(alloc_info_s) + get_message_size();
        }

        inline bool is_valid_message()
        {
            return (header.is_valid() &&
                    has_valid_payload_size());
        }

        inline bool is_valid_config()
        {
            return (is_valid_message() &&
                    header.type == MessageType::CONFIG &&
                    header.payload_size >= sizeof(payload_u::config_s) +
                                           payload.as_config.num_blocks * sizeof(block_info_s));
        }

        inline block_info_s* get_block_info(uint32_t block_index)
        {
            return (block_info_s*) (payload.as_config.data + block_index * sizeof(block_info_s));
        }

        inline uint32_t get_rbf_offset()
        {
            return payload.as_config.num_blocks * sizeof(block_info_s);
        }

        inline uint32_t get_rbf_size()
        {
            return header.payload_size - sizeof(payload_u::config_s) - get_rbf_offset();
        }

        inline uint8_t* get_rbf_ptr()
        {
            return payload.as_config.data + get_rbf_offset();
        }

        inline bool is_valid_config_resp()
        {
            return (is_valid_message() &&
                    header.type == MessageType::CONFIG_RESP &&
                    header.payload_size == 1 &&
                    (payload.as_config_resp.status == ConfigRespStatus::SUCCESS ||
                     payload.as_config_resp.status == ConfigRespStatus::ERROR));
        }

        inline bool is_valid_data()
        {
            return (is_valid_message() &&
                    header.type == MessageType::DATA);
        }

        inline bool is_valid_data(uint32_t data_block_size)
        {
            return (is_valid_message() &&
                    header.type == MessageType::DATA &&
                    get_data_size() % data_block_size == 0);
        }

        inline uint32_t get_data_size()
        {
            return header.payload_size - sizeof(payload_u::data_s);
        }

        inline uint32_t get_data_block_count(uint32_t data_block_size)
        {
            return get_data_size() / data_block_size;
        }

        static message_s* new_message(uint32_t payload_size)
        {
            
            auto alloc_size = sizeof(alloc_info_s) + sizeof(header_u) + payload_size;

            /*
            std::cout << "alloc | raw message | ";
            std::cout << "payload_size: " << payload_size << " | ";
            std::cout << "alloc_size: " << alloc_size << std::endl;
            */

            auto msg = (message_s*) new (std::nothrow) uint8_t[alloc_size];

            while (!msg)
            {
                std::cerr << "Message | cannot allocate: " << alloc_size << " Bytes" << std::endl;
                msg = (message_s*) new (std::nothrow) uint8_t[alloc_size];
            }

            msg->alloc_info.alloc_size = alloc_size;
            msg->header.type = MessageType::UNDEFINED;
            msg->header.payload_size = payload_size;

            return msg;
        }

        static message_s* new_config(std::vector<block_info_s> blocks,
                                     uint32_t input_size =0,
                                     uint32_t output_size =0,
                                     uint8_t* rbf_data =nullptr,
                                     uint32_t rbf_size =0)
        {
            auto num_blocks = blocks.size();

            auto payload_size = sizeof(payload_u::config_s) +
                                num_blocks * sizeof(block_info_s) +
                                rbf_size;

            auto alloc_size = sizeof(alloc_info_s) + sizeof(header_u) + payload_size;

            /*
            std::cout << "alloc | config message | ";
            std::cout << "num_blocks: " << num_blocks << " | ";
            std::cout << "input_size: " << input_size << " | ";
            std::cout << "output_size: " << output_size << " | ";
            std::cout << "rbf_size: " << rbf_size << " | ";
            std::cout << "payload_size: " << payload_size << " | ";
            std::cout << "alloc_size: " << alloc_size << std::endl;
            */

            auto config = (message_s*) new uint8_t[alloc_size];

            config->alloc_info.alloc_size = alloc_size;

            config->header.type = MessageType::CONFIG;
            config->header.payload_size = payload_size;

            config->payload.as_config.input_size = input_size;
            config->payload.as_config.output_size = output_size;
            config->payload.as_config.num_blocks = num_blocks;

            for (int i=0; i < num_blocks; i++)
                *(config->get_block_info(i)) = blocks[i];

            if (rbf_data)
                std::memcpy(config->get_rbf_ptr(), rbf_data, rbf_size);

            return config;
        }

        static message_s* new_config_resp(ConfigRespStatus status =ConfigRespStatus::SUCCESS)
        {
            auto payload_size = sizeof(payload_u::config_resp_s);

            auto alloc_size = sizeof(alloc_info_s) + sizeof(header_u) + payload_size;

            /*
            std::cout << "alloc | config_resp message | ";
            std::cout << "payload_size: " << payload_size << " | ";
            std::cout << "alloc_size: " << alloc_size << std::endl;
            */

            auto resp = (message_s*) new uint8_t[alloc_size];

            resp->alloc_info.alloc_size = alloc_size;

            resp->header.type = MessageType::CONFIG_RESP;
            resp->header.payload_size = payload_size;

            resp->payload.as_config_resp.status = status;

            return resp;
        }

        static message_s* new_data(uint32_t data_size, uint32_t data_count, uint32_t id =0)
        {
            uint32_t payload_size = sizeof(payload_u::data_s) + (data_size * data_count);

            auto alloc_size = sizeof(alloc_info_s) + sizeof(header_u) + payload_size;

            /*
            std::cout << "alloc | input message | ";
            std::cout << "input_size: " << input_size << " | ";
            std::cout << "input_count: " << input_count << " | ";
            std::cout << "ID: 0x" << to_hex(id) << " | ";
            std::cout << "payload_size: " << payload_size << " | ";
            std::cout << "alloc_size: " << alloc_size << std::endl;
            */

            auto data = (message_s*) new uint8_t[alloc_size];

            data->alloc_info.alloc_size = alloc_size;

            data->header.type = MessageType::DATA;
            data->header.payload_size = payload_size;

            data->payload.as_data.id = id;

            return data;
        }

        void destroy()
        {
            delete[] this;
        }

        void print(std::string prefix ="")
        {
            std::cout << prefix << "Type:" << "\t";
            std::cout << bytes_to_hex((uint8_t*)&(this->header.type), 4);

            switch (this->header.type)
            {
            case MessageType::UNDEFINED:
                std::cout << " (UNDEFINED)";
                break;
            
            case MessageType::CONFIG:
                std::cout << " (CONFIG)";
                break;

            case MessageType::CONFIG_RESP:
                std::cout << " (CONFIG_RESP)";
                break;

            case MessageType::DATA:
                std::cout << " (DATA)";
                break;

            default:
                std::cout << " (UNKNOWN)";
                break;
            }

            std::cout << std::endl;
            
            std::cout << prefix << "Size:" << "\t";
            std::cout << to_hex_as_bytes(this->header.payload_size);
            std::cout << " (" << this->header.payload_size << ")" << std::endl;


            switch (header.type)
            {
            case MessageType::UNDEFINED:

                std::cout << prefix << "Data:";
                print_bytes(payload.as_bytes, header.payload_size, 4, prefix + "\t");

                break;
            
            case MessageType::CONFIG:
                
                std::cout << prefix << "iSize:" << "\t";
                std::cout << to_hex_as_bytes(payload.as_config.input_size);
                std::cout << " (" << payload.as_config.input_size << ")" << std::endl;

                std::cout << prefix << "oSize:" << "\t";
                std::cout << to_hex_as_bytes(payload.as_config.output_size);
                std::cout << " (" << payload.as_config.output_size << ")" << std::endl;

                std::cout << prefix << "nBlk:" << "\t";
                std::cout << to_hex_as_bytes(payload.as_config.num_blocks);
                std::cout << " (" << payload.as_config.num_blocks << ")" << std::endl;

                for (int i=0; i < payload.as_config.num_blocks; i++)
                {
                    std::cout << prefix << "Blk #" << i + 1 << std::endl;

                    auto blk = get_block_info(i);

                    std::cout << prefix << "cBase:" << "\t";
                    std::cout << to_hex_as_bytes(blk->control_base);
                    std::cout << " (" << blk->control_base << ")" << std::endl;

                    std::cout << prefix << "iBase:" << "\t";
                    std::cout << to_hex_as_bytes(blk->input_base);
                    std::cout << " (" << blk->input_base << ")" << std::endl;

                    std::cout << prefix << "oBase:" << "\t";
                    std::cout << to_hex_as_bytes(blk->output_base);
                    std::cout << " (" << blk->output_base << ")" << std::endl;
                }
                
                {
                    auto rbf_ptr = get_rbf_ptr();

                    std::cout << prefix << "RBF:" << std::endl;
                    print_bytes(rbf_ptr, get_rbf_size(), 16, prefix + "\t");
                }

                break;
            
            case MessageType::CONFIG_RESP:

                std::cout << prefix << "Status:" << "\t";

                std::cout << to_hex_as_bytes(payload.as_config_resp.status);

                std::cout << " (";
                std::cout << (payload.as_config_resp.status == ConfigRespStatus::SUCCESS ? "SUCCESS": "ERROR");
                std::cout << ")" << std::endl;


                break;
            
            case MessageType::DATA:

                std::cout << prefix << "ID:" << "\t";
                std::cout << to_hex_as_bytes(payload.as_data.id);
                std::cout << " (0x" << to_hex(payload.as_data.id) << ")" << std::endl;

                std::cout << prefix << "Data:" << std::endl;
                print_bytes(payload.as_data.data, get_data_size(), 4, prefix + "\t");

                break;

            default:
                break;
            }
 
        }

    };
}

#pragma pack(pop)