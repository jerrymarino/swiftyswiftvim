#import "Logging.hpp"
#import <mutex>
std::mutex ioMutex;

void ssvim::Logger::writeLock() {
  ioMutex.lock();
}

void ssvim::Logger::writeUnLock() {
  ioMutex.unlock();
}
