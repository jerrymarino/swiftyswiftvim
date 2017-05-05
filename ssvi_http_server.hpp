/**
 * Beast HTTP server example with long running operations and lib dispatch.
 *
 * Note: Assume that we are calling dispatch_main() after running the server
 */

#include "file_body.hpp"
#include "mime_type.hpp"

#include <beast/http.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/placeholders.hpp>
#include <beast/core/streambuf.hpp>
#include <boost/asio.hpp>
#include <cstddef>
#include <cstdio>
#include <dispatch/dispatch.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <sstream>
#include <map>

namespace beast {
namespace http {

/**
 * SSVI HTTP Server is a HTTP front end for Swift Semantic
 * tasks.
 */
class ssvi_http_server
{
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

    std::mutex _shared_mutex;
    bool _logging_enabled = true;
    boost::asio::io_service _io_service;
    boost::asio::ip::tcp::acceptor _acceptor;
    socket_type _socket;
    std::string _root_path;
    std::vector<std::thread> _thread;

public:
    ssvi_http_server(endpoint_type const& ep,
            std::size_t threads, std::string const& root)
        : _acceptor(_io_service)
        , _socket(_io_service)
        , _root_path(root)
    {
        _acceptor.open(ep.protocol());
        _acceptor.bind(ep);
        _acceptor.listen(
            boost::asio::socket_base::max_connections);
        _acceptor.async_accept(_socket,
            std::bind(&ssvi_http_server::on_accept, this,
                beast::asio::placeholders::error));
        _thread.reserve(threads);
        for(std::size_t i = 0; i < threads; ++i)
            _thread.emplace_back(
                [&] { _io_service.run(); });
    }

    ~ssvi_http_server()
    {
        error_code ec;
        _io_service.dispatch(
            [&]{ _acceptor.close(ec); });
        for(auto& t : _thread)
            t.join();
    }

    template<class... Args>
    void
    log(Args const&... args)
    {
        if(_logging_enabled)
        {
            std::lock_guard<std::mutex> lock(_shared_mutex);
            log_args(args...);
        }
    }

private:
    template<class Stream, class Handler,
        bool isRequest, class Body, class Fields>
    class write_op
    {
        struct data
        {
            bool cont;
            Stream& s;
            message<isRequest, Body, Fields> m;

            data(Handler& handler, Stream& s_,
                    message<isRequest, Body, Fields>&& _shared_mutex)
                : cont(beast_asio_helpers::
                    is_continuation(handler))
                , s(s_)
                , m(std::move(_shared_mutex))
            {
            }
        };

        handler_ptr<data, Handler> d_;

    public:
        write_op(write_op&&) = default;
        write_op(write_op const&) = default;

        template<class DeducedHandler, class... Args>
        write_op(DeducedHandler&& h, Stream& s, Args&&... args)
            : d_(std::forward<DeducedHandler>(h),
                s, std::forward<Args>(args)...)
        {
            (*this)(error_code{}, false);
        }

        void
        operator()(error_code ec, bool again = true)
        {
            auto& d = *d_;
            d.cont = d.cont || again;
            if(! again)
            {
                beast::http::async_write(d.s, d.m, std::move(*this));
                return;
            }
            d_.invoke(ec);
        }

        friend
        void* asio_handler_allocate(
            std::size_t size, write_op* op)
        {
            return beast_asio_helpers::
                allocate(size, op->d_.handler());
        }

        friend
        void asio_handler_deallocate(
            void* p, std::size_t size, write_op* op)
        {
            return beast_asio_helpers::
                deallocate(p, size, op->d_.handler());
        }

        friend
        bool asio_handler_is_continuation(write_op* op)
        {
            return op->d_->cont;
        }

        template<class Function>
        friend
        void asio_handler_invoke(Function&& f, write_op* op)
        {
            return beast_asio_helpers::
                invoke(f, op->d_.handler());
        }
    };

    template<class Stream,
        bool isRequest, class Body, class Fields,
            class DeducedHandler>
    static
    void
    async_write(Stream& stream, message<
        isRequest, Body, Fields>&& msg,
            DeducedHandler&& handler)
    {
        write_op<Stream, typename std::decay<DeducedHandler>::type,
            isRequest, Body, Fields>{std::forward<DeducedHandler>(
                handler), stream, std::move(msg)};
    }

    void log_args()
    {
    }

    template<class Arg, class... Args>
    void
    log_args(Arg const& arg, Args const&... args)
    {
        std::cerr << arg;
        log_args(args...);
    }

    void
    fail(error_code ec, std::string what)
    {
        log(what, ": ", ec.message(), "\n");
    }

    void
    on_accept(error_code ec);
};
    
} // http
} // beast

