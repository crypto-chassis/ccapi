#ifndef INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
#include <future>
#include <string>
namespace ccapi {
/**
 * This class is used for retrying http requests for the REST API.
 */
class HttpRetry CCAPI_FINAL {
 public:
  explicit HttpRetry(int numRetry = 0, int numRedirect = 0, std::string redirectUrlStr = "",
                     std::shared_ptr<std::promise<void> > promisePtr = std::shared_ptr<std::promise<void> >(nullptr))
      : numRetry(numRetry), numRedirect(numRedirect), promisePtr(promisePtr) {}
  std::string toString() const {
    std::ostringstream oss;
    oss << promisePtr;
    std::string output =
        "HttpConnection [numRetry = " + ccapi::toString(numRetry) + ", numRedirect = " + ccapi::toString(numRedirect) + ", promisePtr = " + oss.str() + "]";
    return output;
  }
  int numRetry;
  int numRedirect;
  std::shared_ptr<std::promise<void> > promisePtr;
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
