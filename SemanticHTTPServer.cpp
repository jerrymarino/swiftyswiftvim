/**
 * Beast HTTP server example with long running operations and lib dispatch.
 *
 * Note: Assume that we are calling dispatch_main() after running the server
 */

#include "SemanticHTTPServer.hpp"
#include "file_body.hpp"

#include "SwiftCompleter.hpp"

#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/placeholders.hpp>
#include <beast/core/streambuf.hpp>
#include <beast/http.hpp>

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <dispatch/dispatch.h>

#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

typedef enum LogLevel { none, debug } LogLevel;

struct service_context {
  std::string secret;
  LogLevel logging;
};

static auto HeaderValueContentTypeJSON = "application/json";
static auto HeaderKeyContentType = "Content-Type";
static auto HeaderKeyServer = "Server";
static auto HeaderValueServer = "SSVIM";

using namespace beast::http;

namespace ssvim {
namespace http {

using socket_type = boost::asio::ip::tcp::socket;

using req_type = request<string_body>;
using resp_type = response<file_body>;

/**
 * Session is an instance of an HTTP Session.
 *
 * The server will allocate a new instance for each accepted
 * request.
 */
class Session;

using EndpointFn = std::function<void(std::shared_ptr<Session>)>;

class EndpointImpl : public std::enable_shared_from_this<EndpointImpl> {
public:
  EndpointImpl(EndpointFn start) : _start(start) {}

  void handleRequest(std::shared_ptr<Session> session) {
    std::cout << "___HANDLE_REQUEST";
    std::cout.flush();
    // Assume we have a dispatch main queue running.
    //
    // Run the endpoint on the background thread
    auto kickoff = _start;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
                   ^{
                     std::cout << "___START_BACKGROUND";
                     std::cout.flush();
                     // TODO: Exception safety
                     // Assume we have a dispatch main queue running.
                     kickoff(session);
                   });
  }

private:
  EndpointFn _start;
};

using namespace ssvim;

EndpointImpl makeSlowTestEndpoint();
EndpointImpl makeStatusEndpoint();
EndpointImpl makeShutdownEndpoint();
EndpointImpl makeCompletionsEndpoint();
EndpointImpl makeDiagnosticsEndpoint();

response<string_body> notFoundResponse(req_type request);
response<string_body> errorResponse(req_type request, std::string message);

class Session : public std::enable_shared_from_this<Session> {
  streambuf _streambuf;
  socket_type _socket;
  service_context _context;
  boost::asio::io_service::strand _strand;
  req_type _request;
  std::map<std::string, EndpointImpl> _endpoints;
  EndpointImpl *_endpoint;

public:
  Session(Session &&) = default;
  Session(Session const &) = default;
  Session &operator=(Session &&) = delete;
  Session &operator=(Session const &) = delete;

  Session(socket_type &&sock, service_context ctx)
      : _socket(std::move(sock)), _context(ctx),
        _strand(_socket.get_io_service()) {
    _endpoint = NULL;
    // TODO: Come up with a better way
    if (ctx.logging == debug) {
      std::cout << "Secret: ";
      std::cout << _context.secret;
      std::cout << "\n";
    }

    // Setup Endpoints.
    // TODO: Perhaps this can be done statically
    _endpoints = std::map<std::string, EndpointImpl>();
    auto insert_endpoint = [&](std::string named, EndpointImpl impl) {
      _endpoints.insert(std::pair<std::string, EndpointImpl>(named, impl));
    };

    insert_endpoint("/status", makeStatusEndpoint());
    insert_endpoint("/shutdown", makeShutdownEndpoint());
    insert_endpoint("/completions", makeCompletionsEndpoint());
    insert_endpoint("/diagnostics", makeDiagnosticsEndpoint());
    insert_endpoint("/slow_test", makeSlowTestEndpoint());
  }

  void start() { doRead(); }

  std::shared_ptr<Session> detach() { return shared_from_this(); }

  void doRead() {
    async_read(_socket, _streambuf, _request,
               _strand.wrap(std::bind(&Session::onRead, shared_from_this(),
                                      asio::placeholders::error)));
  }

  void onRead(error_code const &ec) {
    std::cout << "__ONREAD";
    if (ec)
      return fail(ec, "read");
    auto path = _request.url;

    // Typical flow of handling a response
    // - Detach and retain - necessary to keep this alive.
    // - Quickly return to prevent from blocking acceptor loop.
    // - Perform a long running task.
    // - Schedule write for the response body
    auto detachedSession = detach();

    if (_context.logging == debug) {
      std::cout << "__WILL_READ: " << path << "\n";
      std::cout.flush();
    }

    auto endpointImpl = _endpoints.find(std::string(path));
    if (endpointImpl != _endpoints.end()) {
      std::cout << "__GOTEP:";
      std::cout.flush();
      _endpoint = &endpointImpl->second;
      _endpoint->handleRequest(detachedSession);
      return;
    }

    // Schedule not found response
    detachedSession->write(notFoundResponse(_request));
  }

  void onWrite(error_code ec) {
    if (ec)
      fail(ec, "write");
    doRead();
  }

#pragma mark - State

  req_type request() { return _request; }

#pragma mark - Writing messages

  // Schedule a write
  void write(response<string_body> res) {
    async_write(_socket, std::move(res),
                std::bind(&Session::onWrite, shared_from_this(),
                          asio::placeholders::error));
  }

  void fail(error_code ec, std::string what) {
    auto message = what + " and: " + ec.message();
    std::cout << message;
  }

