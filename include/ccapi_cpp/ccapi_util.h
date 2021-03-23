#ifndef INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#include <chrono>

namespace {
    //used for ftx authentication
    constexpr char hexmap[] = {'0',
                               '1',
                               '2',
                               '3',
                               '4',
                               '5',
                               '6',
                               '7',
                               '8',
                               '9',
                               'a',
                               'b',
                               'c',
                               'd',
                               'e',
                               'f'};
}


namespace ccapi {
typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> TimePoint;
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
