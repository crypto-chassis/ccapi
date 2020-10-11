#ifndef INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
#include <string>
namespace ccapi {
class HttpRetry final {
 public:
  HttpRetry(int numRetry = 0, int numRedirect = 0, std::string redirectUrlStr = "", std::shared_ptr<std::promise<void> > promisePtr = std::make_shared<std::promise<void> >(nullptr)):
  numRetry(numRetry),numRedirect(numRedirect),redirectUrlStr(redirectUrlStr),promisePtr(promisePtr){}
  std::string toString() const {
    std::ostringstream oss;
    oss << promisePtr;
    std::string output = "HttpConnection [numRetry = " + ccapi::toString(numRetry) + ", numRedirect = " + ccapi::toString(numRedirect) + ", redirectUrlStr = " + redirectUrlStr + ", promisePtr = " + oss.str() + "]";
    return output;
  }
  int numRetry;
  int numRedirect;
  std::string redirectUrlStr;
  std::shared_ptr<std::promise<void> > promisePtr;
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HTTP_RETRY_H_
