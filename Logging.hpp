#include <iostream>

namespace ssvim {

typedef enum LogLevel {
  LogLevelError = 1,
  LogLevelInfo = 2,
  LogLevelExtreme = 3
} LogLevel;

class Logger : public std::enable_shared_from_this<Logger> {
  typedef enum LogOptions {
    // TODO: turn this down in production
    LogOptionsNone = 0 << 0,
    LogOptionsError = LogLevelError << 0,
    LogOptionsInfo = LogOptionsError | LogLevelInfo << 0,
    LogOptionsExtreme = LogOptionsInfo | LogLevelExtreme << 0
  } LogOptions;

  static auto optionsWithLevel(LogLevel level) {
    switch (level) {
    case LogLevelError:
      return LogOptionsError;
    default:
      return LogOptionsInfo;
    }
  }

  LogOptions _setLevel;
  std::string _channel;

public:
  Logger(LogLevel level, std::string channel = "SS")
      : _setLevel(optionsWithLevel(level)), _channel("__" + channel) {
  }

  template <class... Args> Logger log(LogLevel level, Args const &... args) {
    if ((_setLevel & level) != 0) {
      return logArgs(level, args...);
    }
    return *this;
  }

  // By default this logs debugging messages
  template <class Args> Logger &operator<<(Args args) {
    log(LogLevelInfo, args);
    return *this;
  }

private:
  Logger &logArgs(LogLevel level) {
    return *this;
  }

  template <class Arg, class... Args>
  Logger &logArgs(LogLevel level, Arg const &arg, Args const &... args) {
    if (level == LogLevelError) {
      std::cerr << _channel << arg << std::endl;
    } else {
      std::cout << _channel << arg << std::endl;
    }
    logArgs(level, args...);
    return *this;
  }
};
} // namespace ssvim
