#pragma once

#include <iostream>
#include <vector>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "User_Queue.hpp"
#include "Connection.hpp"

namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;
using asio::co_spawn;
using asio::detached;
using asio::experimental::awaitable_operators::operator||;
using asio::ip::tcp;


class Server
{
public:

    Server(asio::io_context& ioc, unsigned short node_port, unsigned short user_port)
        : _node_acceptor(ioc, tcp::endpoint(tcp::v4(), node_port)),
          _user_acceptor(ioc, tcp::endpoint(tcp::v4(), user_port)),
          _node_port(node_port),
          _user_port(user_port)
    { }


    awaitable<void> run()
    {
        std::cout << "Starting server ..." << std::endl;
        co_await(_start_node_server() || _start_user_server());
    }


private:
    awaitable<void> _start_node_server()
    {
        std::cout << "Server | Node server started on " << _node_acceptor.local_endpoint() << std::endl;

        for (;;)
        {
            auto [ec, sock] = co_await _node_acceptor.async_accept(as_tuple(use_awaitable));
        
            if (ec)
            {
                std::cerr << "Server | Error while accepting node connection, " << ec.what() << std::endl;
                continue;
            }

            std::cout << "Server | New node connection from " << sock.remote_endpoint() << std::endl;

            // create new node and add it to user
            auto node = std::make_shared<Connection>(std::move(sock));

            _nodes.push_back(node);
        }
    }

    awaitable<void> _start_user_server()
    {
        std::cout << "Server | User server started on " << _user_acceptor.local_endpoint() << std::endl;

        for (;;)
        {
            auto [ec, sock] = co_await _user_acceptor.async_accept(as_tuple(use_awaitable));

            if (ec)
            {
                std::cerr << "Server | Error while accepting user connection, " << ec.what() << std::endl;
                continue;
            }

            std::cout << "Server | New user connection from " << sock.remote_endpoint() << std::endl;

            // create new user and start it
            auto user = User(std::move(sock));

            auto nodes_tmp = _nodes;

            _nodes.clear();

            co_await user.start(nodes_tmp);

            co_return;
        }
    }

private:
    tcp::acceptor _node_acceptor;
    tcp::acceptor _user_acceptor;

    unsigned short _node_port;
    unsigned short _user_port;

    std::vector<std::shared_ptr<Connection>> _nodes;
};