/**
 * Beast HTTP server example with long running operations and lib dispatch.
 *
 * Note: Assume that we are calling dispatch_main() after running the server
 */

#include "file_body.hpp"
#include "ssvim_http_server.hpp"

#include "SwiftCompleter.h"

#include <beast/http.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/placeholders.hpp>
#include <beast/core/streambuf.hpp>

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <dispatch/dispatch.h>

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <sstream>
#include <map>
#include <functional>
#include <string>

typedef enum log_level {
    none,
    debug
} log_level;

struct service_context {
    std::string secret;
    log_level logging;
};

using namespace beast::http;

namespace ssvim {
namespace http {

using socket_type = boost::asio::ip::tcp::socket;

using req_type = request<string_body>;
using resp_type = response<file_body>;

/**
 * Session is an instance of an HTTP session.
 *
 * The server will allocate a new instance for each accepted
 * request.
 */
class session;

using endpoint_fn = std::function<void(std::shared_ptr<session>)>;

class endpoint_impl : public std::enable_shared_from_this<endpoint_impl>{
    public: endpoint_impl(
        endpoint_fn start
    ) : 
        _start(start) {}

    void handle_request(std::shared_ptr<session> session) {
        std::cout << "___HANDLE_REQUEST"; 
        std::cout.flush();
        // Assume we have a dispatch main queue running.
        //
        // Run the endpoint on the background thread
        auto kickoff = _start;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            std::cout << "___START_BACKGROUND"; 
            std::cout.flush();
            //TODO: Exception safety
            // Assume we have a dispatch main queue running.
            kickoff(session);
        });
    }

    private:
    endpoint_fn _start;
};

using namespace ssvim;

endpoint_impl make_slow_test_endpoint();
endpoint_impl make_status_endpoint();
endpoint_impl make_shutdown_endpoint();
endpoint_impl make_completions_endpoint();
endpoint_impl make_diagnostics_endpoint();

response<string_body> not_found_response(req_type request);
response<string_body> error_response(req_type request, std::string message);

class session : public std::enable_shared_from_this<session>
{
    int _instance_id;
    streambuf _streambuf;
    socket_type _socket;
    service_context _context;
    boost::asio::io_service::strand _strand;
    req_type _request;
    std::map<std::string, endpoint_impl> _endpoints;
    endpoint_impl *_endpoint;
public:
    session(session&&) = default;
    session(session const&) = default;
    session& operator=(session&&) = delete;
    session& operator=(session const&) = delete;
    
    session(socket_type&& sock, service_context ctx)
    : _socket(std::move(sock))
    , _context(ctx)
    , _strand(_socket.get_io_service()
            )
    {
        _endpoint = NULL;
        static int n = 0;
        _instance_id = ++n;
        // TODO: Come up with a better way
        if (ctx.logging == debug) {
            std::cout << "Secret: ";
            std::cout << _context.secret;
            std::cout << " Id: ";
            std::cout << _instance_id;
            std::cout << "\n";
        }

        // Setup Endpoints.
        // TODO: Perhaps this can be done statically
        _endpoints = std::map<std::string, endpoint_impl>();
        auto insert_endpoint = [&](std::string named, endpoint_impl impl){
            _endpoints.insert(std::pair<std::string, endpoint_impl>(named, impl));
        };

        insert_endpoint("/status", make_status_endpoint());
        insert_endpoint("/shutdown", make_shutdown_endpoint());
        insert_endpoint("/completions", make_completions_endpoint());
        insert_endpoint("/diagnostics", make_diagnostics_endpoint());
        insert_endpoint("/slow_test", make_slow_test_endpoint());
    }
    
    void start()
    {
        do_read();
    }
    
    std::shared_ptr<session>
    detach()
    {
        return shared_from_this();
    }
    
    void do_read()
    {
        async_read(_socket, _streambuf, _request, _strand.wrap(
                                                  std::bind(&session::on_read, shared_from_this(),
                                                            asio::placeholders::error)));
    }
   
