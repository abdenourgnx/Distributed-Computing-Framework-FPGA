#pragma once

#include <iostream>
#include <memory>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "Message.hpp"
#include "Connection.hpp"
#include "Event.hpp"
#include "Counter.hpp"
#include "Stack.hpp"


namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;
using asio::experimental::awaitable_operators::operator||;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;


class User
{
public:
    User(tcp::socket sock)
        : _con(std::move(sock)),
          _nodes_stack(sock.get_executor()),
          _config(nullptr),
          _config_response_sent(false),
          _data_counter(sock.get_executor()),
          _node_receiver_counter(sock.get_executor()),
          _stop_receiving_ev(sock.get_executor())
    {
        _dbg_pre = std::string("User: ") + _con.get_id().to_string() + " | ";
        std::cout << _dbg_pre << "Created" << std::endl;
    }

    ~User()
    {
        _con.close();

        std::cout << _dbg_pre << "Destroyed" << std::endl;
    }

    awaitable<void> start(std::vector<std::shared_ptr<Connection>> nodes)
    {
        std::cout << _dbg_pre << "Started" << std::endl;
        // Wait for first config
        {
            std::cout << _dbg_pre << "Waiting for first config" << std::endl;

            auto [ec, first_config] = co_await _con.receive_message();

            if (ec)
            {
                std::cerr << _dbg_pre << "Error on receiving first config, ec=" << ec.what() << std::endl;
                co_return;
            }

            std::cout << _dbg_pre << "Received config:" << std::endl;
            first_config->print("\t");

            if (!first_config->is_valid_config())
            {
                first_config->destroy();
                co_return;
            }

            // Store configuration message
            _config = first_config;
        }

        for (auto node: nodes)
            _nodes_stack.push(node);
        
        if (! co_await _configure_nodes())
            co_return;

        std::cout << _dbg_pre << "Starting main loop" << std::endl;

        co_await _main_loop();

        _config->destroy();
    }

    awaitable<bool> _configure_nodes()
    {
        std::cout << _dbg_pre << "Starting nodes configuration" << std::endl;

        Counter config_counter(co_await asio::this_coro::executor, "Config Counter | ");

        //Queue<std::shared_ptr<Connection>> nodes_tmp(co_await asio::this_coro::executor);


        // configuring nodes
        while (!_nodes_stack.empty())
        {
            auto node = _nodes_stack.pop();

            // increment config counter
            config_counter.incr();

            std::cout << _dbg_pre << "Configuring node " << node->get_id().to_string() << std::endl;

            // spawn configuration on node - decrement config counter on finnish
            co_spawn(co_await asio::this_coro::executor,
                     _configure_node(node),
                     [this, &config_counter, node](std::exception_ptr exp, boost::system::error_code ec)
                     {
                        if (ec)
                        {
                            std::cerr << _dbg_pre << "Error on configuring node " << node->get_id().to_string();
                            std::cerr << ", ec=" << ec.what() << std::endl;
                        }else
                        {
                            std::cout << _dbg_pre << "Node " << node->get_id().to_string() << " configured" << std::endl;
                            this->_nodes_stack.push(node);
                        }

                        config_counter.decr();
                     });
        }

        std::cout << _dbg_pre << "Waiting for nodes to be configured" << std::endl;

        // wait for config counter to drop to zero
        co_await config_counter.async_wait_zero();

        std::cout << _dbg_pre << "Nodes configuration finished" << std::endl;

        // send config response to user
        auto ec = co_await _send_config_response();

        if (ec)
        {
            std::cerr << _dbg_pre << "Error while sending configuration response to user, " << ec.what() << std::endl;
            co_return false;
        }

        std::cout << _dbg_pre << "Configuration response sent to user" << std::endl;

        co_return true;
    }

    awaitable<boost::system::error_code> _configure_node(std::shared_ptr<Connection> node)
    {
        // send configuration message to node
        {
            auto ec = co_await node->send_message(_config);

            if (ec)
                co_return ec;
        }

        // wait for configuration response
        auto [ec, response] = co_await node->receive_message();
        
        if (ec)
            co_return ec;

        if (!response->is_valid_config_resp())
            ec = asio::error::fault;

        if (response->payload.as_config_resp.status != Message::ConfigRespStatus::SUCCESS)
            ec = asio::error::fault;

        response->destroy();
        co_return ec;
    }

    awaitable<boost::system::error_code> _send_config_response()
    {
        auto response = Message::message_s::new_config_resp();

        auto ec = co_await _con.send_message(response);

        response->destroy();

        co_return ec;
    }
    
