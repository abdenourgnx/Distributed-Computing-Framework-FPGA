#include <iostream>
#include <vector>
#include <stdio.h>

#include <boost/date_time.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>

#include "Message.hpp"
#include "Connection.hpp"

namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;
using asio::experimental::awaitable_operators::operator||;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using asio::ip::address;


Message::message_s* curr_config;

unsigned long bytes_sent =0;
unsigned long bytes_received =0;

unsigned long messages_sent =0;
unsigned long messages_received =0;

awaitable<boost::system::error_code> send_config_response(Connection& con, Message::ConfigRespStatus status)
{
    auto response = Message::message_s::new_config_resp(status);

    co_return co_await con.send_message(response);
}

awaitable<bool> config_received(Connection& con, Message::message_s* config)
{
    curr_config = config;

    std::cout << "Config message received from server:" << std::endl;
    curr_config->print("\t");

    if (curr_config->payload.as_config.input_size < curr_config->payload.as_config.output_size ||
        curr_config->payload.as_config.input_size < 16 ||
        curr_config->payload.as_config.num_blocks ||
        curr_config->get_rbf_size() != 4)
    {
        curr_config->destroy();
        std::cerr << "Received an invalid Benchmarking Configuration" << std::endl;

        auto ec = co_await send_config_response(con, Message::ConfigRespStatus::ERROR);

        if (ec)
        {
            std::cerr << "Error while sending response to server, " << ec.what() << std::endl;
            co_return false;
        }

        std::cout << "Negative response sent to server" << std::endl;

        co_return false;

    }

    {
        auto ec = co_await send_config_response(con, Message::ConfigRespStatus::SUCCESS);

        if (ec)
        {
            std::cerr << "Error while sending response to server, " << ec.what() << std::endl;
            co_return false;
        }
    }

    std::cout << "Positive response sent to server" << std::endl;

    co_return true;
}

awaitable<void> processing_loop(Connection& con)
{
    for (;;)
    {
        std::cout << "Starting processing loop" << std::endl;

        asio::deadline_timer _timer(co_await asio::this_coro::executor);

        auto input_size = curr_config->payload.as_config.input_size;
        auto output_size = curr_config->payload.as_config.output_size;

        auto processing_delay = *((uint32_t*)curr_config->get_rbf_ptr());

        for (;;)
        {
            auto [ec, input] = co_await con.receive_message();

            if (ec == asio::stream_errc::eof)
                co_return;

            if (ec)
            {
                std::cerr << "Error while receiving input, " << ec.what() << std::endl;
                co_return;
            }

            if (!input->is_valid_data(input_size))
            {
                std::cout << "Received invalid input from server" << std::endl;
                co_return;
            }

            bytes_received += input->get_data_size();
            messages_received++;

            //std::cout << "Received input from server:" << std::endl;
            //input->print("\t");

            auto input_count = input->get_data_block_count(input_size);

            auto id = input->payload.as_data.id;

            auto output = Message::message_s::new_data(output_size, input_count, id);

            memcpy(output->payload.as_data.data, input->payload.as_data.data, 8);

            _timer.expires_from_now(boost::posix_time::microsec(processing_delay * input_count));
            co_await _timer.async_wait(use_awaitable);

            //std::cout << "Sending output to server:" << std::endl;
            //output->print("\t");

            ec = co_await con.send_message(output);

            if (ec == asio::stream_errc::eof)
                co_return;

            if (ec)
            {
                std::cout << "Error while sending output, " << ec.what() << std::endl;
                co_return;
            }

            bytes_sent += output->get_data_size();
            messages_sent++;

            input->destroy();
            output->destroy();
        }
    }
}

awaitable<void> stats()
{
    auto ddl_time = boost::posix_time::second_clock::universal_time();

    asio::deadline_timer _timer(co_await asio::this_coro::executor);

    for (;;)
    {
        _timer.expires_at(ddl_time);

        auto [ec] = co_await _timer.async_wait(as_tuple(use_awaitable));

        if (ec)
            co_return;

        std::cout << ddl_time.time_of_day().total_seconds() << ",";

        std::cout << bytes_received << ",";
        std::cout << bytes_sent << std::endl;

        bytes_received =0;
        bytes_sent =0;

        ddl_time += boost::posix_time::seconds(1);
    }
}

awaitable<void> co_main()
{
    tcp::socket sock(co_await asio::this_coro::executor);

    tcp::endpoint ep(address::from_string("127.0.0.1"), 8888);

    {
        auto [ec] = co_await sock.async_connect(ep, as_tuple(use_awaitable));

        if (ec)
        {
            std::cerr << "Error while connecting to server, " << ec.what() << std::endl;
            co_return;
        }
    }

    std::cout << "Connected to server" << std::endl;

    Connection con(std::move(sock));

    {
        auto [ec, first_config] = co_await con.receive_message();

        if (ec)
        {
            std::cerr << "Error while waiting on config, " << ec.what() << std::endl;
            co_return;
        }

        if (!first_config->is_valid_config())
        {
            std::cerr << "Error while waiting on config, Invalid config" << std::endl;
            first_config->destroy();
            co_return;
        }
    
        if (! co_await config_received(con, first_config))
            co_return;
    }

    co_await (stats() || processing_loop(con));

    con.close();

    std::cout << "Connection to server closed" << std::endl;
}

int main(int argc, char* argv[])
{
    asio::io_context ioc;

    co_spawn(ioc,
             co_main(),
             detached);

    ioc.run();

    return 0;
}