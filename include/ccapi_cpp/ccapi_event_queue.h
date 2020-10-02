#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class EventQueue final {
 public:
  void push(Event&& event) {
    {
      std::lock_guard<std::mutex> lock(this->lock);
      if (this->maxSize == -1 || this->queue.size() < this->maxSize) {
        CCAPI_LOGGER_TRACE("this->queue.size() = "+size_tToString(this->queue.size()));
        this->queue.push_back(event);
      } else {
        CCAPI_LOGGER_WARN("event queue is full!");
      }
    }
    this->cv.notify_all();
  }
  Event& front() {
    std::lock_guard<std::mutex> lock(this->lock);
    return this->queue.front();
  }
  std::vector<Event> purge() {
    std::lock_guard<std::mutex> lock(this->lock);
    std::vector<Event> p;
    std::swap(p, this->queue);
    return p;
  }
  size_t size() const {
    std::lock_guard<std::mutex> lock(this->lock);
    return this->queue.size();
  }
  size_t getMaxSize() const {
    return maxSize;
  }
  void setMaxSize(size_t maxSize) {
    this->maxSize = maxSize;
  }
  std::condition_variable cv;

 private:
  std::vector<Event> queue;
  mutable std::mutex lock;
  size_t maxSize{};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
