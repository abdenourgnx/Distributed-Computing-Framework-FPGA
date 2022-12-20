#include <iostream>
#include <vector>

#include <boost/date_time.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/high_resolution_timer.hpp>
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
#include "Counter.hpp"

#include <map>

namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;
using asio::experimental::awaitable_operators::operator&&;
using asio::experimental::awaitable_operators::operator||;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using asio::ip::address;

std::map<uint32_t, unsigned long> average_tp;

awaitable<boost::system::error_code>
send_benchmarking_config(Connection& con, uint32_t input_size, uint32_t output_size, uint32_t processing_delay)
{
    std::vector<Message::block_info_s> blocks;

    auto config = Message::message_s::new_config(blocks, input_size, output_size,
                                                 (uint8_t*)&processing_delay, sizeof(processing_delay));

    auto ec = co_await con.send_message(config);

    co_return ec;
}

awaitable<boost::system::error_code> wait_response(Connection& con)
{
    auto [ec, response] = co_await con.receive_message();

    if (ec)
        co_return ec;

    if (!response->is_valid_config_resp())
        ec = asio::error::fault;

    if (response->payload.as_config_resp.status != Message::ConfigRespStatus::SUCCESS)
        ec = asio::error::fault;

    response->destroy();
    co_return ec;
}

unsigned long messages_sent = 0;
bool sending_done = false;

awaitable<double> sending_loop(Connection& con,
                               unsigned long sending_delay)
{
    std::cout << "Start sending loop" << std::endl;

    asio::deadline_timer _timer(co_await asio::this_coro::executor);

    uint32_t curr_id =0;

    auto start_time = boost::posix_time::microsec_clock::universal_time();
    auto curr_time = boost::posix_time::microsec_clock::universal_time();

    unsigned long bytes_sent =0;    

    for (;;)
    {
        _timer.expires_from_now(boost::posix_time::microseconds(sending_delay));
        co_await _timer.async_wait(use_awaitable);
        
        auto input = Message::message_s::new_data(64*64*4*2, 1, curr_id);

        auto t = boost::posix_time::microsec_clock::universal_time();

        memcpy(input->payload.as_data.data, &t, sizeof(t));

        auto ec = co_await con.send_message(input);

        if (ec)
        {
            std::cerr << "Error while sending data to server, " << ec.what() << std::endl;
            con.close();
            co_return -1;
        }

        bytes_sent += input->get_data_size();
        messages_sent++;

        input->destroy();
        curr_id++;

        curr_time = boost::posix_time::microsec_clock::universal_time();

        if ((curr_time - start_time).total_microseconds() >= (unsigned long) 10 * 1000 * 1000)
        {
            sending_done = true;
            break;
        }
    }

    std::cout << "Stopping sending loop" << std::endl;

    auto time_duration = (unsigned long)(curr_time - start_time).total_microseconds();
    auto time_duration_seconds = (double)time_duration / (1000*1000);

    auto messages_avg = messages_sent / time_duration_seconds;
    auto throughput_avg = bytes_sent / time_duration_seconds;

    std::cout << std::fixed;
    //std::cout << "\t time_duration= " << time_duration << std::endl;
    //std::cout << "\t time_duration_seconds= " << time_duration_seconds << std::endl;
    //std::cout << "\t messages_sent= " << messages_sent << std::endl;
    //std::cout << "\t messages_avg= " << messages_avg << std::endl;
    //std::cout << "\t bytes_received= " << bytes_sent << std::endl;
    std::cout << "sending_throughput= " << throughput_avg << std::endl;

    co_return throughput_avg;
}

