#ifndef APP_INCLUDE_APP_ORDER_H_
#define APP_INCLUDE_APP_ORDER_H_
#include "ccapi_cpp/ccapi_decimal.h"
namespace ccapi {
class Order {
 public:
  std::string toString() const {
    std::string output = "Order [orderId = " + orderId + ", side = " + side + ", limitPrice = " + limitPrice.toString() +
                         ", quantity = " + quantity.toString() + ", cumulativeFilledQuantity = " + cumulativeFilledQuantity.toString() +
                         ", remainingQuantity = " + remainingQuantity.toString() + ", status = " + status + "]";
    return output;
  }
  void reset() {
    this->orderId = "";
    this->clientOrderId = "";
    this->side = "";
    this->limitPrice = Decimal();
    this->quantity = Decimal();
    this->cumulativeFilledQuantity = Decimal();
    this->remainingQuantity = Decimal();
    this->status = "";
  }
  std::string orderId;
  std::string clientOrderId;
  std::string side;
  Decimal limitPrice;
  Decimal quantity;
  Decimal cumulativeFilledQuantity;
  Decimal remainingQuantity;
  std::string status;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_ORDER_H_