  // Schedule an error message
  void error(std::string message) {
    auto res = errorResponse(_request, message);
    async_write(_socket, std::move(res),
                std::bind(&Session::onWrite, shared_from_this(),
                          asio::placeholders::error));
  }
};

#pragma mark - Server

void SemanticHTTPServer::onAccept(error_code ec) {
  if (!_acceptor.is_open())
    return;
  if (ec)
    return fail(ec, "accept");
  socket_type sock(std::move(_socket));
  _acceptor.async_accept(_socket, std::bind(&SemanticHTTPServer::onAccept, this,
                                            asio::placeholders::error));

  // Start a new Session.
  service_context ctx;
  ctx.secret = "Some Secret";
  ctx.logging = debug;
  auto session = std::make_shared<Session>(std::move(sock), ctx);
  session->start();
}

#pragma mark - Endpoint impl

EndpointImpl makeStatusEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    response<string_body> res;
    res.status = 200;
    res.version = session->request().version;
    res.fields.insert(HeaderKeyServer, HeaderValueServer);
    res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
    prepare(res);
    session->write(res);
  });
}

EndpointImpl makeShutdownEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    std::cout << "Recieved Shutdown Request";
    response<string_body> res;
    res.status = 200;
    res.version = session->request().version;
    res.fields.insert(HeaderKeyServer, HeaderValueServer);
    res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
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

ptree readJSONPostBody(std::string body) {
  ptree pt;
  std::istringstream is(body);
  read_json(is, pt);
  return pt;
}

template <typename T>
const std::vector<T> as_vector(ptree const &pt, ptree::key_type const &key) {
  std::vector<T> r;
  for (auto &item : pt.get_child(key))
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
EndpointImpl makeCompletionsEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    // Parse in data
    auto bodyString = session->request().body;
    std::cout << bodyString;
    auto bodyJSON = readJSONPostBody(bodyString);

    auto fileName = bodyJSON.get<std::string>("file_name");
    auto column = bodyJSON.get<int>("column");
    auto line = bodyJSON.get<int>("line");
    auto contents = bodyJSON.get<std::string>("contents");
    auto flags = as_vector<std::string>(bodyJSON, "flags");
    std::cout << "file_name:" << fileName;
    std::cout << "column:" << column;
    std::cout << "line:" << line;
    for (auto &f : flags) {
      std::cout << "flags:" << f;
    }

    using namespace ssvim;
    SwiftCompleter completer;

    auto files = std::vector<UnsavedFile>();
    auto unsaved = UnsavedFile();
    unsaved.contents = contents;
    unsaved.fileName = fileName;
    files.push_back(unsaved);

    std::cout << "__SEND_REQ";
    std::cout.flush();
    auto candidates = completer.CandidatesForLocationInFile(
        fileName, line, column, files, flags);

    std::cout << "__GOT_CANDIDATES";
    std::cout << candidates;
    std::cout.flush();
    // Build out response
    response<string_body> res;
    res.status = 200;
    res.version = session->request().version;
    res.fields.insert(HeaderKeyServer, HeaderValueServer);
    res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
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
// @param file_name: the name of the users file
EndpointImpl makeDiagnosticsEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    // Parse in data
    auto bodyString = session->request().body;
    std::cout << bodyString;
    auto bodyJSON = readJSONPostBody(bodyString);

    auto fileName = bodyJSON.get<std::string>("file_name");
    auto contents = bodyJSON.get<std::string>("contents");
    auto flags = as_vector<std::string>(bodyJSON, "flags");
    std::cout << "file_name:" << fileName;
    for (auto &f : flags) {
      std::cout << "flags:" << f;
    }

    using namespace ssvim;
    SwiftCompleter completer;

    auto files = std::vector<UnsavedFile>();
    auto unsaved = UnsavedFile();
    unsaved.contents = contents;
    unsaved.fileName = fileName;
    files.push_back(unsaved);

    std::cout << "__SEND_REQ";
    std::cout.flush();
    auto diagnostics = completer.DiagnosticsForFile(fileName, files, flags);

    std::cout << "__GOT_DIAGNOSTICS";
    std::cout << diagnostics;
    std::cout.flush();
    // Build out response
    response<string_body> res;
    res.status = 200;
    res.version = session->request().version;
    res.fields.insert(HeaderKeyServer, HeaderValueServer);
    res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
    res.body = diagnostics;
    prepare(res);
    session->write(res);
  });
}

EndpointImpl makeSlowTestEndpoint() {
  return EndpointImpl([](std::shared_ptr<Session> session) {
    // Wait for 10 seconds to write hello world.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     std::cout << "Enter main: ";
                     std::cout << session->request().url;
                     std::cout.flush();

                     response<string_body> res;
                     res.status = 200;
                     res.version = session->request().version;
                     res.fields.insert(HeaderKeyServer, HeaderValueServer);
                     res.fields.insert(HeaderKeyContentType,
                                       HeaderValueContentTypeJSON);
                     res.body = "Hello World";
                     prepare(res);
                     session->write(res);
                   });
  });
}

response<string_body> errorResponse(req_type request, std::string message) {
  response<string_body> res;
  res.status = 500;
  res.reason = "Internal Error";
  res.version = request.version;
  res.fields.insert(HeaderKeyServer, HeaderValueServer);
  res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
  res.body = std::string{"An internal error occurred"} + message;
  prepare(res);
  return res;
}

response<string_body> notFoundResponse(req_type request) {
  response<string_body> res;
  res.status = 404;
  res.reason = "Not Found";
  res.version = request.version;
  res.fields.insert(HeaderKeyServer, HeaderValueServer);
  res.fields.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
  res.body = "Endpoint: '" + request.url + "' not found";
  prepare(res);
  return res;
}

} // namespace http
} // namespace ssvim
