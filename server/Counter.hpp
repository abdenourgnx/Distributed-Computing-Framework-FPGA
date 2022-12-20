#pragma once

#include <iostream>
#include <stack>

#include <boost/asio/io_context.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/deadline_timer.hpp>


namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;


class Counter
{
public:
    Counter(asio::io_context ioc, std::string prefix =""): _timer(ioc), _prefix(prefix)
    {
        _timer.expires_at(boost::posix_time::pos_infin);
    }

    Counter(asio::any_io_executor iox, std::string prefix =""): _timer(iox), _prefix(prefix)
    {
        _timer.expires_at(boost::posix_time::pos_infin);
    }

    ~Counter() { }

    void incr()
    {
        this->_val++;
        _timer.cancel();
        if (_prefix != "")
            std::cout << _prefix << "Incremented" << std::endl;
    }

    void decr()
    {
        if (_prefix != "")
            std::cout << _prefix << "Decremented" << std::endl;
        
        if (this->_val == 0)
            return;

        this->_val--;
        _timer.cancel();
    }

    uint get_value()
    {
        return this->_val;
    }

    awaitable<boost::system::error_code> async_wait_zero()
    {
        for (;;)
        {
            if (this->_val == 0)
                co_return boost::system::error_code(0, boost::system::system_category());

            auto [ec] = co_await _timer.async_wait(as_tuple(use_awaitable));
    
            if (ec == asio::error::operation_aborted)
                continue;

            if (ec)
                co_return ec;
        }
    }

private:
    std::string _prefix;
    uint _val;
    asio::deadline_timer _timer;
};