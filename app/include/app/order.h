#ifndef APP_INCLUDE_APP_ORDER_H_
#define APP_INCLUDE_APP_ORDER_H_
namespace ccapi {
class Order {
public:
  std::string orderId;
  std::string side;
  Decimal limitPrice;
  Decimal quantity;
  Decimal cumulativeFilledQuantity;
  std::string status;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_ORDER_H_