awaitable<double> receiving_loop(Connection& con)
{
    std::cout << "Start receiving loop" << std::endl;

    auto start_time = boost::posix_time::microsec_clock::universal_time();

    unsigned long bytes_received = 0;
    unsigned long messages_received =0;
    unsigned long total_latency = 0;
    unsigned long latency_min = ULONG_MAX;
    unsigned long latency_max = 0;

    for (;;)
    {
        if (sending_done && messages_received >= messages_sent)
            break;

        auto [ec, result] = co_await con.receive_message();

        if (ec)
        {
            std::cerr << "Receiving | Error while receiving result from server, " << ec.what() << std::endl;
            con.close();
            co_return -1;
        }

        if (!result->is_valid_data())
        {
            std::cerr << "Receiving | Received non data message" << std::endl;
            result->destroy();
            con.close();
            co_return -1;
        }

        if (!result->is_valid_data(64*64*4))
        {
            std::cerr << "Receiving | Received invalid output size, data_size:" << result->get_data_size() << std::endl;
            result->destroy();
            con.close();
            co_return -1;
        }

        if (result->get_data_block_count(64*64*4) != 1)
        {
            std::cerr << "Receiving | Received invalid output_count, data_size:" << result->get_data_size() << std::endl;
            result->destroy();
            con.close();
            co_return -1;
        }

        boost::posix_time::ptime create_time;

        memcpy(&create_time, result->payload.as_data.data, sizeof(create_time));

        auto curr_time = boost::posix_time::microsec_clock::universal_time();

        auto latency = (unsigned long)(curr_time - create_time).total_microseconds();

        if (latency < latency_min)
            latency_min = latency;

        if (latency > latency_max)
            latency_max = latency;

        total_latency += latency;
        bytes_received += result->get_data_size();
        messages_received++;

        result->destroy();
    }

    std::cout << "Stopping receiving loop" << std::endl;

    auto curr_time = boost::posix_time::microsec_clock::universal_time();

    auto time_duration = (unsigned long)(curr_time - start_time).total_microseconds();
    auto time_duration_seconds = (double)time_duration / (1000*1000);

    auto latency_avg = total_latency / messages_received;
    auto messages_avg = messages_received / time_duration_seconds;
    auto throughput_avg = bytes_received / time_duration_seconds;

    std::cout << std::fixed;
    //std::cout << "\t time_duration= " << time_duration << std::endl;
    //std::cout << "\t time_duration_seconds= " << time_duration_seconds << std::endl;
    std::cout << "latency_avg= " << latency_avg << std::endl;
    std::cout << "\t latency_max= " << latency_max << std::endl;
    std::cout << "\t latency_min= " << latency_min << std::endl;
    //std::cout << "\t messages_sent= " << messages_sent << std::endl;
    //std::cout << "\t messages_avg= " << messages_avg << std::endl;
    //std::cout << "\t bytes_received= " << bytes_received << std::endl;
    std::cout << "receiving_throughput= " << throughput_avg << std::endl;

    messages_sent = 0;
    sending_done = false;

    co_return throughput_avg;
}

awaitable<void> co_main(unsigned long sending_delay)
{
    tcp::socket sock(co_await asio::this_coro::executor);

    tcp::endpoint ep(address::from_string("127.0.0.1"), 9999);

    co_await sock.async_connect(ep, use_awaitable);

    std::cout << "Connected to server" << std::endl;

    Connection con(std::move(sock));


    {
        std::cout << "Sending benchmarking configuration to server" << std::endl;

        auto ec = co_await send_benchmarking_config(con, 64*64*4*2, 64*64*4, 919);

        if (ec)
        {
            std::cerr << "Error while sending config to server, " << ec.what() << std::endl;
            co_return;
        }
    }

    std::cout << "Config message sent to server" << std::endl;

    {
        auto ec = co_await wait_response(con);

        if (ec)
        {
            std::cerr << "Error while waiting on response, " << ec.what() << std::endl;
            co_return;
        }
    }

    std::cout << "Response received from server" << std::endl;

    auto [tp_tx, tp_rx] = co_await (sending_loop(con, sending_delay) &&
                                    receiving_loop(con));

    con.close();

    std::cout << "Connection to server closed" << std::endl;
}

int main(int argc, char* argv[])
{
    std::cout << "Starting user" << std::endl;
    
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " [SENDING_DELAY]" << std::endl;
        return -1;
    }

    auto sending_delay = atoi(argv[1]);


    asio::io_context ioc;

    co_spawn(ioc,
             co_main(sending_delay),
             [](std::exception_ptr exp)
             {
                if (exp)
                    std::rethrow_exception(exp);
             });

    ioc.run();

    std::cout << "User finished" << std::endl;

    return 0;
}