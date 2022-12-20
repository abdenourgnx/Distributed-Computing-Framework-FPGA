#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "Server.hpp"

namespace asio = boost::asio;

using asio::co_spawn;
using asio::detached;

int main (int argc, char* argv[])
{
    asio::io_context ioc;

    Server srv(ioc, 8888, 9999);

    co_spawn(ioc,
             srv.run(),
             [](auto exp)
             {
                std::cout << "Server stopped." << std::endl;
             });

    ioc.run();
}