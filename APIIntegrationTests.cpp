#import <arpa/inet.h>
#import <assert.h>
#import <beast/core/streambuf.hpp>
#import <beast/http.hpp>
#import <boost/asio.hpp>
#import <boost/lexical_cast.hpp>
#import <boost/property_tree/json_parser.hpp>
#import <boost/property_tree/ptree.hpp>
#import <boost/variant.hpp>
#import <fstream>
#import <iostream>
#import <sstream>
#import <sys/socket.h>
#import <tuple>
#import <vector>

using namespace beast::http;
using namespace boost::asio;

namespace ssvim {
// Result is a variant where there can be a success or error case
template <typename Success, typename Error>
using Result = boost::variant<Success, Error>;

namespace ResultStatus {
const int Ok = 0;
const int Error = 1;

template <typename T, typename R> auto Get(R v) {
  return boost::get<T>(v);
}
} // namespace ResultStatus

// Example usage:
//    ssvim::Result<int, float> v;
//    v = 12; // v contains int
//    switch(v.which()) {
//        case ssvim::ResultStatus::Ok:
//            std::cout << "INT" << ssvim::ResultStatus::Get<int>(v);
//            break;
//        case ssvim::ResultStatus::Error:
//            std::cout << "FLOAT" << ssvim::ResultStatus::Get<float>(v);
//            break;
//    }

} // namespace ssvim

template <class String>
void err(beast::error_code const &ec, String const &what) {
  std::cerr << what << ": " << ec.message() << std::endl;
}

typedef enum TestErrorCode {
  TestErrorCodeUndefined,
} TestErrorCode;

static ssvim::Result<response<string_body>, TestErrorCode>
PostRequest(std::string port, std::string path, std::string body) {
  io_service ios;

  // Run tests on localhost
  auto host = "localhost";
  try {
    ip::tcp::resolver r(ios);
    auto it = r.resolve(ip::tcp::resolver::query{host, port});
    ip::tcp::socket sock(ios);
    connect(sock, it);
    auto ep = sock.remote_endpoint();
    request<string_body> req;
    req.method = "POST";
    req.url = path;
    req.body = body;
    req.version = 11;
    req.fields.insert("Host", host + std::string(":") +
                                  boost::lexical_cast<std::string>(ep.port()));
    req.fields.insert("User-Agent", "ssvim-integration_tests/http");
    req.fields.insert("Content-Type", "application/json");
    prepare(req);
    write(sock, req);
    response<string_body> res;
    streambuf sb;
    beast::http::read(sock, sb, res);
    return res;
  } catch (beast::system_error const &ec) {
    std::cerr << host << ": " << ec.what();
  } catch (...) {
    std::cerr << host << ": unknown exception" << std::endl;
  }
  return TestErrorCodeUndefined;
}

std::string ReadFile(const std::string &fileName) {
  std::ifstream ifs(fileName.c_str(),
                    std::ios::in | std::ios::binary | std::ios::ate);

  std::ifstream::pos_type fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> bytes(fileSize);
  ifs.read(&bytes[0], fileSize);
  return std::string(&bytes[0], fileSize);
}

std::string MakeCompletionPostBody(int line, int column, std::string fileName,
                                   std::string contents,
                                   std::vector<std::string> flags) {
  using boost::property_tree::ptree;
  ptree out;
  out.put("line", line);
  out.put("column", column);
  out.put("file_name", fileName);
  out.put("contents", contents);
  boost::property_tree::ptree flagsOut;
  for (auto &f : flags)
    flagsOut.put("", f);
  out.add_child("flags", flagsOut);
  std::ostringstream oss;
  boost::property_tree::write_json(oss, out);
  return oss.str();
}

std::string GetExamplesDir() {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    return std::string(std::string(cwd) + "/Examples/");
  }
  return "";
}

#pragma mark - IntegrationTestSuite

class IntegrationTestSuite {
  std::string _boundPort;

public:
  IntegrationTestSuite(std::string boundPort) : _boundPort(boundPort) {
  }

  void testSuccessfulCompletion() {
    auto exampleDir = GetExamplesDir();
    auto exampleName = exampleDir + std::string("some_swift.swift");
    auto example = ReadFile(exampleName);
    std::vector<std::string> flags;
    flags.push_back("-sdk");
    flags.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/"
                    "MacOSX.platform/Developer/SDKs/MacOSX.sdk");
    flags.push_back("-target");
    flags.push_back("x86_64-apple-macosx10.12");

    using namespace ssvim::ResultStatus;
    auto body = MakeCompletionPostBody(19, 15, exampleName, example, flags);
    auto responseValue = PostRequest(_boundPort, "/completions", body);
    auto res = Get<response<string_body>>(responseValue);
    assert(res.body.length() > 0);
    assert(res.status == 200);
  }

  void testStatus() {
    using namespace ssvim::ResultStatus;
    auto responseValue = PostRequest(_boundPort, "/status", "");
    auto res = Get<response<string_body>>(responseValue);
    assert(res.status == 200);
  }

  void testRunningAfterGarbageJSON() {
    // Send a request, and then check if its still up
    PostRequest(_boundPort, "/completions", "");
  }
};

std::tuple<std::string, int> testBind() {
  // Create socket
  struct sockaddr_in server;
  int socket_desc;
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  assert(socket_desc != -1 && "Could not create socket");
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(0);

  // Bind
  bind(socket_desc, (struct sockaddr *)&server, sizeof(server));

  int sock = 0;
  socklen_t len = sizeof(struct sockaddr);
  struct sockaddr_in addr;
  getsockname(sock, (struct sockaddr *)&addr, &len);
  auto ip = inet_ntoa(addr.sin_addr);
  auto port = ntohs(addr.sin_port);
  fprintf(stderr, "listening on %s:%d\n", ip, port);
  return std::tuple<std::string, int>(ip, (int)port);
}

int main(int, char const *[]) {
  auto exampleDir = GetExamplesDir();
  std::cout << "Running SSVIM integration tests with examples \n " << exampleDir
            << std::endl;

  // Here we start up the server on a port that we bind
  // This emulates a user having an instance running for an editing
  // session.
  auto bootInfo = testBind();
  auto boundPort = std::to_string(std::get<int>(bootInfo));
  auto startCmd = std::string("`./build/http_server");
  startCmd += " --port ";
  startCmd += boundPort;
  startCmd += " >/dev/null`&";

  // Startup the service
  int started = system(startCmd.c_str());
  assert(started == 0 && "Failed to start");
  sleep(1);

  // IntegrationTests Begin
  // NOTE: There should be no expected order to these test invocations, the
  // fact that these are sequential is an implementation detail of the current
  // runner
  IntegrationTestSuite suite(boundPort);

  std::cout << "testStatus" << std::endl;
  suite.testStatus();

  std::cout << "testSuccessfulCompletion" << std::endl;
  suite.testSuccessfulCompletion();

  // TODO:
  // std::cout << "testRunningAfterGarbageJSON" << std::endl;
  // testRunningAfterGarbageJSON();

  // IntegrationTests End

  using namespace ssvim::ResultStatus;
  auto responseValue = PostRequest(boundPort, "/shutdown", "");
  auto res = Get<response<string_body>>(responseValue);
  assert(res.status == 200 && "Failed to shutdown");
  return 0;
}
