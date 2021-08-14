#ifndef APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#define APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#include "app/common.h"
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class SpotMarketMakingEventHandler : public EventHandler {
 public:
  SpotMarketMakingEventHandler(AppLogger* appLogger, CsvWriter* privateTradeCsvWriter, CsvWriter* orderUpdateCsvWriter, CsvWriter* accountBalanceCsvWriter)
      : EventHandler(),
        appLogger(appLogger),
        privateTradeCsvWriter(privateTradeCsvWriter),
        orderUpdateCsvWriter(orderUpdateCsvWriter),
        accountBalanceCsvWriter(accountBalanceCsvWriter) {
    this->totalBalancePeak = 0;
  }
  bool processEvent(const Event& event, Session* session) override {
    if (this->printDebug) {
      this->appLogger->log("Received an event: " + event.toStringPretty());
    }
    auto eventType = event.getType();
    std::string output("Received an event: ");
    std::vector<Request> requestList;
    if (eventType == Event::Type::SUBSCRIPTION_DATA) {
      auto messageList = event.getMessageList();
      int index = -1;
      for (int i = 0; i < messageList.size(); ++i) {
        auto message = messageList.at(i);
        if (message.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH && message.getRecapType() == Message::RecapType::NONE) {
          index = i;
        } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE) {
          std::vector<std::vector<std::string>> rows;
          std::string messageTimeISO = UtilTime::getISOTimestamp(message.getTime());
          for (const auto& element : message.getElementList()) {
            std::vector<std::string> row = {
                messageTimeISO,
                element.getValue("TRADE_ID"),
                element.getValue("LAST_EXECUTED_PRICE"),
                element.getValue("LAST_EXECUTED_SIZE"),
                element.getValue("SIDE"),
                element.getValue("IS_MAKER"),
                element.getValue("ORDER_ID"),
                element.getValue("CLIENT_ORDER_ID"),
                element.getValue("FEE_QUANTITY"),
                element.getValue("FEE_ASSET"),
            };
            this->appLogger->log("Private trade - side: " + element.getValue("SIDE") + ", price: " + element.getValue("LAST_EXECUTED_PRICE") +
                                 ", quantity: " + element.getValue("LAST_EXECUTED_SIZE") + ".");
            rows.push_back(row);
          }
          this->privateTradeCsvWriter->writeRows(rows);
          this->privateTradeCsvWriter->flush();
        } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
          std::vector<std::vector<std::string>> rows;
          std::string messageTimeISO = UtilTime::getISOTimestamp(message.getTime());
          for (const auto& element : message.getElementList()) {
            std::vector<std::string> row = {
                messageTimeISO,
                element.getValue("ORDER_ID"),
                element.getValue("CLIENT_ORDER_ID"),
                element.getValue("SIDE"),
                element.getValue("LIMIT_PRICE"),
                element.getValue("QUANTITY"),
                element.getValue("REMAINING_QUANTITY"),
                element.getValue("CUMULATIVE_FILLED_QUANTITY"),
                element.getValue("CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY"),
                element.getValue("STATUS"),
            };
            rows.push_back(row);
          }
          this->orderUpdateCsvWriter->writeRows(rows);
          this->orderUpdateCsvWriter->flush();
        }
      }
      if (index != -1) {
        auto message = messageList.at(index);
        TimePoint messageTime{std::chrono::seconds{0}};
        messageTime = message.getTime();
        for (const auto& element : message.getElementList()) {
          if (element.has("BID_PRICE")) {
            bestBidPrice = element.getValue("BID_PRICE");
          }
          if (element.has("ASK_PRICE")) {
            bestAskPrice = element.getValue("ASK_PRICE");
          }
        }
        std::string messageTimeISO = UtilTime::getISOTimestamp(messageTime);
        if ((this->orderRefreshIntervalOffsetSeconds == -1 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->orderRefreshLastTime).count() >= this->orderRefreshIntervalSeconds) ||
            (this->orderRefreshIntervalOffsetSeconds >= 0 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime.time_since_epoch()).count() % this->orderRefreshIntervalSeconds ==
                 this->orderRefreshIntervalOffsetSeconds)) {
          this->cancelOpenOrdersRequestCorrelationId = messageTimeISO + "-CANCEL_OPEN_ORDERS";
          Request request(Request::Operation::CANCEL_OPEN_ORDERS, this->exchange, this->instrumentRest, this->cancelOpenOrdersRequestCorrelationId);
          requestList.push_back(request);
          this->orderRefreshLastTime = messageTime;
          this->cancelOpenOrdersLastTime = messageTime;
          this->appLogger->log("Cancel open orders.");
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
          this->appLogger->log("Get account balances.");
        }
      }
    } else if (eventType == Event::Type::RESPONSE) {
      auto firstMessage = event.getMessageList().at(0);
      auto correlationIdList = firstMessage.getCorrelationIdList();
      auto messageTimeReceived = firstMessage.getTimeReceived();
      std::string messageTimeReceivedISO = UtilTime::getISOTimestamp(messageTimeReceived);
      if (std::find(correlationIdList.begin(), correlationIdList.end(), this->getAccountBalancesRequestCorrelationId) != correlationIdList.end()) {
        for (const auto& element : firstMessage.getElementList()) {
          auto asset = element.getValue("ASSET");
          if (asset == this->baseAsset) {
            this->baseBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->baseAvailableBalanceProportion;
          } else if (asset == this->quoteAsset) {
            this->quoteBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->quoteAvailableBalanceProportion;
          }
        }
        std::string baseBalanceScientific = AppUtil::printDoubleScientific(baseBalance);
        std::string quoteBalanceScientific = AppUtil::printDoubleScientific(quoteBalance);
        this->accountBalanceCsvWriter->writeRow({
            messageTimeReceivedISO,
            baseBalanceScientific,
            quoteBalanceScientific,
            this->bestBidPrice,
            this->bestAskPrice,
        });
        this->accountBalanceCsvWriter->flush();
        this->appLogger->log(this->baseAsset + " balance is " + baseBalanceScientific + ", " + this->quoteAsset + " balance is " + quoteBalanceScientific);
        this->appLogger->log("Best bid price is " + this->bestBidPrice + ", best ask price is " + this->bestAskPrice + ".");
        this->placeOrders(requestList);
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), "GET_INSTRUMENT") != correlationIdList.end()) {
        auto element = firstMessage.getElementList().at(0);
        this->baseAsset = element.getValue("BASE_ASSET");
        this->quoteAsset = element.getValue("QUOTE_ASSET");
        this->orderPriceIncrement = UtilString::normalizeDecimalString(element.getValue("PRICE_INCREMENT"));
        this->orderQuantityIncrement = UtilString::normalizeDecimalString(element.getValue("QUANTITY_INCREMENT"));
        Subscription subscriptionMarketDepth(this->exchange, this->instrumentWebsocket, "MARKET_DEPTH",
                                             "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0", "MARKET_DEPTH");
        Subscription subscriptionPrivate(this->exchange, this->instrumentWebsocket, "PRIVATE_TRADE,ORDER_UPDATE", "", "PRIVATE_TRADE,ORDER_UPDATE");
        session->subscribe({subscriptionMarketDepth, subscriptionPrivate});
      }
    }
    if (!requestList.empty()) {
      session->sendRequest(requestList);
    }
    return true;
  }
  void placeOrders(std::vector<Request>& requestList) {
    double midPrice;
    if (!this->bestBidPrice.empty() && !this->bestAskPrice.empty()) {
      midPrice = (std::stod(this->bestBidPrice) + std::stod(this->bestAskPrice)) / 2;
    } else {
      this->appLogger->log("At least one side of the order book is empty. Skip.");
      return;
    }
    if (this->baseBalance > 0 || this->quoteBalance > 0) {
      double totalBalance = this->baseBalance * midPrice + this->quoteBalance;
      if (totalBalance > this->totalBalancePeak) {
        this->totalBalancePeak = totalBalance;
      }
      if ((this->totalBalancePeak - totalBalance) / this->totalBalancePeak > this->killSwitchMaximumDrawdown) {
        this->appLogger->log("Kill switch triggered - Maximum drawdown. Exit.");
        std::exit(EXIT_SUCCESS);
      }
      double r = this->baseBalance * midPrice / totalBalance;
      std::string orderQuantity =
          AppUtil::roundInput((this->quoteBalance / midPrice + this->baseBalance) * this->orderQuantityProportion, this->orderQuantityIncrement, false);
      if (r < this->inventoryBasePortionTarget) {
        std::string buyPrice = AppUtil::roundInput(midPrice * (1 - halfSpreadMinimum), this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity));
        }
        std::string sellPrice = AppUtil::roundInput(
            midPrice * (1 + AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 0, this->halfSpreadMaximum, r)),
            this->orderPriceIncrement, true);
        if (std::stod(sellPrice) * std::stod(orderQuantity) <= this->baseBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity));
        }
      } else {
        std::string buyPrice = AppUtil::roundInput(
            midPrice * (1 - AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 1, this->halfSpreadMaximum, r)),
            this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity));
        }
        std::string sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
        if (std::stod(sellPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity));
        }
      }
    } else {
      this->appLogger->log("Account has no assets. Skip.");
      return;
    }
  }
  std::string exchange, instrumentRest, instrumentWebsocket, baseAsset, quoteAsset, accountId, orderPriceIncrement, orderQuantityIncrement;
  double halfSpreadMinimum, halfSpreadMaximum, inventoryBasePortionTarget, baseBalance, quoteBalance, baseAvailableBalanceProportion,
      quoteAvailableBalanceProportion, orderQuantityProportion, totalBalancePeak, killSwitchMaximumDrawdown;
  int orderRefreshIntervalSeconds, orderRefreshIntervalOffsetSeconds, accountBalanceRefreshWaitSeconds;
  std::string bestBidPrice, bestAskPrice;
  TimePoint orderRefreshLastTime{std::chrono::seconds{0}};
  TimePoint cancelOpenOrdersLastTime{std::chrono::seconds{0}};
  TimePoint getAccountBalancesLastTime{std::chrono::seconds{0}};
  std::string cancelOpenOrdersRequestCorrelationId, getAccountBalancesRequestCorrelationId;
  bool useGetAccountsToGetAccountBalances{}, printDebug{};

 private:
  Request createRequestForCreateOrder(const std::string& side, const std::string& price, const std::string& quantity) {
    Request request(Request::Operation::CREATE_ORDER, this->exchange, this->instrumentRest);
    request.appendParam({
        {"SIDE", side},
        {"QUANTITY", quantity},
        {"LIMIT_PRICE", price},
    });
    this->appLogger->log("Place order - side: " + side + ", price: " + price + ", quantity: " + quantity + ".");
    return request;
  }
  AppLogger* appLogger;
  CsvWriter* privateTradeCsvWriter;
  CsvWriter* orderUpdateCsvWriter;
  CsvWriter* accountBalanceCsvWriter;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