    awaitable<bool> _main_loop()
    {
        {
            std::vector<std::shared_ptr<Connection>> nodes_tmp;

            while (!_nodes_stack.empty())
                nodes_tmp.push_back(_nodes_stack.pop());

            // start receiving loops for all nodes
            for (auto node: nodes_tmp)
            {
                std::cout << _dbg_pre << "Starting receiving loop on node " << node->get_id().to_string() << std::endl;

                _node_receiver_counter.incr();

                // spawn receiver loop
                co_spawn(co_await asio::this_coro::executor,
                        _receiver_loop(node),
                        [this, node](std::exception_ptr exp)
                        {
                            std::cout << _dbg_pre << "Receiving loop on ";
                            std::cout << node->get_id().to_string();
                            std::cout << " stopped" << std::endl;

                            this->_node_receiver_counter.decr();
                        });

                _nodes_stack.push(node);
            }
        }

        // loop
        for (;;)
        {
            // wait for message from user
            auto [ec, message] = co_await _con.receive_message();

            if (ec == asio::error::eof)
            {
                std::cout << _dbg_pre << "User connection closed" << std::endl;
                co_return false;
            }

            if (ec)
            {
                std::cerr << _dbg_pre << "Error on main loop, " << ec.what() << std::endl;
                co_return false;
            }

            // is input
            if (message->is_valid_data(_config->payload.as_config.input_size))
            {
                // increment data counter
                _data_counter.incr();

                co_await _dispatch_input(message);

                continue;
            }
            
            // is config
            if (message->is_valid_config())
            {
                std::cout << _dbg_pre << "Received new configuration:" << std::endl;
                message->print("\t");

                // Save configuration message
                _config->destroy();
                _config = message;

                std::cout << _dbg_pre << "Waiting for data counter to drop to zero" << std::endl;
                co_await _data_counter.async_wait_zero();

                std::cout << _dbg_pre << "Stopping all receiving loops" << std::endl;

                _stop_receiving_ev.notify_all();
                co_await _node_receiver_counter.async_wait_zero();

                std::cout << _dbg_pre << "All receiving loops stopped" << std::endl;

                co_return true;

            }
        }
    }

    awaitable<void> _dispatch_input(Message::message_s* input)
    {
        //std::cout << "Popping node from stack" << std::endl;

        // pop one node from ready nodes stack
        auto node = co_await _nodes_stack.async_pop();

        co_spawn(co_await asio::this_coro::executor,
                 _send_input_to_node(node, input),
                 detached);
    }

    awaitable<void> _send_input_to_node(std::shared_ptr<Connection> node, Message::message_s* input)
    {
        //std::cout << "Sending input to node" << std::endl;

        // send input to node
        auto ec = co_await node->send_message(input);

        if (ec)
        {
            std::cerr << _dbg_pre << "Error while sending input to node, " << ec.what() << std::endl;
            co_await _dispatch_input(input);
            co_return;
        }

        input->destroy();

        //std::cout << "Pushing node back to stack" << std::endl;

        // push back node to ready stack
        _nodes_stack.push(node);
    }

    awaitable<void> _receiver_loop(std::shared_ptr<Connection> node)
    {
        // loop
        for (;;)
        {
            auto ret_var = co_await (node->receive_message() || _stop_receiving_ev.async_wait());

            // stop receiving event notified
            if (ret_var.index())
            {
                std::cerr << _dbg_pre << "| Receiver loop on " << node->get_id().to_string() << " | ";
                std::cerr << "Canceled" << std::endl;
                break;
            }

            auto [ec, message] = std::get<std::tuple<boost::system::error_code, Message::message_s*>>(ret_var);
            
            //auto [ec, message] = co_await node->receive_message();

            if (ec == asio::error::eof)
            {
                std::cerr << _dbg_pre << "| Receiver loop on " << node->get_id().to_string() << " | ";
                std::cerr << "Connection closed" << std::endl;
                break;
            }

            if (ec)
            {
                std::cerr << _dbg_pre << "| Receiver loop on " << node->get_id().to_string() << " | ";
                std::cerr << "Error while receiving data, " << ec.what() << std::endl;
                break;
            }

            if (!message->is_valid_data(_config->payload.as_config.output_size))
            {
                std::cerr << _dbg_pre << "| Receiver loop on " << node->get_id().to_string() << " | ";
                std::cerr << "Received invalid data size: " << message->get_data_size() << std::endl;
                message->destroy();
                break;
            }

            // send result to user
            ec = co_await _con.send_message(message);

            message->destroy();
            
            if (ec)
            {
                std::cerr << _dbg_pre << "| Receiver loop on " << node->get_id().to_string() << " | ";
                std::cerr << "Error while sending data, " << ec.what() << std::endl;
                break;
            }

            // decrement data counter
            _data_counter.decr();
        }
    }

private:
    std::string _dbg_pre;
    Connection _con;
    Message::message_s* _config;
    bool _config_response_sent;
    Counter _data_counter;
    Stack<std::shared_ptr<Connection>> _nodes_stack;
    Counter _node_receiver_counter;
    Event _stop_receiving_ev;
};