    void on_read(error_code const& ec)
    {
        std::cout << "__ONREAD";
        if(ec)
            return fail(ec, "read");
        auto path = _request.url;

        // Typical flow of handling a response
        // - Detach and retain - necessary to keep this alive.
        // - Quickly return to prevent from blocking acceptor loop.
        // - Perform a long running task.
        // - Schedule write for the response body
        auto detached_session = detach();

        if (_context.logging == debug){
            std::cout << "__WILL_READ: " << _instance_id << path << "\n";
            std::cout.flush();
        }

        auto endpoint_entry = _endpoints.find(std::string(path)); 
        if (endpoint_entry != _endpoints.end()) {
            std::cout << "__GOTEP:"; 
            std::cout.flush();
            _endpoint = &endpoint_entry->second;; 
            _endpoint->handle_request(detached_session);
            return;
        }

        // Schedule not found response
        detached_session->write(not_found_response(_request));
    }
    
    void on_write(error_code ec)
    {
        if(ec)
            fail(ec, "write");
        do_read();
    }

    #pragma mark - State
    
    int instance_id() {
        return _instance_id;
    }

    req_type request() {
        return _request;
    }

    #pragma mark - Writing messages

    // Schedule a write 
    void write(response<string_body> res)
    {
        async_write(_socket, std::move(res),
                    std::bind(&session::on_write, shared_from_this(),
                              asio::placeholders::error));
    }

    void
    fail(error_code ec, std::string what)
    {
        auto message = what + " and: " + ec.message();
        std::cout << message;
    }

    // Schedule an error message
    void error(std::string message)
    {
        auto res = error_response(_request, message);
        async_write(_socket, std::move(res),
                    std::bind(&session::on_write, shared_from_this(),
                              asio::placeholders::error));
    }
};

#pragma mark - Server

void ssvi_http_server::
on_accept(error_code ec)
{
    if(!_acceptor.is_open())
        return;
    if(ec)
        return fail(ec, "accept");
    socket_type sock(std::move(_socket));
    _acceptor.async_accept(_socket,
        std::bind(&ssvi_http_server::on_accept, this,
            asio::placeholders::error));

    // Start a new session.
    service_context ctx;
    ctx.secret = "Some Secret";
    ctx.logging = debug;
    auto new_session = std::make_shared<session>(std::move(sock), ctx);
    new_session->start();
}

#pragma mark - Endpoint impl

endpoint_impl make_status_endpoint() {
    return endpoint_impl([&](std::shared_ptr<session> session){
        response<string_body> res;
        res.status = 200;
        res.version = session->request().version;
        res.fields.insert("Server", "ssvi_http_server");
        res.fields.insert("Content-Type", "application/json");
        prepare(res);
        session->write(res);
    });
}

endpoint_impl make_shutdown_endpoint() {
    return endpoint_impl([&](std::shared_ptr<session> session){
        std::cout << "Recieved Shutdown Request";
        response<string_body> res;
        res.status = 200;
        res.version = session->request().version;
        res.fields.insert("Server", "ssvi_http_server");
        res.fields.insert("Content-Type", "application/json");
        prepare(res);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC),
                       dispatch_get_main_queue(), ^{
                       std::cout << "Shutting down...";
                       exit(0);
        });
        session->write(res);
    });
}

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

ptree parse_json_post_body(std::string body) {
    ptree pt;
    std::istringstream is (body);
    read_json(is, pt);
    return pt;
}

template <typename T>
const std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key)
{
    std::vector<T> r;
    for (auto& item : pt.get_child(key))
        r.push_back(item.second.get_value<T>());
    return r;
}

