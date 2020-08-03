#ifndef INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_LIST_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_LIST_H_
#include <vector>
#include <set>
#include "ccapi_cpp/ccapi_subscription.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class SubscriptionList final {
 public:
  void add(Subscription subscription) {
    this->subscriptionList.push_back(subscription);
  }
  const std::vector<Subscription>& getSubscriptionList() const {
    return subscriptionList;
  }
  void setSubscriptionList(const std::vector<Subscription>& subscriptionList) {
    this->subscriptionList = subscriptionList;
  }
  std::string toString() const {
    return ccapi::toString(subscriptionList);
  }

 private:
  std::vector<Subscription> subscriptionList;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_LIST_H_
