#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_DISPATCHER_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_DISPATCHER_H_
#include <stddef.h>
#include <functional>
#include <vector>
#include <queue>
#include <atomic>
#ifdef CCAPI_CPP_MINGW_THREAD
#include <mingw-threads/mingw.thread.h>
#include <mingw-threads/mingw.mutex.h>
#include <mingw-threads/mingw.condition_variable.h>
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#endif
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class EventDispatcher CCAPI_FINAL {
 public:
  explicit EventDispatcher(const int numDispatcherThreads = 1)
      : numDispatcherThreads(numDispatcherThreads) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("numDispatcherThreads = "+size_tToString(numDispatcherThreads));
    this->start();
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  ~EventDispatcher() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void dispatch(const std::function<void()>& op) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->shouldContinue.load()) {
      CCAPI_LOGGER_TRACE("start to dispatch an operation");
      std::unique_lock<std::mutex> lock(this->lock);
      this->queue.push(op);
      // Manual unlocking is done before notifying, to avoid waking up
      // the waiting thread only to block again (see notify_one for details)
      lock.unlock();
      this->cv.notify_all();
    } else {
      CCAPI_LOGGER_WARN("dispatching of events were paused");
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void start() {
    this->shouldContinue = true;
    for (size_t i = 0; i < numDispatcherThreads; i++) {
      this->dispatcherThreads.push_back(std::thread(&EventDispatcher::dispatch_thread_handler, this));
    }
  }
  void resume() {
    this->shouldContinue = true;
  }
  void pause() {
    this->shouldContinue = false;
  }
  void stop() {
    std::unique_lock<std::mutex> lock(this->lock);
    this->quit = true;
    lock.unlock();
    this->cv.notify_all();
    for (auto& dispatcherThread : this->dispatcherThreads) {
      dispatcherThread.join();
    }
  }

 private:
  size_t numDispatcherThreads;
  std::atomic<bool> shouldContinue{};
  std::vector<std::thread> dispatcherThreads;
  std::mutex lock;
  std::queue<std::function<void()> > queue;
  std::condition_variable cv;
  bool quit{};
  void dispatch_thread_handler() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::unique_lock<std::mutex> lock(this->lock);
    do {
      CCAPI_LOGGER_TRACE("predicate is "+size_tToString(this->queue.size()));
      this->cv.wait(lock, [&] {
        return (this->queue.size() || this->quit);
      });
      CCAPI_LOGGER_TRACE("wait has exited");
      CCAPI_LOGGER_TRACE("after wait, this->queue.size() = "+size_tToString(this->queue.size()));
      if (!this->quit && this->queue.size()) {
        auto op = std::move(this->queue.front());
        this->queue.pop();
        lock.unlock();
        op();
        lock.lock();
      }
    } while (!this->quit);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_DISPATCHER_H_
