#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
#include <queue>
#include <mutex>
#include <vector>
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class EventQueue final {
 public:
  void push(Event&& event) {
    std::lock_guard<std::mutex> lock(this->lock);
    CCAPI_LOGGER_TRACE("this->queue.size() = "+size_tToString(this->queue.size()));
    this->queue.push_back(event);
  }
  Event& front() {
    std::lock_guard<std::mutex> lock(this->lock);
    return this->queue.front();
  }
  std::vector<Event> purge() {
    std::unique_lock<std::mutex> lock(this->lock);
    std::vector<Event> p;
    std::swap(p, this->queue);
    lock.unlock();
    return p;
  }
  size_t size() const {
    return this->queue.size();
  }

 private:
  std::vector<Event> queue;
  std::mutex lock;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_QUEUE_H_
