#import <iostream>

namespace ssvim {

typedef enum LogLevel {
  LogLevelError = 1,
  LogLevelInfo,
  LogLevelExtreme
} LogLevel;

class Logger : public std::enable_shared_from_this<Logger> {
  LogLevel const _level;
  std::string _messagePrefix;

public:
  Logger(LogLevel level, std::string channel = "SS")
      : _level(level), _messagePrefix("__" + channel + ": ") {
  }

  template <class... Args> Logger log(LogLevel level, Args const &... args) {
    if (level <= _level) {
      return logArgs(level, args...);
    }
    return *this;
  }

  // By default this logs debugging messages
  template <class Args> Logger &operator<<(Args args) {
    log(LogLevelInfo, args);
    return *this;
  }

  auto level() {
    return _level;
  };

private:
  Logger &logArgs(LogLevel level) {
    return *this;
  }

  template <class Arg, class... Args>
  Logger &logArgs(LogLevel level, Arg const &arg, Args const &... args) {
    if (level == LogLevelError) {
      writeLock();
      std::cerr << _messagePrefix << arg << std::endl;
      writeUnLock();
    } else {
      writeLock();
      std::cout << _messagePrefix << arg << std::endl;
      writeUnLock();
    }
    logArgs(level, args...);
    return *this;
  }

  void writeLock();

  void writeUnLock();
};
} // namespace ssvim