// Make completions endpoint returns an endpoint that
// handles basic completion requests
//
// @param flags: an array of string flags
// @param contents: the current files
// @param line: the users line
// @param column: the users column
// @param file_name: the name of the users file
endpoint_impl make_completions_endpoint() {
    return endpoint_impl([&](std::shared_ptr<session> session){
        // Parse in data
        auto body_string = session->request().body;
        std::cout << body_string; 
        auto body_json = parse_json_post_body(body_string);

        auto file_name = body_json.get<std::string>("file_name");
        auto column = body_json.get<int>("column");
        auto line = body_json.get<int>("line");
        auto contents = body_json.get<std::string>("contents");
        auto flags = as_vector<std::string>(body_json, "flags");
        std::cout << "file_name:" << file_name; 
        std::cout << "column:" << column; 
        std::cout << "line:" << line; 
        for (auto &f : flags){
            std::cout << "flags:" << f; 
        }

        using namespace ssvim;
        SwiftCompleter completer;

        auto files = std::vector<UnsavedFile>();
        auto unsaved = UnsavedFile();
        unsaved.contents = contents;
        unsaved.fileName = file_name;
        files.push_back(unsaved);

        std::cout << "__SEND_REQ"; 
        std::cout.flush();
        auto candidates = completer.CandidatesForLocationInFile(
                file_name,
                line,
                column,
                files,
                flags);

        std::cout << "__GOT_CANDIDATES"; 
        std::cout << candidates; 
        std::cout.flush();
        // Build out response
        response<string_body> res;
        res.status = 200;
        res.version = session->request().version;
        res.fields.insert("Server", "ssvi_http_server");
        res.fields.insert("Content-Type", "application/json");
        res.body = candidates;
        prepare(res);
        session->write(res);
    });
}

// Make completions endpoint returns an endpoint that
// handles basic completion requests
//
// @param flags: an array of string flags
// @param contents: the current files
// @param line: the users line
// @param column: the users column
// @param file_name: the name of the users file
endpoint_impl make_diagnostics_endpoint() {
    return endpoint_impl([&](std::shared_ptr<session> session){
        // Parse in data
        auto body_string = session->request().body;
        std::cout << body_string; 
        auto body_json = parse_json_post_body(body_string);

        auto file_name = body_json.get<std::string>("file_name");
        auto contents = body_json.get<std::string>("contents");
        auto flags = as_vector<std::string>(body_json, "flags");
        std::cout << "file_name:" << file_name; 
        for (auto &f : flags){
            std::cout << "flags:" << f; 
        }

        using namespace ssvim;
        SwiftCompleter completer;

        auto files = std::vector<UnsavedFile>();
        auto unsaved = UnsavedFile();
        unsaved.contents = contents;
        unsaved.fileName = file_name;
        files.push_back(unsaved);

        std::cout << "__SEND_REQ"; 
        std::cout.flush();
        auto diagnostics = completer.DiagnosticsForFile(
                file_name,
                files,
                flags);

        std::cout << "__GOT_DIAGNOSTICS"; 
        std::cout << diagnostics; 
        std::cout.flush();
        // Build out response
        response<string_body> res;
        res.status = 200;
        res.version = session->request().version;
        res.fields.insert("Server", "ssvi_http_server");
        res.fields.insert("Content-Type", "application/json");
        res.body = diagnostics;
        prepare(res);
        session->write(res);
    });
}

endpoint_impl make_slow_test_endpoint()
{
    return endpoint_impl([](std::shared_ptr<session> session){
        // Wait for 10 seconds to write hello world.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC),
                       dispatch_get_main_queue(), ^{
            std::cout << "Enter main: " << session->instance_id() << "\n";
            std::cout << session->request().url;
            std::cout.flush();
            
            response<string_body> res;
            res.status = 200;
            res.version = session->request().version;
            res.fields.insert("Server", "ssvi_http_server");
            res.fields.insert("Content-Type", "application/json");
            res.body = "Hello World";
            prepare(res);
            session->write(res);
        });
    });
}

response<string_body>
error_response(req_type request, std::string message){
    response<string_body> res;
    res.status = 500;
    res.reason = "Internal Error";
    res.version = request.version;
    res.fields.insert("Server", "http_async_server");
    res.fields.insert("Content-Type", "application/json");
    res.body =
    std::string{"An internal error occurred"} + message;
    prepare(res);
    return res;
}

response<string_body>
not_found_response(req_type request){
    response<string_body> res;
    res.status = 404;
    res.reason = "Not Found";
    res.version = request.version;
    res.fields.insert("Server", "http_async_server");
    res.fields.insert("Content-Type", "application/json");
    res.body = "Endpoint: '" + request.url + "' not found";
    prepare(res);
    return res;
}

} // http
} // ssvim

