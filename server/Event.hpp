#pragma once

#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/deadline_timer.hpp>


namespace asio = boost::asio;

using asio::awaitable;
using asio::use_awaitable;
using asio::experimental::as_tuple;


class Event
{
public:
    Event(asio::io_context ioc) : _timer(ioc)
    {
        _timer.expires_at(boost::posix_time::pos_infin);
    }

    Event(asio::any_io_executor iox) : _timer(iox)
    {
        _timer.expires_at(boost::posix_time::pos_infin);
    }

    Event(Event&) = delete;
    Event(Event&&) = delete; 

public:
    awaitable<boost::system::error_code> async_wait()
    {
        auto [ec] = co_await _timer.async_wait(as_tuple(use_awaitable));

        if (ec == asio::error::operation_aborted)
            ec.clear();

        co_return ec;
    }

    void notify_one() { _timer.cancel_one(); }

    void notify_all() { _timer.cancel(); }

private:
    asio::deadline_timer _timer;
};