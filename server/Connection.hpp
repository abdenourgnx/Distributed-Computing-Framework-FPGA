#pragma once

#include <iostream>
#include <string>
#include <tuple>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#include "Message.hpp"


namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;
using asio::ip::tcp;


class Connection
{
public:
    Connection(tcp::socket sock)
        : _sock(std::move(sock))
    {
        _id = ID::from_endpoint(_sock.remote_endpoint());
        std::cout << "Connection " << _id.to_string() << " | Created" << std::endl;
    }

    ~Connection()
    {
        this->close();
        std::cout << "Connection " << _id.to_string() << " | Closed" << std::endl;
    }

public:
    awaitable<std::tuple<boost::system::error_code, Message::message_s*>>
    receive_message()
    {
        Message::header_u header;

        auto ec = co_await _read_exactly(header.as_bytes, sizeof(header));

        if (ec)
        {
            co_return std::make_tuple(ec, nullptr);
        }
        
        if (!header.is_valid())
        {
            std::cerr << "Connection " << _id.to_string() << " | ";
            std::cerr << "Received invalid header:";

            for (int i=0; i<sizeof(header); i++)
                std::cerr << " 0x" << to_hex(header.as_bytes[i]);

            std::cerr << std::endl;

            co_return std::make_tuple(asio::error::fault, nullptr);
        }
        
        auto message = Message::message_s::new_message(header.payload_size);
        message->header.type = header.type;

        ec = co_await _read_exactly(message->payload.as_bytes, header.payload_size);

        if (ec)
        {
            message->destroy();
            co_return std::make_tuple(ec, nullptr);
        }

        if (!message->is_valid_message())
        {
            std::cerr << "Connection " << _id.to_string() << " | ";
            std::cerr << "Received invalid message" << std::endl;
            message->destroy();
            co_return std::make_tuple(asio::error::fault, nullptr);
        }

        co_return std::make_tuple(ec, message);
    }

    awaitable<boost::system::error_code> send_message(Message::message_s* message)
    {
        if (!message->is_valid_message())
        {
            std::cerr << "Connection " << _id.to_string() << " | ";
            std::cerr << "Trying to send invalid message" << std::endl;
            co_return asio::error::fault;
        }

        co_return co_await _write_exactly(message->as_bytes, message->get_message_size());
    }

    inline void close() { _sock.close(); }

    inline bool is_closed() { return !_sock.is_open(); }


public:
    union ID
    {
        struct
        {
            uint8_t ip[4];
            uint16_t port;
        };

        uint64_t i64;

        static
        ID from_endpoint(tcp::endpoint ep)
        {
            ID id;

            id.ip[0] = (uint8_t)ep.address().to_v4().to_bytes().at(0);
            id.ip[1] = (uint8_t)ep.address().to_v4().to_bytes().at(1);
            id.ip[2] = (uint8_t)ep.address().to_v4().to_bytes().at(2);
            id.ip[3] = (uint8_t)ep.address().to_v4().to_bytes().at(3);
        
            id.port = ep.port();

            return id;
        }

        std::string to_string()
        {
            std::string out;

            out += std::to_string(ip[0]) + '.';
            out += std::to_string(ip[1]) + '.';
            out += std::to_string(ip[2]) + '.';
            out += std::to_string(ip[3]) + ':';
            out += std::to_string(port);
            
            return out;
        }

        bool operator< (const ID& other) const
        {
            return this->i64 < other.i64;
        }
    };

public:
    inline ID get_id() { return _id; }


private:
    awaitable<boost::system::error_code> _read_exactly(uint8_t* buf, uint32_t length)
    {
        auto [ec, bytes_received] = co_await _sock.async_read_some(asio::buffer(buf, length), as_tuple(use_awaitable));
    
        if (ec)
            co_return ec;

        if (bytes_received < length)
            ec = co_await _read_exactly(buf + bytes_received, length - bytes_received);

        co_return ec;
    }

    awaitable<boost::system::error_code> _write_exactly(uint8_t* buf, uint32_t length)
    {
        auto [ec, bytes_sent] = co_await _sock.async_write_some(asio::buffer(buf, length), as_tuple(use_awaitable));

        if (ec)
            co_return ec;

        if (bytes_sent < length)
            ec = co_await _write_exactly(buf + bytes_sent, length - bytes_sent);

        co_return ec;
    }

private:
    tcp::socket _sock;
    ID _id;
};