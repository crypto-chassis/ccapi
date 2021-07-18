#ifndef APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#define APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#include "app/common.h"
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class SpotMarketMakingEventHandler : public EventHandler {
 public:
  SpotMarketMakingEventHandler(AppLogger* appLogger, CsvWriter* privateTradeCsvWriter, CsvWriter* accountBalanceCsvWriter)
      : EventHandler(), appLogger(appLogger), privateTradeCsvWriter(privateTradeCsvWriter), accountBalanceCsvWriter(accountBalanceCsvWriter) {}

  bool processEvent(const Event& event, Session* session) override {
    this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "Received an event: " + event.toString() + ".");
    auto firstMessage = event.getMessageList().at(0);
    std::vector<Request> requestList;
    auto eventType = event.getType();
    if (eventType == Event::Type::SUBSCRIPTION_DATA) {
      if (firstMessage.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH && firstMessage.getRecapType() == Message::RecapType::NONE) {
        TimePoint messageTime{std::chrono::seconds{0}};
        for (const auto& message : event.getMessageList()) {
          messageTime = message.getTime();
          for (const auto& element : message.getElementList()) {
            if (element.has("BID_PRICE")) {
              bestBidPrice = element.getValue("BID_PRICE");
            }
            if (element.has("ASK_PRICE")) {
              bestAskPrice = element.getValue("ASK_PRICE");
            }
          }
        }
        std::string messageTimeISO = UtilTime::getISOTimestamp(messageTime);
        this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "Message time is " + messageTimeISO + ".");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      "cancelOpenOrdersLastTime time is " + UtilTime::getISOTimestamp(this->cancelOpenOrdersLastTime) + ".");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      "this->accountBalanceRefreshWaitSeconds is " + std::to_string(this->accountBalanceRefreshWaitSeconds) + ".");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      "getAccountBalancesLastTime time is " + UtilTime::getISOTimestamp(this->getAccountBalancesLastTime) + ".");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      "orderRefreshLastTime time is " + UtilTime::getISOTimestamp(this->orderRefreshLastTime) + ".");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,"Message time is " + UtilTime::getISOTimestamp(this->cancelOpenOrdersLastTime)
        // + ".");
        if ((this->orderRefreshIntervalOffsetSeconds == -1 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->orderRefreshLastTime).count() >= this->orderRefreshIntervalSeconds) ||
            (this->orderRefreshIntervalOffsetSeconds > 0 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime.time_since_epoch()).count() % this->orderRefreshIntervalSeconds ==
                 this->orderRefreshIntervalOffsetSeconds)) {
          this->cancelOpenOrdersRequestCorrelationId = messageTimeISO + "-CANCEL_OPEN_ORDERS";
          Request request(Request::Operation::CANCEL_OPEN_ORDERS, this->exchange, this->instrument, this->cancelOpenOrdersRequestCorrelationId);
          requestList.push_back(request);
          this->orderRefreshLastTime = messageTime;
          this->cancelOpenOrdersLastTime = messageTime;
        } else if (std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->cancelOpenOrdersLastTime).count() >=
                       this->accountBalanceRefreshWaitSeconds &&
                   this->getAccountBalancesLastTime < this->cancelOpenOrdersLastTime &&
                   this->cancelOpenOrdersLastTime + std::chrono::seconds(this->accountBalanceRefreshWaitSeconds) > this->orderRefreshLastTime) {
          this->getAccountBalancesRequestCorrelationId = UtilTime::getISOTimestamp(messageTime) + "-GET_ACCOUNT_BALANCES";
          Request request(this->useGetAccountsToGetAccountBalances ? Request::Operation::GET_ACCOUNTS : Request::Operation::GET_ACCOUNT_BALANCES,
                          this->exchange, "", this->getAccountBalancesRequestCorrelationId);
          if (!this->accountId.empty()) {
            request.appendParam({
                {"ACCOUNT_ID", this->accountId},
            });
          }
          requestList.push_back(request);
          this->getAccountBalancesLastTime = messageTime;
          // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,"About to send requests " + toString(requestList) + ".");
        }
      } else if (firstMessage.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE) {
        // SAVE trades csv
      }
    } else if (eventType == Event::Type::RESPONSE) {
      auto correlationIdList = firstMessage.getCorrelationIdList();
      auto messageTimeReceived = firstMessage.getTimeReceived();
      // if (std::find(correlationIdList.begin(), correlationIdList.end(), this->cancelOpenOrdersRequestCorrelationId) != correlationIdList.end()) {
      //   // this->cancelOpenOrdersLastTime = messageTimeReceived;
      //
      // } else
      if (std::find(correlationIdList.begin(), correlationIdList.end(), this->getAccountBalancesRequestCorrelationId) != correlationIdList.end()) {
        for (const auto& element : firstMessage.getElementList()) {
          auto asset = element.getValue("ASSET");
          if (asset == this->baseAsset) {
            this->baseBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->baseAvailableBalanceProportion;
          } else if (asset == this->quoteAsset) {
            this->quoteBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->quoteAvailableBalanceProportion;
          }
        }
        //print scientific
        this->appLogger->log(
            CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
            this->baseAsset + " balance is " + std::to_string(baseBalance) + ", " + this->quoteAsset + " balance is " + std::to_string(quoteBalance));
        // write csv
        this->placeOrders(requestList);
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), "GET_INSTRUMENT") != correlationIdList.end()) {
        auto element = firstMessage.getElementList().at(0);
        this->baseAsset = element.getValue("BASE_ASSET");
        this->quoteAsset = element.getValue("QUOTE_ASSET");
        this->orderPriceIncrement = element.getValue("PRICE_INCREMENT");
        this->orderQuantityIncrement = element.getValue("QUANTITY_INCREMENT");
        this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
                             "Base asset is " + this->baseAsset + ", quote asset is " + this->quoteAsset + ", order price increment is " +
                                 this->orderPriceIncrement + ", order quantity increment is " + this->orderQuantityIncrement + ".");
        Subscription subscriptionMarketDepth(this->exchange, this->instrument, "MARKET_DEPTH",
                                             "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0", "MARKET_DEPTH");
        Subscription subscriptionPrivateTrade(this->exchange, this->instrument, "PRIVATE_TRADE", "", "PRIVATE_TRADE");
        session->subscribe({subscriptionMarketDepth, subscriptionPrivateTrade});
      }
    }
    if (!requestList.empty()) {
      this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "About to send requests " + toString(requestList) + ".");
      session->sendRequest(requestList);
    }
    return true;
  }
  void placeOrders(std::vector<Request>& requestList) {
    double midPrice;
    if (!this->bestBidPrice.empty() && !this->bestAskPrice.empty()) {
      midPrice = (std::stod(this->bestBidPrice) + std::stod(this->bestAskPrice)) / 2;
    } else {
      this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "At least one side of the order book is empty. Skip.");
      return;
    }
    this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "Mid price is " + std::to_string(midPrice));
    if (this->baseBalance > 0 || this->quoteBalance > 0) {
      double r = this->baseBalance / (this->baseBalance + this->quoteBalance / midPrice);
      // std::cout.precision(20);
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->quoteBalance));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(midPrice));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->baseBalance));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->orderQuantityProportion));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::string(this->orderQuantityIncrement));
      std::string orderQuantity =
          AppUtil::roundInput((this->quoteBalance / midPrice + this->baseBalance) * this->orderQuantityProportion, this->orderQuantityIncrement, false);
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "r");
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(r));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->inventoryBasePortionMinimum));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->inventoryBasePortionTarget));
      // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, std::to_string(this->inventoryBasePortionMaximum));
      if (r < this->inventoryBasePortionMinimum) {
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "");
        std::string buyPrice = AppUtil::roundInput(midPrice * (1 - halfSpreadMinimum), this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity));
        }
      } else if (r >= this->inventoryBasePortionMinimum && r < this->inventoryBasePortionTarget) {
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "");
        std::string buyPrice = AppUtil::roundInput(midPrice * (1 - halfSpreadMinimum), this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity));
        }
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      std::to_string(AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum,
        //                                                                this->inventoryBasePortionMinimum, this->halfSpreadMaximum, r)));
        std::string sellPrice = AppUtil::roundInput(midPrice * (1 + AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum,
                                                                                               this->inventoryBasePortionMinimum, this->halfSpreadMaximum, r)),
                                                    this->orderPriceIncrement, true);
        if (std::stod(sellPrice) * std::stod(orderQuantity) <= this->baseBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity));
        }
      } else if (r >= this->inventoryBasePortionTarget && r < this->inventoryBasePortionMaximum) {
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "");
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER,
        //                      std::to_string(AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum,
        //                                                                this->inventoryBasePortionMaximum, this->halfSpreadMaximum, r)));
        std::string buyPrice = AppUtil::roundInput(midPrice * (1 - AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum,
                                                                                              this->inventoryBasePortionMaximum, this->halfSpreadMaximum, r)),
                                                   this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity));
        }
        std::string sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
        if (std::stod(sellPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity));
        }
      } else {
        // this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "");
        std::string sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
        if (std::stod(sellPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity));
        }
      }
    } else {
      this->appLogger->log(CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, "Account has no assets. Skip.");
      return;
    }
  }
  std::string exchange, instrument, baseAsset, quoteAsset, accountId, orderPriceIncrement, orderQuantityIncrement;
  double halfSpreadMinimum, halfSpreadMaximum, inventoryBasePortionTarget, inventoryBasePortionMinimum, inventoryBasePortionMaximum, baseBalance, quoteBalance,
      baseAvailableBalanceProportion, quoteAvailableBalanceProportion, orderQuantityProportion;
  int orderRefreshIntervalSeconds, orderRefreshIntervalOffsetSeconds, accountBalanceRefreshWaitSeconds;
  std::string bestBidPrice, bestAskPrice;
  TimePoint orderRefreshLastTime{std::chrono::seconds{0}};
  TimePoint cancelOpenOrdersLastTime{std::chrono::seconds{0}};
  TimePoint getAccountBalancesLastTime{std::chrono::seconds{0}};
  std::string cancelOpenOrdersRequestCorrelationId, getAccountBalancesRequestCorrelationId;
  bool useGetAccountsToGetAccountBalances{};

 private:
  Request createRequestForCreateOrder(const std::string& side, const std::string& price, const std::string& quantity) {
    Request request(Request::Operation::CREATE_ORDER, this->exchange, this->instrument);
    request.appendParam({
        {"SIDE", side},
        {"QUANTITY", quantity},
        {"LIMIT_PRICE", price},
    });
    return request;
  }
  AppLogger* appLogger;
  CsvWriter* privateTradeCsvWriter;
  CsvWriter* accountBalanceCsvWriter;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
