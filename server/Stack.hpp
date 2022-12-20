#pragma once

#include <iostream>
#include <stack>

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


template <typename T>
class Stack
{
public:
    Stack(asio::io_context ioc): _timer_pop(ioc)
    {
        _timer_pop.expires_at(boost::posix_time::pos_infin);
    }

    Stack(asio::any_io_executor iox): _timer_pop(iox)
    {
        _timer_pop.expires_at(boost::posix_time::pos_infin);
    }

    ~Stack() { }

    void push(T item)
    {
        _stack.push(item);
        _timer_pop.cancel_one();
    }

    T pop()
    {
        T item = _stack.top();
        _stack.pop();
        return item;
    }

    bool empty() { return _stack.empty(); }

    awaitable<T> async_pop()
    {
        if (!this->empty())
            co_return this->pop();
    
        co_await _timer_pop.async_wait(as_tuple(use_awaitable));
    
        co_return this->pop();
    }

private:
    std::stack<T> _stack;
    asio::deadline_timer _timer_pop;
};