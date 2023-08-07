#ifndef INCLUDE_CCAPI_CPP_CCAPI_QUEUE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_QUEUE_H_
#include <mutex>
#include <queue>
#include <vector>

#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
/**
 * This class represents a generic FIFO queue.
 */
template <class T>
class Queue {
 public:
  std::string EXCEPTION_QUEUE_FULL = "queue is full";
  std::string EXCEPTION_QUEUE_EMPTY = "queue is empty";
  explicit Queue(const size_t maxSize = 0) : maxSize(maxSize) {}
  void pushBack(const T& t) {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    if (this->maxSize <= 0 || this->queue.size() < this->maxSize) {
      CCAPI_LOGGER_TRACE("this->queue.size() = " + size_tToString(this->queue.size()));
      this->queue.push_back(t);
    } else {
      throw std::runtime_error(EXCEPTION_QUEUE_FULL);
    }
  }
  void pushBack(T&& t) {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    if (this->maxSize <= 0 || this->queue.size() < this->maxSize) {
      CCAPI_LOGGER_TRACE("this->queue.size() = " + size_tToString(this->queue.size()));
      this->queue.push_back(t);
    } else {
      throw std::runtime_error(EXCEPTION_QUEUE_FULL);
    }
  }
  T popBack() {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    if (this->queue.empty()) {
      throw std::runtime_error(EXCEPTION_QUEUE_EMPTY);
    } else {
      T t = std::move(this->queue.back());
      this->queue.pop_back();
      return t;
    }
  }
  std::vector<T> purge() {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    std::vector<T> p;
    std::swap(p, this->queue);
    return p;
  }
  void removeAll(std::vector<T>& c) {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    if (c.empty()) {
      c = std::move(this->queue);
    } else {
      c.reserve(c.size() + this->queue.size());
      std::move(std::begin(this->queue), std::end(this->queue), std::back_inserter(c));
    }
    this->queue.clear();
  }
  size_t size() const {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    return this->queue.size();
  }
  bool empty() const {
#ifndef CCAPI_USE_SINGLE_THREAD
    std::lock_guard<std::mutex> lock(this->m);
#endif
    return this->queue.empty();
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::vector<T> queue;
#ifndef CCAPI_USE_SINGLE_THREAD
  mutable std::mutex m;
#endif
  size_t maxSize{};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_QUEUE_H_
