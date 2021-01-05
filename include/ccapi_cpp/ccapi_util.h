#ifndef INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#include <chrono>
namespace ccapi {
typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> TimePoint;
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
