#ifndef APP_INCLUDE_APP_EVENT_HANDLER_BASE_H_
#define APP_INCLUDE_APP_EVENT_HANDLER_BASE_H_
#ifndef APP_EVENT_HANDLER_BASE_ORDER_STATUS_NEW
#define APP_EVENT_HANDLER_BASE_ORDER_STATUS_NEW "NEW"
#endif
#ifndef APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED
#define APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED "CANCELED"
#endif
#ifndef APP_EVENT_HANDLER_BASE_ORDER_STATUS_PARTIALLY_FILLED
#define APP_EVENT_HANDLER_BASE_ORDER_STATUS_PARTIALLY_FILLED "PARTIALLY_FILLED"
#endif
#ifndef APP_EVENT_HANDLER_BASE_ORDER_STATUS_FILLED
#define APP_EVENT_HANDLER_BASE_ORDER_STATUS_FILLED "FILLED"
#endif
#ifndef APP_PUBLIC_TRADE_LAST
#define APP_PUBLIC_TRADE_LAST 0
#endif
#ifndef APP_PUBLIC_TRADE_VOLUME
#define APP_PUBLIC_TRADE_VOLUME 1
#endif
#ifndef APP_PUBLIC_TRADE_VOLUME_IN_QUOTE
#define APP_PUBLIC_TRADE_VOLUME_IN_QUOTE 2
#endif
#include <sys/stat.h>

#include <random>
#include <sstream>

#include "app/common.h"
#include "app/historical_market_data_event_processor.h"
#include "app/order.h"
#include "boost/optional/optional.hpp"
#ifndef CCAPI_APP_IS_BACKTEST
#include "ccapi_cpp/ccapi_session.h"
#else
#include <future>

#include "ccapi_cpp/ccapi_element.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace ccapi {
class Session {
 public:
  virtual void subscribe(std::vector<Subscription>& subscriptionList) {}
  virtual void sendRequest(const Event& event, Session* session, std::vector<Request>& requestList) {}
  virtual void sendRequest(Request& request) {}
  virtual void sendRequestByWebsocket(Request& request) {}
  virtual void stop() {}
};
}  // namespace ccapi
#endif
// #include <filesystem>
namespace ccapi {
class EventHandlerBase : public EventHandler {
 public:
  enum class AppMode {
    MARKET_MAKING,
    SINGLE_ORDER_EXECUTION,
  };
  enum class TradingMode {
    LIVE,
    PAPER,
    BACKTEST,
  };
  enum class AdverseSelectionGuardActionType {
    NONE,
    MAKE,
    TAKE,
  };
  enum class AdverseSelectionGuardInformedTraderSide {
    NONE,
    BUY,
    SELL,
  };
  enum class TradingStrategy {
    TWAP,
    VWAP,
    POV,
    IS,
  };
  virtual ~EventHandlerBase() {}
  virtual void onInit(Session* session) {}
  bool processEvent(const Event& event, Session* session) override {
    if (this->skipProcessEvent) {
      return true;
    }
    APP_LOGGER_DEBUG("********");
    APP_LOGGER_DEBUG("Received an event: " + event.toStringPretty());
    if (this->openBuyOrder) {
      APP_LOGGER_DEBUG("Open buy order is " + this->openBuyOrder->toString() + ".");
    }
    if (this->openSellOrder) {
      APP_LOGGER_DEBUG("Open sell order is " + this->openSellOrder->toString() + ".");
    }
    APP_LOGGER_DEBUG(this->baseAsset + " balance is " + Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString() + ", " + this->quoteAsset +
                     " balance is " + Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString() + ".");
    auto eventType = event.getType();
    std::vector<Request> requestList;
    if (eventType == Event::Type::SUBSCRIPTION_DATA) {
      const auto& messageList = event.getMessageList();
      int index = -1;
      for (int i = 0; i < messageList.size(); ++i) {
        const auto& message = messageList.at(i);
        if (message.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH && message.getRecapType() == Message::RecapType::NONE) {
          index = i;
        } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE) {
          if (!this->privateDataOnlySaveFinalSummary && this->privateTradeCsvWriter) {
            std::vector<std::vector<std::string>> rows;
            const std::string& messageTimeISO = UtilTime::getISOTimestamp(message.getTime());
            for (const auto& element : message.getElementList()) {
              std::vector<std::string> row = {
                  messageTimeISO,
                  element.getValue(CCAPI_TRADE_ID),
                  element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE),
                  element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE),
                  element.getValue(CCAPI_EM_ORDER_SIDE),
                  element.getValue(CCAPI_IS_MAKER),
                  element.getValue(CCAPI_EM_ORDER_ID),
                  element.getValue(CCAPI_EM_CLIENT_ORDER_ID),
                  element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY),
                  element.getValue(CCAPI_EM_ORDER_FEE_ASSET),
              };
              APP_LOGGER_INFO("Private trade - side: " + element.getValue(CCAPI_EM_ORDER_SIDE) +
                              ", price: " + element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE) +
                              ", quantity: " + element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE) + ".");
              rows.emplace_back(std::move(row));
            }
            this->privateTradeCsvWriter->writeRows(rows);
            this->privateTradeCsvWriter->flush();
          }
          for (const auto& element : message.getElementList()) {
            double lastExecutedPrice = std::stod(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE));
            double lastExecutedSize = std::stod(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE));
            const auto& feeQuantityStr = element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY);
            double feeQuantity = feeQuantityStr.empty() ? 0 : std::stod(feeQuantityStr);
            std::string feeAsset = element.getValue(CCAPI_EM_ORDER_FEE_ASSET);
            bool isMaker = element.getValue(CCAPI_IS_MAKER) == "1";
            if (this->tradingMode == TradingMode::LIVE) {
              std::string side = element.getValue(CCAPI_EM_ORDER_SIDE);
              if (side == CCAPI_EM_ORDER_SIDE_BUY) {
                this->baseBalance += lastExecutedSize;
                this->quoteBalance -= lastExecutedPrice * lastExecutedSize;
              } else {
                this->baseBalance -= lastExecutedSize;
                this->quoteBalance += lastExecutedPrice * lastExecutedSize;
              }
              this->updateAccountBalancesByFee(feeAsset, feeQuantity, side, isMaker);
            }
            this->privateTradeVolumeInBaseSum += lastExecutedSize;
            this->privateTradeVolumeInQuoteSum += lastExecutedSize * lastExecutedPrice;
            if (feeAsset == this->baseAsset) {
              this->privateTradeFeeInBaseSum += feeQuantity;
              this->privateTradeFeeInQuoteSum += feeQuantity * lastExecutedPrice;
            } else if (feeAsset == this->quoteAsset) {
              this->privateTradeFeeInBaseSum += feeQuantity / lastExecutedPrice;
              this->privateTradeFeeInQuoteSum += feeQuantity;
            }
          }
        } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
          if (this->numOpenOrders > 0) {
            for (const auto& element : message.getElementList()) {
              auto quantity = element.getValue(CCAPI_EM_ORDER_QUANTITY);
              auto cumulativeFilledQuantity = element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY);
              auto remainingQuantity = element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY);
              bool filled = false;
              if (!quantity.empty() && !cumulativeFilledQuantity.empty()) {
                filled = Decimal(quantity).toString() == Decimal(cumulativeFilledQuantity).toString();
              } else if (!remainingQuantity.empty()) {
                filled = Decimal(remainingQuantity).toString() == "0";
              }
              if (filled) {
                this->numOpenOrders -= 1;
              }
            }
            if (this->numOpenOrders == 0) {
              APP_LOGGER_INFO("All open orders are filled.");
              if (this->immediatelyPlaceNewOrders) {
                const auto& messageTimeReceived = message.getTimeReceived();
                const auto& messageTimeReceivedISO = UtilTime::getISOTimestamp(messageTimeReceived);
                this->orderRefreshLastTime = messageTimeReceived;
                this->cancelOpenOrdersLastTime = messageTimeReceived;
              }
            }
          }
          if (!this->privateDataOnlySaveFinalSummary && this->orderUpdateCsvWriter) {
            std::vector<std::vector<std::string>> rows;
            const std::string& messageTimeISO = UtilTime::getISOTimestamp(message.getTime());
            for (const auto& element : message.getElementList()) {
              std::vector<std::string> row = {
                  messageTimeISO,
                  element.getValue(CCAPI_EM_ORDER_ID),
                  element.getValue(CCAPI_EM_CLIENT_ORDER_ID),
                  element.getValue(CCAPI_EM_ORDER_SIDE),
                  element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE),
                  element.getValue(CCAPI_EM_ORDER_QUANTITY),
                  element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY),
                  element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY),
                  element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY),
                  element.getValue(CCAPI_EM_ORDER_STATUS),
              };
              rows.emplace_back(std::move(row));
            }
            this->orderUpdateCsvWriter->writeRows(rows);
            this->orderUpdateCsvWriter->flush();
          }
        } else if (message.getType() == Message::Type::MARKET_DATA_EVENTS_TRADE || message.getType() == Message::Type::MARKET_DATA_EVENTS_AGG_TRADE) {
          const auto& messageTime = message.getTime();
          if (message.getRecapType() == Message::RecapType::NONE) {
            if (this->tradingMode == TradingMode::PAPER || this->tradingMode == TradingMode::BACKTEST) {
              for (const auto& element : message.getElementList()) {
                bool isBuyerMaker = element.getValue(CCAPI_IS_BUYER_MAKER) == "1";
                const auto& takerPrice = Decimal(element.getValue(CCAPI_LAST_PRICE));
                Order order;
                if (isBuyerMaker && this->openBuyOrder) {
                  order = this->openBuyOrder.get();
                } else if (!isBuyerMaker && this->openSellOrder) {
                  order = this->openSellOrder.get();
                }
                if ((isBuyerMaker && this->openBuyOrder && takerPrice <= this->openBuyOrder.get().limitPrice) ||
                    (!isBuyerMaker && this->openSellOrder && takerPrice >= this->openSellOrder.get().limitPrice)) {
                  const auto& takerQuantity = Decimal(element.getValue(CCAPI_LAST_SIZE));
                  Decimal lastFilledQuantity;
                  if (takerQuantity < order.remainingQuantity) {
                    lastFilledQuantity = takerQuantity;
                    order.cumulativeFilledQuantity = order.cumulativeFilledQuantity.add(lastFilledQuantity);
                    order.remainingQuantity = order.remainingQuantity.subtract(lastFilledQuantity);
                    order.status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_PARTIALLY_FILLED;
                    if (isBuyerMaker) {
                      this->openBuyOrder = order;
                    } else {
                      this->openSellOrder = order;
                    }
                  } else {
                    lastFilledQuantity = order.remainingQuantity;
                    order.cumulativeFilledQuantity = order.quantity;
                    order.remainingQuantity = Decimal("0");
                    order.status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_FILLED;
                    if (isBuyerMaker) {
                      this->openBuyOrder = boost::none;
                    } else {
                      this->openSellOrder = boost::none;
                    }
                  }
                  if (isBuyerMaker) {
                    this->baseBalance += lastFilledQuantity.toDouble();
                    this->quoteBalance -= order.limitPrice.toDouble() * lastFilledQuantity.toDouble();
                  } else {
                    this->baseBalance -= lastFilledQuantity.toDouble();
                    this->quoteBalance += order.limitPrice.toDouble() * lastFilledQuantity.toDouble();
                  }
                  double feeQuantity = 0;
                  if ((isBuyerMaker && UtilString::toLower(this->makerBuyerFeeAsset) == UtilString::toLower(this->baseAsset)) ||
                      (!isBuyerMaker && UtilString::toLower(this->makerSellerFeeAsset) == UtilString::toLower(this->baseAsset))) {
                    feeQuantity = lastFilledQuantity.toDouble() * this->makerFee;
                    this->baseBalance -= feeQuantity;
                  } else if ((isBuyerMaker && UtilString::toLower(this->makerBuyerFeeAsset) == UtilString::toLower(this->quoteAsset)) ||
                             (!isBuyerMaker && UtilString::toLower(this->makerSellerFeeAsset) == UtilString::toLower(this->quoteAsset))) {
                    feeQuantity = order.limitPrice.toDouble() * lastFilledQuantity.toDouble() * this->makerFee;
                    this->quoteBalance -= feeQuantity;
                  }
                  Event virtualEvent;
                  virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
                  Message messagePrivateTrade;
                  messagePrivateTrade.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                  messagePrivateTrade.setTime(messageTime);
                  messagePrivateTrade.setTimeReceived(messageTime);
                  messagePrivateTrade.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
                  Element elementPrivateTrade;
                  elementPrivateTrade.insert(CCAPI_TRADE_ID, std::to_string(++this->virtualTradeId));
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, order.limitPrice.toString());
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, lastFilledQuantity.toString());
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_SIDE, order.side);
                  elementPrivateTrade.insert(CCAPI_IS_MAKER, "1");
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_ID, order.orderId);
                  elementPrivateTrade.insert(CCAPI_EM_CLIENT_ORDER_ID, order.clientOrderId);
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_FEE_QUANTITY, Decimal(UtilString::printDoubleScientific(feeQuantity)).toString());
                  elementPrivateTrade.insert(CCAPI_EM_ORDER_FEE_ASSET, isBuyerMaker ? this->makerBuyerFeeAsset : this->makerSellerFeeAsset);
                  std::vector<Element> elementListPrivateTrade;
                  elementListPrivateTrade.emplace_back(std::move(elementPrivateTrade));
                  messagePrivateTrade.setElementList(elementListPrivateTrade);
                  Message messageOrderUpdate;
                  messageOrderUpdate.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
                  messageOrderUpdate.setTime(messageTime);
                  messageOrderUpdate.setTimeReceived(messageTime);
                  messageOrderUpdate.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
                  Element elementOrderUpdate;
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_ID, order.orderId);
                  elementOrderUpdate.insert(CCAPI_EM_CLIENT_ORDER_ID, order.clientOrderId);
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_SIDE, order.side);
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_LIMIT_PRICE, order.limitPrice.toString());
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_QUANTITY, order.quantity.toString());
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, order.cumulativeFilledQuantity.toString());
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, order.remainingQuantity.toString());
                  elementOrderUpdate.insert(CCAPI_EM_ORDER_STATUS, order.status);
                  std::vector<Element> elementListOrderUpdate;
                  elementListOrderUpdate.emplace_back(std::move(elementOrderUpdate));
                  messageOrderUpdate.setElementList(elementListOrderUpdate);
                  std::vector<Message> messageList;
                  messageList.emplace_back(std::move(messagePrivateTrade));
                  messageList.emplace_back(std::move(messageOrderUpdate));
                  virtualEvent.setMessageList(messageList);
                  APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent.toStringPretty());
                  this->processEvent(virtualEvent, session);
                }
              }
            }
            this->postProcessMessageMarketDataEventTrade(message, messageTime);
          }
        }
      }
      if (index != -1) {
        const auto& message = messageList.at(index);
        const auto& messageTime = message.getTime();
        if (messageTime < this->startTimeTp) {
          return true;
        }
        this->snapshotBid.clear();
        this->snapshotAsk.clear();
        for (const auto& element : message.getElementList()) {
          const auto& elementNameValueMap = element.getNameValueMap();
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_BID_N_PRICE);
            if (it != elementNameValueMap.end()) {
              auto price = it->second;
              if (price != CCAPI_BEST_BID_N_PRICE_EMPTY) {
                this->snapshotBid[Decimal(price)] = elementNameValueMap.at(CCAPI_BEST_BID_N_SIZE);
              }
            }
          }
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_ASK_N_PRICE);
            if (it != elementNameValueMap.end()) {
              auto price = it->second;
              if (price != CCAPI_BEST_ASK_N_PRICE_EMPTY) {
                this->snapshotAsk[Decimal(price)] = elementNameValueMap.at(CCAPI_BEST_ASK_N_SIZE);
              }
            }
          }
          if (this->snapshotBid.empty()) {
            this->bestBidPrice = "";
            this->bestBidSize = "";
          } else {
            auto it = this->snapshotBid.rbegin();
            this->bestBidPrice = it->first.toString();
            this->bestBidSize = it->second;
          }
          if (this->snapshotAsk.empty()) {
            this->bestAskPrice = "";
            this->bestAskSize = "";
          } else {
            auto it = this->snapshotAsk.begin();
            this->bestAskPrice = it->first.toString();
            this->bestAskSize = it->second;
          }
          if (!this->bestBidPrice.empty() && !this->bestAskPrice.empty()) {
            if (this->useWeightedMidPrice) {
              this->midPrice = (std::stod(this->bestBidPrice) * std::stod(this->bestAskSize) + std::stod(this->bestAskPrice) * std::stod(this->bestBidSize)) /
                               (std::stod(this->bestBidSize) + std::stod(this->bestAskSize));
            } else {
              this->midPrice = (std::stod(this->bestBidPrice) + std::stod(this->bestAskPrice)) / 2;
            }
          } else {
            this->midPrice = 0;
          }
        }
        if (this->tradingMode == TradingMode::PAPER || this->tradingMode == TradingMode::BACKTEST) {
          bool buySideCrossed = !this->bestAskPrice.empty() && this->openBuyOrder && Decimal(this->bestAskPrice) <= this->openBuyOrder.get().limitPrice;
          bool sellSideCrossed = !this->bestBidPrice.empty() && this->openSellOrder && Decimal(this->bestBidPrice) >= this->openSellOrder.get().limitPrice;
          if ((buySideCrossed || sellSideCrossed) && this->marketImpfactFactor > 0) {
            Event virtualEvent;
            virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
            Message message;
            message.setType(exchange.rfind("binance", 0) == 0 ? Message::Type::MARKET_DATA_EVENTS_AGG_TRADE : Message::Type::MARKET_DATA_EVENTS_TRADE);
            message.setRecapType(Message::RecapType::NONE);
            message.setTime(messageTime);
            message.setTimeReceived(messageTime);
            message.setCorrelationIdList({PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID});
            std::vector<Element> elementList;
            Element element;
            const Order& order = buySideCrossed ? this->openBuyOrder.get() : this->openSellOrder.get();
            element.insert(CCAPI_LAST_PRICE, order.limitPrice.toString());
            element.insert(CCAPI_LAST_SIZE,
                           Decimal(UtilString::printDoubleScientific(order.remainingQuantity.toDouble() * this->marketImpfactFactor)).toString());
            element.insert(CCAPI_IS_BUYER_MAKER, buySideCrossed ? "1" : "0");
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            virtualEvent.addMessage(message);
            APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent.toStringPretty());
            this->processEvent(virtualEvent, session);
          }
        }
        const std::string& messageTimeISO = UtilTime::getISOTimestamp(messageTime);
        const std::string& messageTimeISODate = messageTimeISO.substr(0, 10);
        if (this->previousMessageTimeISODate.empty() || messageTimeISODate != previousMessageTimeISODate) {
          std::string prefix;
          if (!this->privateDataFilePrefix.empty()) {
            prefix = this->privateDataFilePrefix;
          }
          std::string suffix;
          if (!this->privateDataFileSuffix.empty()) {
            suffix = this->privateDataFileSuffix;
          }
          std::string privateTradeCsvFilename(prefix + this->exchange + "__" + UtilString::toLower(this->baseAsset) + "-" +
                                              UtilString::toLower(this->quoteAsset) + "__" + messageTimeISODate + "__private-trade" + suffix + ".csv"),
              orderUpdateCsvFilename(prefix + this->exchange + "__" + UtilString::toLower(this->baseAsset) + "-" + UtilString::toLower(this->quoteAsset) +
                                     "__" + messageTimeISODate + "__order-update" + suffix + ".csv"),
              accountBalanceCsvFilename(prefix + this->exchange + "__" + UtilString::toLower(this->baseAsset) + "-" + UtilString::toLower(this->quoteAsset) +
                                        "__" + messageTimeISODate + "__account-balance" + suffix + ".csv");
          if (!this->privateDataDirectory.empty()) {
            // std::filesystem::create_directory(std::filesystem::path(this->privateDataDirectory.c_str()));
            privateTradeCsvFilename = this->privateDataDirectory + "/" + privateTradeCsvFilename;
            orderUpdateCsvFilename = this->privateDataDirectory + "/" + orderUpdateCsvFilename;
            accountBalanceCsvFilename = this->privateDataDirectory + "/" + accountBalanceCsvFilename;
          }
          CsvWriter* privateTradeCsvWriter = nullptr;
          CsvWriter* orderUpdateCsvWriter = nullptr;
          CsvWriter* accountBalanceCsvWriter = nullptr;
          if (!privateDataOnlySaveFinalSummary) {
            privateTradeCsvWriter = new CsvWriter();
            {
              struct stat buffer;
              if (stat(privateTradeCsvFilename.c_str(), &buffer) != 0) {
                privateTradeCsvWriter->open(privateTradeCsvFilename, std::ios_base::app);
                privateTradeCsvWriter->writeRow({
                    "TIME",
                    "TRADE_ID",
                    "LAST_EXECUTED_PRICE",
                    "LAST_EXECUTED_SIZE",
                    "SIDE",
                    "IS_MAKER",
                    "ORDER_ID",
                    "CLIENT_ORDER_ID",
                    "FEE_QUANTITY",
                    "FEE_ASSET",
                });
                privateTradeCsvWriter->flush();
              } else {
                privateTradeCsvWriter->open(privateTradeCsvFilename, std::ios_base::app);
              }
            }
            orderUpdateCsvWriter = new CsvWriter();
            {
              struct stat buffer;
              if (stat(orderUpdateCsvFilename.c_str(), &buffer) != 0) {
                orderUpdateCsvWriter->open(orderUpdateCsvFilename, std::ios_base::app);
                orderUpdateCsvWriter->writeRow({
                    "TIME",
                    "ORDER_ID",
                    "CLIENT_ORDER_ID",
                    "SIDE",
                    "LIMIT_PRICE",
                    "QUANTITY",
                    "REMAINING_QUANTITY",
                    "CUMULATIVE_FILLED_QUANTITY",
                    "CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY",
                    "STATUS",
                });
                orderUpdateCsvWriter->flush();
              } else {
                orderUpdateCsvWriter->open(orderUpdateCsvFilename, std::ios_base::app);
              }
            }
          }
          if (!this->privateDataOnlySaveFinalSummary) {
            accountBalanceCsvWriter = new CsvWriter();
            {
              struct stat buffer;
              if (stat(accountBalanceCsvFilename.c_str(), &buffer) != 0) {
                accountBalanceCsvWriter->open(accountBalanceCsvFilename, std::ios_base::app);
                accountBalanceCsvWriter->writeRow({
                    "TIME",
                    "BASE_AVAILABLE_BALANCE",
                    "QUOTE_AVAILABLE_BALANCE",
                    "BEST_BID_PRICE",
                    "BEST_ASK_PRICE",
                });
                accountBalanceCsvWriter->flush();
              } else {
                accountBalanceCsvWriter->open(accountBalanceCsvFilename, std::ios_base::app);
              }
            }
          }
          if (this->privateTradeCsvWriter) {
            delete this->privateTradeCsvWriter;
          }
          this->privateTradeCsvWriter = privateTradeCsvWriter;
          if (this->orderUpdateCsvWriter) {
            delete this->orderUpdateCsvWriter;
          }
          this->orderUpdateCsvWriter = orderUpdateCsvWriter;
          if (this->accountBalanceCsvWriter) {
            delete this->accountBalanceCsvWriter;
          }
          this->accountBalanceCsvWriter = accountBalanceCsvWriter;
        }
        this->previousMessageTimeISODate = messageTimeISODate;
        if ((this->orderRefreshIntervalOffsetSeconds == -1 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->orderRefreshLastTime).count() >= this->orderRefreshIntervalSeconds) ||
            (this->orderRefreshIntervalOffsetSeconds >= 0 &&
             std::chrono::duration_cast<std::chrono::seconds>(messageTime.time_since_epoch()).count() % this->orderRefreshIntervalSeconds ==
                 this->orderRefreshIntervalOffsetSeconds)) {
          this->cancelOpenOrders(event, session, requestList, messageTime, messageTimeISO, false);
        } else if (std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->cancelOpenOrdersLastTime).count() >=
                       this->accountBalanceRefreshWaitSeconds &&
                   this->getAccountBalancesLastTime < this->cancelOpenOrdersLastTime &&
                   this->cancelOpenOrdersLastTime + std::chrono::seconds(this->accountBalanceRefreshWaitSeconds) >= this->orderRefreshLastTime) {
          this->getAccountBalances(event, session, requestList, messageTime, messageTimeISO);
        }
        this->postProcessMessageMarketDataEventMarketDepth(message, messageTime);
      }
    } else if (eventType == Event::Type::RESPONSE) {
      const auto& firstMessage = event.getMessageList().at(0);
      const auto& correlationIdList = firstMessage.getCorrelationIdList();
      const auto& messageTimeReceived = firstMessage.getTimeReceived();
      const auto& messageTimeReceivedISO = UtilTime::getISOTimestamp(messageTimeReceived);
      if (firstMessage.getType() == Message::Type::RESPONSE_ERROR) {
        APP_LOGGER_ERROR(event.toStringPretty() + ".");
      }
      if (std::find(correlationIdList.begin(), correlationIdList.end(), std::string("CREATE_ORDER_") + CCAPI_EM_ORDER_SIDE_BUY) != correlationIdList.end() ||
          std::find(correlationIdList.begin(), correlationIdList.end(), std::string("CREATE_ORDER_") + CCAPI_EM_ORDER_SIDE_SELL) != correlationIdList.end() ||
          (std::find(correlationIdList.begin(), correlationIdList.end(), PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID) != correlationIdList.end() &&
           firstMessage.getType() == Message::Type::CREATE_ORDER)) {
        const auto& element = firstMessage.getElementList().at(0);
        Order order;
        order.orderId = element.getValue(CCAPI_EM_ORDER_ID);
        order.clientOrderId = element.getValue(CCAPI_EM_CLIENT_ORDER_ID);
        order.side = element.getValue(CCAPI_EM_ORDER_SIDE);
        order.limitPrice = Decimal(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE));
        order.quantity = Decimal(element.getValue(CCAPI_EM_ORDER_QUANTITY));
        order.cumulativeFilledQuantity = Decimal("0");
        order.remainingQuantity = order.quantity;
        order.status = element.getValue(CCAPI_EM_ORDER_STATUS);
        bool isBuy = element.getValue(CCAPI_EM_ORDER_SIDE) == CCAPI_EM_ORDER_SIDE_BUY;
        if (isBuy) {
          this->openBuyOrder = order;
        } else {
          this->openSellOrder = order;
        }
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), this->cancelBuyOrderRequestCorrelationId) != correlationIdList.end() ||
                 std::find(correlationIdList.begin(), correlationIdList.end(), this->cancelSellOrderRequestCorrelationId) != correlationIdList.end() ||
                 (std::find(correlationIdList.begin(), correlationIdList.end(), PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID) != correlationIdList.end() &&
                  firstMessage.getType() == Message::Type::CANCEL_ORDER)) {
        const auto& element = firstMessage.getElementList().at(0);
        bool isBuy = element.getValue(CCAPI_EM_ORDER_SIDE) == CCAPI_EM_ORDER_SIDE_BUY;
        if (isBuy) {
          this->openBuyOrder = boost::none;
        } else {
          this->openSellOrder = boost::none;
        }
        if (!this->openBuyOrder && !this->openSellOrder) {
          if (this->accountBalanceRefreshWaitSeconds == 0) {
            this->getAccountBalances(event, session, requestList, messageTimeReceived, messageTimeReceivedISO);
          }
        }
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), this->cancelOpenOrdersRequestCorrelationId) != correlationIdList.end()) {
        if (this->accountBalanceRefreshWaitSeconds == 0) {
          this->getAccountBalances(event, session, requestList, messageTimeReceived, messageTimeReceivedISO);
        }
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), this->getAccountBalancesRequestCorrelationId) != correlationIdList.end()) {
        if (this->tradingMode == TradingMode::LIVE) {
          for (const auto& element : firstMessage.getElementList()) {
            this->extractBalanceInfo(element);
          }
        }
        const auto& baseBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString();
        const auto& quoteBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString();
        if (!this->privateDataOnlySaveFinalSummary && this->accountBalanceCsvWriter &&
            (baseBalanceDecimalNotation != "0" || quoteBalanceDecimalNotation != "0")) {
          this->accountBalanceCsvWriter->writeRow({
              messageTimeReceivedISO,
              baseBalanceDecimalNotation,
              quoteBalanceDecimalNotation,
              this->bestBidPrice,
              this->bestAskPrice,
          });
          this->accountBalanceCsvWriter->flush();
        }
        if (this->numOpenOrders == 0) {
          size_t oldRequestListSize = requestList.size();
          this->placeOrders(event, session, requestList, messageTimeReceived);
          this->numOpenOrders = requestList.size() - oldRequestListSize;
        }
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), "GET_INSTRUMENT") != correlationIdList.end()) {
        const auto& element = firstMessage.getElementList().at(0);
        this->extractInstrumentInfo(element);
        if (this->tradingMode == TradingMode::BACKTEST) {
          HistoricalMarketDataEventProcessor historicalMarketDataEventProcessor(
              std::bind(&EventHandlerBase::processEvent, this, std::placeholders::_1, nullptr));
          historicalMarketDataEventProcessor.exchange = this->exchange;
          historicalMarketDataEventProcessor.baseAsset = UtilString::toLower(this->baseAsset);
          historicalMarketDataEventProcessor.quoteAsset = UtilString::toLower(this->quoteAsset);
          historicalMarketDataEventProcessor.historicalMarketDataStartDateTp = this->historicalMarketDataStartDateTp;
          historicalMarketDataEventProcessor.historicalMarketDataEndDateTp = this->historicalMarketDataEndDateTp;
          historicalMarketDataEventProcessor.historicalMarketDataDirectory = this->historicalMarketDataDirectory;
          historicalMarketDataEventProcessor.historicalMarketDataFilePrefix = this->historicalMarketDataFilePrefix;
          historicalMarketDataEventProcessor.historicalMarketDataFileSuffix = this->historicalMarketDataFileSuffix;
          historicalMarketDataEventProcessor.clockStepSeconds = this->clockStepMilliseconds / 1000;
          historicalMarketDataEventProcessor.startTimeTp = this->startTimeTp;
          historicalMarketDataEventProcessor.totalDurationSeconds = this->totalDurationSeconds;
          historicalMarketDataEventProcessor.processEvent();
          std::string prefix;
          if (!this->privateDataFilePrefix.empty()) {
            prefix = this->privateDataFilePrefix;
          }
          std::string suffix;
          if (!this->privateDataFileSuffix.empty()) {
            suffix = this->privateDataFileSuffix;
          }
          std::string privateDataSummaryCsvFilename(
              prefix + this->exchange + "__" + UtilString::toLower(this->baseAsset) + "-" + UtilString::toLower(this->quoteAsset) + "__" +
              UtilTime::getISOTimestamp(this->historicalMarketDataStartDateTp).substr(0, 10) + "__" +
              UtilTime::getISOTimestamp(this->historicalMarketDataEndDateTp).substr(0, 10) + "__summary" + suffix + ".csv");
          if (!this->privateDataDirectory.empty()) {
            privateDataSummaryCsvFilename = this->privateDataDirectory + "/" + privateDataSummaryCsvFilename;
          }
          CsvWriter* privateDataFinalSummaryCsvWriter = new CsvWriter();
          {
            struct stat buffer;
            if (stat(privateDataSummaryCsvFilename.c_str(), &buffer) != 0) {
              privateDataFinalSummaryCsvWriter->open(privateDataSummaryCsvFilename, std::ios_base::app);
              privateDataFinalSummaryCsvWriter->writeRow({
                  "BASE_AVAILABLE_BALANCE",
                  "QUOTE_AVAILABLE_BALANCE",
                  "BEST_BID_PRICE",
                  "BEST_ASK_PRICE",
                  "TRADE_VOLUME_IN_BASE_SUM",
                  "TRADE_VOLUME_IN_QUOTE_SUM",
                  "TRADE_FEE_IN_BASE_SUM",
                  "TRADE_FEE_IN_QUOTE_SUM",
              });
              privateDataFinalSummaryCsvWriter->flush();
            } else {
              privateDataFinalSummaryCsvWriter->open(privateDataSummaryCsvFilename, std::ios_base::app);
            }
          }
          privateDataFinalSummaryCsvWriter->writeRow({
              Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString(),
              Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString(),
              this->bestBidPrice,
              this->bestAskPrice,
              Decimal(UtilString::printDoubleScientific(this->privateTradeVolumeInBaseSum)).toString(),
              Decimal(UtilString::printDoubleScientific(this->privateTradeVolumeInQuoteSum)).toString(),
              Decimal(UtilString::printDoubleScientific(this->privateTradeFeeInBaseSum)).toString(),
              Decimal(UtilString::printDoubleScientific(this->privateTradeFeeInQuoteSum)).toString(),
          });
          privateDataFinalSummaryCsvWriter->flush();
          delete privateDataFinalSummaryCsvWriter;
          try {
            this->promisePtr->set_value();
          } catch (const std::future_error& e) {
            APP_LOGGER_DEBUG(e.what());
          }
        } else {
          std::vector<Subscription> subscriptionList;
          this->createSubscriptionList(subscriptionList);
          session->subscribe(subscriptionList);
        }
      }
    } else if (eventType == Event::Type::SESSION_STATUS) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::SESSION_CONNECTION_UP) {
          for (const auto& correlationId : message.getCorrelationIdList()) {
            if (correlationId == PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID) {
              const auto& messageTimeReceived = message.getTimeReceived();
              const auto& messageTimeReceivedISO = UtilTime::getISOTimestamp(messageTimeReceived);
              this->cancelOpenOrders(event, session, requestList, messageTimeReceived, messageTimeReceivedISO, true);
            }
          }
        }
      }
    } else if (eventType == Event::Type::SUBSCRIPTION_STATUS) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
          for (const auto& element : message.getElementList()) {
            APP_LOGGER_INFO(element.getValue(CCAPI_INFO_MESSAGE));
          }
        } else if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE) {
          for (const auto& element : message.getElementList()) {
            APP_LOGGER_ERROR(element.getValue(CCAPI_ERROR_MESSAGE));
          }
        }
      }
    }
    this->processEventFurther(event, session, requestList);
    if (!requestList.empty()) {
      if (this->tradingMode == TradingMode::PAPER || this->tradingMode == TradingMode::BACKTEST) {
        for (const auto& request : requestList) {
          bool createdBuyOrder = false;
          const auto& now = request.getTimeSent();
          Event virtualEvent;
          Event virtualEvent_2;
          Event virtualEvent_3;
          Message message;
          Message message_2;
          message.setTime(now);
          message.setTimeReceived(now);
          message_2.setTime(now);
          message_2.setTimeReceived(now);
          message_2.setCorrelationIdList({request.getCorrelationId()});
          std::vector<Element> elementList;
          const auto& operation = request.getOperation();
          if (operation == Request::Operation::GET_ACCOUNT_BALANCES || operation == Request::Operation::GET_ACCOUNTS) {
            virtualEvent.setType(Event::Type::RESPONSE);
            message.setCorrelationIdList({request.getCorrelationId()});
            message.setType(operation == Request::Operation::GET_ACCOUNT_BALANCES ? Message::Type::GET_ACCOUNT_BALANCES : Message::Type::GET_ACCOUNTS);
            std::vector<Element> elementList;
            {
              Element element;
              element.insert(CCAPI_EM_ASSET, this->baseAsset);
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING,
                             Decimal(UtilString::printDoubleScientific(this->baseBalance / this->baseAvailableBalanceProportion)).toString());
              elementList.emplace_back(std::move(element));
            }
            {
              Element element;
              element.insert(CCAPI_EM_ASSET, this->quoteAsset);
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING,
                             Decimal(UtilString::printDoubleScientific(this->quoteBalance / this->quoteAvailableBalanceProportion)).toString());
              elementList.emplace_back(std::move(element));
            }
            message.setElementList(elementList);
            std::vector<Message> messageList;
            messageList.emplace_back(std::move(message));
            virtualEvent.setMessageList(messageList);
          } else if (operation == Request::Operation::CREATE_ORDER) {
            auto newBaseBalance = this->baseBalance;
            auto newQuoteBalance = this->quoteBalance;
            const auto& param = request.getParamList().at(0);
            const auto& side = param.at(CCAPI_EM_ORDER_SIDE);
            const auto& price = param.at(CCAPI_EM_ORDER_LIMIT_PRICE);
            const auto& quantity = param.at(CCAPI_EM_ORDER_QUANTITY);
            const auto& clientOrderId = param.at(CCAPI_EM_CLIENT_ORDER_ID);
            bool sufficientBalance = false;
            if (side == CCAPI_EM_ORDER_SIDE_BUY) {
              double transactedAmount = std::stod(price) * std::stod(quantity);
              if (UtilString::toLower(this->makerBuyerFeeAsset) == UtilString::toLower(this->quoteAsset)) {
                transactedAmount *= 1 + this->makerFee;
              }
              newQuoteBalance -= transactedAmount;
              if (newQuoteBalance >= 0) {
                sufficientBalance = true;
              } else {
                APP_LOGGER_INFO("Insufficient quote balance.");
              }
            } else if (side == CCAPI_EM_ORDER_SIDE_SELL) {
              double transactedAmount = std::stod(quantity);
              if (UtilString::toLower(this->makerSellerFeeAsset) == UtilString::toLower(this->baseAsset)) {
                transactedAmount *= 1 + this->makerFee;
              }
              newBaseBalance -= transactedAmount;
              if (newBaseBalance >= 0) {
                sufficientBalance = true;
              } else {
                APP_LOGGER_INFO("Insufficient base balance.");
              }
            }
            if (sufficientBalance) {
              virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
              message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              Order order;
              order.orderId = std::to_string(++this->virtualOrderId);
              order.clientOrderId = clientOrderId;
              order.side = side;
              order.limitPrice = Decimal(price);
              order.quantity = Decimal(quantity);
              order.cumulativeFilledQuantity = Decimal("0");
              order.remainingQuantity = order.quantity;
              order.status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_NEW;
              Element element;
              this->extractOrderInfo(element, order);
              createdBuyOrder = side == CCAPI_EM_ORDER_SIDE_BUY;
              if (createdBuyOrder) {
                this->openBuyOrder = order;
              } else {
                this->openSellOrder = order;
              }
              std::vector<Element> elementList;
              std::vector<Element> elementList_2;
              elementList.emplace_back(std::move(element));
              elementList_2 = elementList;
              message.setElementList(elementList);
              std::vector<Message> messageList;
              messageList.emplace_back(std::move(message));
              virtualEvent.setMessageList(messageList);
              virtualEvent_2.setType(Event::Type::RESPONSE);
              message_2.setType(Message::Type::CREATE_ORDER);
              message_2.setElementList(elementList_2);
              std::vector<Message> messageList_2;
              messageList_2.emplace_back(std::move(message_2));
              virtualEvent_2.setMessageList(messageList_2);
            } else {
              virtualEvent_2.setType(Event::Type::RESPONSE);
              message_2.setType(Message::Type::RESPONSE_ERROR);
              Element element;
              element.insert(CCAPI_ERROR_MESSAGE, "insufficient balance");
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(element));
              message_2.setElementList(elementList);
              std::vector<Message> messageList_2;
              messageList_2.emplace_back(std::move(message_2));
              virtualEvent_2.setMessageList(messageList_2);
            }
          } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
            virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
            message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            if (this->openBuyOrder) {
              this->openBuyOrder.get().status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED;
              Element element;
              this->extractOrderInfo(element, this->openBuyOrder.get());
              elementList.emplace_back(std::move(element));
              this->openBuyOrder = boost::none;
            }
            if (this->openSellOrder) {
              this->openSellOrder.get().status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED;
              Element element;
              this->extractOrderInfo(element, this->openSellOrder.get());
              elementList.emplace_back(std::move(element));
              this->openSellOrder = boost::none;
            }
            std::vector<Element> elementList_2;
            if (!elementList.empty()) {
              elementList_2 = elementList;
              message.setElementList(elementList);
              virtualEvent.setMessageList({message});
            }
            virtualEvent_2.setType(Event::Type::RESPONSE);
            message_2.setType(Message::Type::CANCEL_OPEN_ORDERS);
            message_2.setElementList(elementList_2);
            std::vector<Message> messageList_2;
            messageList_2.emplace_back(std::move(message_2));
            virtualEvent_2.setMessageList(messageList_2);
          } else if (operation == Request::Operation::CANCEL_ORDER) {
            virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
            message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            if (this->openBuyOrder && request.getCorrelationId() == this->cancelBuyOrderRequestCorrelationId) {
              this->openBuyOrder.get().status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED;
              Element element;
              this->extractOrderInfo(element, this->openBuyOrder.get());
              elementList.emplace_back(std::move(element));
              this->openBuyOrder = boost::none;
            }
            if (this->openSellOrder && request.getCorrelationId() == this->cancelSellOrderRequestCorrelationId) {
              this->openSellOrder.get().status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_CANCELED;
              Element element;
              this->extractOrderInfo(element, this->openSellOrder.get());
              elementList.emplace_back(std::move(element));
              this->openSellOrder = boost::none;
            }
            std::vector<Element> elementList_2;
            if (!elementList.empty()) {
              elementList_2 = elementList;
              message.setElementList(elementList);
              virtualEvent.setMessageList({message});
            }
            virtualEvent_2.setType(Event::Type::RESPONSE);
            message_2.setType(Message::Type::CANCEL_ORDER);
            message_2.setElementList(elementList_2);
            std::vector<Message> messageList_2;
            messageList_2.emplace_back(std::move(message_2));
            virtualEvent_2.setMessageList(messageList_2);
          }
          if (!virtualEvent.getMessageList().empty()) {
            APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent.toStringPretty());
            this->processEvent(virtualEvent, session);
          }
          if (!virtualEvent_2.getMessageList().empty()) {
            APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent_2.toStringPretty());
            this->processEvent(virtualEvent_2, session);
          }
          if (operation == Request::Operation::CREATE_ORDER) {
            if (this->openBuyOrder || this->openSellOrder) {
              std::vector<Element> elementListPrivateTrade;
              auto it = this->snapshotAsk.begin();
              auto rit = this->snapshotBid.rbegin();
              Order matchedOrder = createdBuyOrder ? this->openBuyOrder.get() : this->openSellOrder.get();
              std::string takerFeeAsset = createdBuyOrder ? this->takerBuyerFeeAsset : this->takerSellerFeeAsset;
              while ((createdBuyOrder && it != this->snapshotAsk.end()) || (!createdBuyOrder && rit != this->snapshotBid.rend())) {
                Decimal priceToMatch = createdBuyOrder ? it->first : rit->first;
                if ((createdBuyOrder && this->openBuyOrder->limitPrice >= priceToMatch) ||
                    (!createdBuyOrder && this->openSellOrder->limitPrice <= priceToMatch)) {
                  Decimal quantityToMatch = Decimal(createdBuyOrder ? it->second : rit->second);
                  Decimal matchedQuantity = std::min(quantityToMatch, matchedOrder.remainingQuantity);
                  matchedOrder.cumulativeFilledQuantity = matchedOrder.cumulativeFilledQuantity.add(matchedQuantity);
                  matchedOrder.remainingQuantity = matchedOrder.remainingQuantity.subtract(matchedQuantity);
                  if (createdBuyOrder) {
                    this->baseBalance += matchedQuantity.toDouble();
                    this->quoteBalance -= priceToMatch.toDouble() * matchedQuantity.toDouble();
                  } else {
                    this->baseBalance -= matchedQuantity.toDouble();
                    this->quoteBalance += priceToMatch.toDouble() * matchedQuantity.toDouble();
                  }
                  double feeQuantity = 0;
                  if (UtilString::toLower(takerFeeAsset) == UtilString::toLower(this->baseAsset)) {
                    feeQuantity = matchedQuantity.toDouble() * this->takerFee;
                    this->baseBalance -= feeQuantity;
                  } else if (UtilString::toLower(takerFeeAsset) == UtilString::toLower(this->quoteAsset)) {
                    feeQuantity = priceToMatch.toDouble() * matchedQuantity.toDouble() * this->takerFee;
                    this->quoteBalance -= feeQuantity;
                  }
                  Element element;
                  element.insert(CCAPI_TRADE_ID, std::to_string(++this->virtualTradeId));
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, priceToMatch.toString());
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, matchedQuantity.toString());
                  element.insert(CCAPI_EM_ORDER_SIDE, matchedOrder.side);
                  element.insert(CCAPI_IS_MAKER, "0");
                  element.insert(CCAPI_EM_ORDER_ID, matchedOrder.orderId);
                  element.insert(CCAPI_EM_CLIENT_ORDER_ID, matchedOrder.clientOrderId);
                  element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, Decimal(UtilString::printDoubleScientific(feeQuantity)).toString());
                  element.insert(CCAPI_EM_ORDER_FEE_ASSET, takerFeeAsset);
                  elementListPrivateTrade.emplace_back(std::move(element));
                  if (matchedOrder.remainingQuantity == Decimal("0")) {
                    break;
                  }
                } else {
                  break;
                }
                if (createdBuyOrder) {
                  it++;
                } else {
                  rit++;
                }
              }
              if ((createdBuyOrder && it == this->snapshotAsk.end()) || (!createdBuyOrder && rit == this->snapshotBid.rend())) {
                APP_LOGGER_WARN(std::string("All ") + (createdBuyOrder ? "asks" : "bids") +
                                " in the order book have been depleted. Do your order book data have enough depth?");
              }
              if (matchedOrder.remainingQuantity == Decimal("0")) {
                matchedOrder.status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_FILLED;
                if (createdBuyOrder) {
                  this->openBuyOrder = boost::none;
                } else {
                  this->openSellOrder = boost::none;
                }
              } else {
                matchedOrder.status = APP_EVENT_HANDLER_BASE_ORDER_STATUS_PARTIALLY_FILLED;
                if (createdBuyOrder) {
                  this->openBuyOrder = matchedOrder;
                } else {
                  this->openSellOrder = matchedOrder;
                }
              }
              if (!elementListPrivateTrade.empty()) {
                virtualEvent_3.setType(Event::Type::SUBSCRIPTION_DATA);
                std::vector<Message> messageList;
                {
                  Message message;
                  message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
                  message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                  message.setTime(now);
                  message.setTimeReceived(now);
                  message.setElementList(elementListPrivateTrade);
                  messageList.emplace_back(std::move(message));
                }
                {
                  Message message;
                  message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
                  message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
                  message.setTime(now);
                  message.setTimeReceived(now);
                  Element element;
                  this->extractOrderInfo(element, matchedOrder);
                  std::vector<Element> elementList;
                  elementList.emplace_back(std::move(element));
                  message.setElementList(elementList);
                  messageList.emplace_back(std::move(message));
                }
                virtualEvent_3.setMessageList(messageList);
              }
            }
          }
          if (!virtualEvent_3.getMessageList().empty()) {
            APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent_3.toStringPretty());
            this->processEvent(virtualEvent_3, session);
          }
        }
      } else {
        if (this->useWebsocketToExecuteOrder) {
          for (auto& request : requestList) {
            auto operation = request.getOperation();
            if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER) {
              request.setCorrelationId(PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID);
              session->sendRequestByWebsocket(request);
            } else {
              session->sendRequest(request);
            }
          }
        } else {
          session->sendRequest(requestList);
        }
      }
    }
    return true;
  }
  AppMode appMode{AppMode::MARKET_MAKING};
  std::string previousMessageTimeISODate, exchange, instrumentRest, instrumentWebsocket, baseAsset, quoteAsset, accountId, orderPriceIncrement,
      orderQuantityIncrement, privateDataDirectory, privateDataFilePrefix, privateDataFileSuffix, bestBidPrice, bestBidSize, bestAskPrice, bestAskSize,
      cancelOpenOrdersRequestCorrelationId, getAccountBalancesRequestCorrelationId, cancelBuyOrderRequestCorrelationId, cancelSellOrderRequestCorrelationId;
  double halfSpreadMinimum{}, halfSpreadMaximum{}, inventoryBasePortionTarget{}, baseBalance{}, quoteBalance{}, baseAvailableBalanceProportion{1},
      quoteAvailableBalanceProportion{1}, orderQuantityProportion{}, totalBalancePeak{}, killSwitchMaximumDrawdown{},
      adverseSelectionGuardTriggerInventoryBasePortionMinimum{}, adverseSelectionGuardTriggerInventoryBasePortionMaximum{},
      adverseSelectionGuardActionOrderQuantityProportion{}, adverseSelectionGuardTriggerRollCorrelationCoefficientMaximum{},
      adverseSelectionGuardTriggerRocMinimum{}, adverseSelectionGuardTriggerRocMaximum{}, adverseSelectionGuardTriggerRsiMinimum{},
      adverseSelectionGuardTriggerRsiMaximum{}, privateTradeVolumeInBaseSum{}, privateTradeVolumeInQuoteSum{}, privateTradeFeeInBaseSum{},
      privateTradeFeeInQuoteSum{}, midPrice{};
  int orderRefreshIntervalSeconds{}, orderRefreshIntervalOffsetSeconds{}, accountBalanceRefreshWaitSeconds{}, clockStepMilliseconds{},
      adverseSelectionGuardActionOrderRefreshIntervalSeconds{}, originalOrderRefreshIntervalSeconds{}, adverseSelectionGuardMarketDataSampleIntervalSeconds{},
      adverseSelectionGuardMarketDataSampleBufferSizeSeconds{}, adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations{},
      adverseSelectionGuardTriggerRocNumObservations{}, adverseSelectionGuardTriggerRsiNumObservations{};
  TimePoint orderRefreshLastTime{std::chrono::seconds{0}}, cancelOpenOrdersLastTime{std::chrono::seconds{0}},
      getAccountBalancesLastTime{std::chrono::seconds{0}};
  bool useGetAccountsToGetAccountBalances{}, useCancelOrderToCancelOpenOrders{}, useWebsocketToExecuteOrder{}, useWeightedMidPrice{},
      privateDataOnlySaveFinalSummary{}, enableAdverseSelectionGuard{}, enableAdverseSelectionGuardByInventoryLimit{},
      enableAdverseSelectionGuardByInventoryDepletion{}, enableAdverseSelectionGuardByRollCorrelationCoefficient{},
      adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset{}, enableAdverseSelectionGuardByRoc{}, enableAdverseSelectionGuardByRsi{},
      enableUpdateOrderBookTickByTick{}, immediatelyPlaceNewOrders{}, adverseSelectionGuardTriggerRocOrderDirectionReverse{},
      adverseSelectionGuardTriggerRsiOrderDirectionReverse{}, adverseSelectionGuardTriggerRollCorrelationCoefficientOrderDirectionReverse{},
      enableMarketMaking{};
  TradingMode tradingMode{TradingMode::LIVE};
  AdverseSelectionGuardActionType adverseSelectionGuardActionType{AdverseSelectionGuardActionType::NONE};
  std::shared_ptr<std::promise<void>> promisePtr{nullptr};
  int numOpenOrders{};
  boost::optional<Order> openBuyOrder, openSellOrder;
  std::map<std::string, std::string> credential_2;

  // start: only single order execution
  double totalTargetQuantity{}, quoteTotalTargetQuantity{}, theoreticalRemainingQuantity{}, theoreticalQuoteRemainingQuantity{}, orderPriceLimit{},
      orderPriceLimitRelativeToMidPrice{}, orderQuantityLimitRelativeToTarget{}, twapOrderQuantityRandomizationMax{}, povOrderQuantityParticipationRate{},
      isKapa{};
  TimePoint startTimeTp{std::chrono::seconds{0}};
  int totalDurationSeconds{}, numOrderRefreshIntervals{}, orderRefreshIntervalIndex{-1};
  std::string orderSide;
  TradingStrategy tradingStrategy{TradingStrategy::TWAP};
  // end: only single order execution

  // start: only applicable to paper trade and backtest
  double makerFee{}, takerFee{}, marketImpfactFactor{};
  std::string makerBuyerFeeAsset, makerSellerFeeAsset, takerBuyerFeeAsset, takerSellerFeeAsset;
  // end: only applicable to paper trade and backtest

  // start: only applicable to backtest
  TimePoint historicalMarketDataStartDateTp{std::chrono::seconds{0}}, historicalMarketDataEndDateTp{std::chrono::seconds{0}};
  std::string historicalMarketDataDirectory, historicalMarketDataFilePrefix, historicalMarketDataFileSuffix;
  // end: only applicable to backtest

 protected:
  virtual void processEventFurther(const Event& event, Session* session, std::vector<Request>& requestList) {}
  virtual void createSubscriptionList(std::vector<Subscription>& subscriptionList) {
    {
      std::string options;
      if (this->tradingMode == TradingMode::LIVE) {
        options += std::string(CCAPI_MARKET_DEPTH_MAX) + "=1";
      } else if (this->tradingMode == TradingMode::PAPER) {
        if (this->appMode == AppMode::MARKET_MAKING) {
          options += std::string(CCAPI_MARKET_DEPTH_MAX) + "=1";
        } else if (this->appMode == AppMode::SINGLE_ORDER_EXECUTION) {
          options += std::string(CCAPI_MARKET_DEPTH_MAX) + "=1000";
        }
      }
      if (!this->enableUpdateOrderBookTickByTick) {
        options += "&" + std::string(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) + "=" + std::to_string(this->clockStepMilliseconds) + "&" +
                   CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS + "=0";
      }
      subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, CCAPI_MARKET_DEPTH, options,
                                    PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID);
    }
    {
      std::string field = CCAPI_TRADE;
      if (this->exchange.rfind("binance", 0) == 0) {
        field = CCAPI_AGG_TRADE;
      }
      subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, field, "", PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID);
    }
    {
      if (this->tradingMode == TradingMode::LIVE) {
        subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, std::string(CCAPI_EM_PRIVATE_TRADE) + "," + CCAPI_EM_ORDER_UPDATE, "",
                                      PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID);
      }
    }
  }
  virtual void postProcessMessageMarketDataEventMarketDepth(const Message& message, const TimePoint& messageTime) {}
  virtual void postProcessMessageMarketDataEventTrade(const Message& message, const TimePoint& messageTime) {
    if (this->enableAdverseSelectionGuard) {
      int intervalStart = UtilTime::getUnixTimestamp(messageTime) / this->adverseSelectionGuardMarketDataSampleIntervalSeconds *
                          this->adverseSelectionGuardMarketDataSampleIntervalSeconds;
      for (auto& kv : this->publicTradeMap) {
        kv.second.erase(kv.second.begin(), kv.second.upper_bound(intervalStart - this->adverseSelectionGuardMarketDataSampleBufferSizeSeconds));
      }
      const auto& elementList = message.getElementList();
      auto rit = elementList.rbegin();
      if (rit != elementList.rend()) {
#if APP_PUBLIC_TRADE_LAST != -1
        this->publicTradeMap[APP_PUBLIC_TRADE_LAST][intervalStart] = std::stod(rit->getValue(CCAPI_LAST_PRICE));
#endif
      }
    }
  }
  virtual void extractInstrumentInfo(const Element& element) {
    this->baseAsset = element.getValue(CCAPI_BASE_ASSET);
    APP_LOGGER_INFO("Base asset is " + this->baseAsset);
    this->quoteAsset = element.getValue(CCAPI_QUOTE_ASSET);
    APP_LOGGER_INFO("Quote asset is " + this->quoteAsset);
    if (this->exchange == "bitfinex") {
      this->orderPriceIncrement = "0.00000001";
    } else {
      this->orderPriceIncrement = Decimal(element.getValue(CCAPI_ORDER_PRICE_INCREMENT)).toString();
    }
    APP_LOGGER_INFO("Order price increment is " + this->orderPriceIncrement);
    if (this->exchange == "bitfinex") {
      this->orderQuantityIncrement = "0.00000001";
    } else {
      this->orderQuantityIncrement = Decimal(element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT)).toString();
    }
    APP_LOGGER_INFO("Order quantity increment is " + this->orderQuantityIncrement);
  }
  virtual void extractBalanceInfo(const Element& element) {
    const auto& asset = element.getValue(CCAPI_EM_ASSET);
    if (asset == this->baseAsset) {
      this->baseBalance = std::stod(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING)) * this->baseAvailableBalanceProportion;
    } else if (asset == this->quoteAsset) {
      this->quoteBalance = std::stod(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING)) * this->quoteAvailableBalanceProportion;
    }
  }
  virtual void updateAccountBalancesByFee(const std::string& feeAsset, double feeQuantity, const std::string& side, bool isMaker) {
    if (feeAsset == this->baseAsset) {
      this->baseBalance -= feeQuantity;
    } else if (feeAsset == this->quoteAsset) {
      this->quoteBalance -= feeQuantity;
    }
  }
  virtual void cancelOpenOrders(const Event& event, Session* session, std::vector<Request>& requestList, const TimePoint& messageTime,
                                const std::string& messageTimeISO, bool alwaysCancel) {
    if (alwaysCancel || this->numOpenOrders != 0) {
      if (this->useCancelOrderToCancelOpenOrders) {
        if (this->openBuyOrder && !this->openBuyOrder.get().orderId.empty()) {
#ifdef CANCEL_BUY_ORDER_REQUEST_CORRELATION_ID
          this->cancelBuyOrderRequestCorrelationId = CANCEL_BUY_ORDER_REQUEST_CORRELATION_ID;
#else
          this->cancelBuyOrderRequestCorrelationId = messageTimeISO + "-CANCEL_BUY_ORDER";
#endif
          Request request(Request::Operation::CANCEL_ORDER, this->exchange, this->instrumentRest, this->cancelBuyOrderRequestCorrelationId);
          request.appendParam({
              {CCAPI_EM_ORDER_ID, this->openBuyOrder.get().orderId},
          });
          request.setTimeSent(messageTime);
          requestList.emplace_back(std::move(request));
        }
        if (this->openSellOrder && !this->openSellOrder.get().orderId.empty()) {
#ifdef CANCEL_SELL_ORDER_REQUEST_CORRELATION_ID
          this->cancelSellOrderRequestCorrelationId = CANCEL_SELL_ORDER_REQUEST_CORRELATION_ID;
#else
          this->cancelSellOrderRequestCorrelationId = messageTimeISO + "-CANCEL_SELL_ORDER";
#endif
          Request request(Request::Operation::CANCEL_ORDER, this->exchange, this->instrumentRest, this->cancelSellOrderRequestCorrelationId);
          if (!this->credential_2.empty()) {
            request.setCredential(this->credential_2);
          }
          request.appendParam({
              {CCAPI_EM_ORDER_ID, this->openSellOrder.get().orderId},
          });
          request.setTimeSent(messageTime);
          requestList.emplace_back(std::move(request));
        }
      } else {
#ifdef CANCEL_OPEN_ORDERS_REQUEST_CORRELATION_ID
        this->cancelOpenOrdersRequestCorrelationId = CANCEL_OPEN_ORDERS_REQUEST_CORRELATION_ID;
#else
        this->cancelOpenOrdersRequestCorrelationId = messageTimeISO + "-CANCEL_OPEN_ORDERS";
#endif
        Request request(Request::Operation::CANCEL_OPEN_ORDERS, this->exchange, this->instrumentRest, this->cancelOpenOrdersRequestCorrelationId);
        if (this->exchange == "huobi") {
          request.appendParam({
              {CCAPI_EM_ACCOUNT_ID, this->accountId},
          });
        }
        request.setTimeSent(messageTime);
        requestList.emplace_back(std::move(request));
      }
      this->numOpenOrders = 0;
      APP_LOGGER_INFO("Cancel open orders.");
    } else {
      this->getAccountBalances(event, session, requestList, messageTime, messageTimeISO);
    }
    this->orderRefreshLastTime = messageTime;
    this->cancelOpenOrdersLastTime = messageTime;
  }
  virtual void getAccountBalances(const Event& event, Session* session, std::vector<Request>& requestList, const TimePoint& messageTime,
                                  const std::string& messageTimeISO) {
#ifdef GET_ACCOUNT_BALANCES_REQUEST_CORRELATION_ID
    this->getAccountBalancesRequestCorrelationId = GET_ACCOUNT_BALANCES_REQUEST_CORRELATION_ID;
#else
    this->getAccountBalancesRequestCorrelationId = messageTimeISO + "-GET_ACCOUNT_BALANCES";
#endif
    Request request(this->useGetAccountsToGetAccountBalances ? Request::Operation::GET_ACCOUNTS : Request::Operation::GET_ACCOUNT_BALANCES, this->exchange, "",
                    this->getAccountBalancesRequestCorrelationId);
    request.setTimeSent(messageTime);
    if (this->exchange == "huobi") {
      request.appendParam({
          {CCAPI_EM_ACCOUNT_ID, this->accountId},
      });
    } else if (this->exchange == "kucoin") {
      request.appendParam({
          {CCAPI_EM_ACCOUNT_TYPE, "trade"},
      });
    }
    requestList.emplace_back(std::move(request));
    this->getAccountBalancesLastTime = messageTime;
    APP_LOGGER_INFO("Get account balances.");
    if (this->appMode == AppMode::MARKET_MAKING) {
      this->onPostGetAccountBalancesMarketMaking(messageTime);
    } else if (this->appMode == AppMode::SINGLE_ORDER_EXECUTION) {
      this->onPostGetAccountBalancesSingleOrderExecution(messageTime);
    }
  }
  virtual void onPostGetAccountBalancesMarketMaking(const TimePoint& now) {
    this->orderRefreshIntervalIndex += 1;
    if (now >= this->startTimeTp + std::chrono::seconds(this->totalDurationSeconds)) {
      APP_LOGGER_INFO("Exit.");
      this->promisePtr->set_value();
      this->skipProcessEvent = true;
    }
  }
  virtual void onPostGetAccountBalancesSingleOrderExecution(const TimePoint& now) {
    this->orderRefreshIntervalIndex += 1;
    if (now >= this->startTimeTp + std::chrono::seconds(this->totalDurationSeconds) ||
        (this->quoteTotalTargetQuantity > 0 ? this->theoreticalQuoteRemainingQuantity <= 0 : this->theoreticalRemainingQuantity <= 0)) {
      APP_LOGGER_INFO("Exit.");
      this->promisePtr->set_value();
      this->skipProcessEvent = true;
    }
  }
  virtual void placeOrders(const Event& event, Session* session, std::vector<Request>& requestList, const TimePoint& now) {
    if (this->midPrice == 0) {
      APP_LOGGER_INFO("At least one side of the order book is empty. Skip.");
      return;
    }
    if (this->baseBalance > 0 || this->quoteBalance > 0) {
      APP_LOGGER_INFO(this->baseAsset + " balance is " + Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString() + ", " + this->quoteAsset +
                      " balance is " + Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString() + ".");
      APP_LOGGER_INFO("Best bid price is " + this->bestBidPrice + ", best bid size is " + this->bestBidSize + ", best ask price is " + this->bestAskPrice +
                      ", best ask size is " + this->bestAskSize + ".");
      if (this->appMode == AppMode::MARKET_MAKING) {
        this->placeOrdersMarketMaking(event, session, requestList, now);
      } else if (this->appMode == AppMode::SINGLE_ORDER_EXECUTION) {
        this->placeOrdersSingleOrderExecution(event, session, requestList, now);
      }
    } else {
      APP_LOGGER_INFO("Account has no assets. Skip.");
    }
  }
  virtual void placeOrdersMarketMaking(const Event& event, Session* session, std::vector<Request>& requestList, const TimePoint& now) {
    double totalBalance = this->baseBalance * this->midPrice + this->quoteBalance;
    if (totalBalance > this->totalBalancePeak) {
      this->totalBalancePeak = totalBalance;
    }
    if ((this->totalBalancePeak - totalBalance) / this->totalBalancePeak > this->killSwitchMaximumDrawdown) {
      APP_LOGGER_INFO("Kill switch triggered - Maximum drawdown. Exit.");
      this->promisePtr->set_value();
      this->skipProcessEvent = true;
      return;
    }
    double r = this->baseBalance * this->midPrice / totalBalance;
    APP_LOGGER_DEBUG("Base balance proportion is " + std::to_string(r) + ".");
    AdverseSelectionGuardInformedTraderSide adverseSelectionGuardInformedTraderSide{AdverseSelectionGuardInformedTraderSide::NONE};
    if (this->enableAdverseSelectionGuard) {
      if (this->enableAdverseSelectionGuardByRollCorrelationCoefficient) {
        this->checkAdverseSelectionGuardByRollCorrelationCoefficient(adverseSelectionGuardInformedTraderSide);
      }
      if (this->enableAdverseSelectionGuardByRoc) {
        this->checkAdverseSelectionGuardByRoc(adverseSelectionGuardInformedTraderSide);
      }
      if (this->enableAdverseSelectionGuardByRsi) {
        this->checkAdverseSelectionGuardByRsi(adverseSelectionGuardInformedTraderSide);
      }
    }
    if (this->enableMarketMaking && adverseSelectionGuardInformedTraderSide == AdverseSelectionGuardInformedTraderSide::NONE) {
      if (this->enableAdverseSelectionGuard && r < this->adverseSelectionGuardTriggerInventoryBasePortionMinimum &&
          this->enableAdverseSelectionGuardByInventoryLimit) {
        adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
      } else if (this->enableAdverseSelectionGuard && r > this->adverseSelectionGuardTriggerInventoryBasePortionMaximum &&
                 this->enableAdverseSelectionGuardByInventoryLimit) {
        adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
      } else {
        std::string orderQuantity =
            AppUtil::roundInput((this->quoteBalance / this->midPrice + this->baseBalance) * this->orderQuantityProportion, this->orderQuantityIncrement, false);
        if (r < this->inventoryBasePortionTarget) {
          std::string buyPrice;
          if (this->enableAdverseSelectionGuard &&
              (this->enableAdverseSelectionGuardByInventoryLimit || this->enableAdverseSelectionGuardByInventoryDepletion) &&
              (this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::MAKE ||
               this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE)) {
            double halfSpread =
                -AppUtil::linearInterpolate(this->inventoryBasePortionTarget, -this->halfSpreadMinimum,
                                            this->enableAdverseSelectionGuardByInventoryLimit ? this->adverseSelectionGuardTriggerInventoryBasePortionMinimum
                                                                                              : this->orderQuantityProportion,
                                            this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE
                                                ? (std::stod(this->bestAskPrice) - this->midPrice) / this->midPrice
                                                : (std::stod(this->bestAskPrice) - std::stod(this->orderPriceIncrement) - this->midPrice) / this->midPrice,
                                            r);
            buyPrice = AppUtil::roundInput(this->midPrice * (1 - halfSpread), this->orderPriceIncrement, false);
          } else {
            buyPrice = AppUtil::roundInput(this->midPrice * (1 - this->halfSpreadMinimum), this->orderPriceIncrement, false);
          }
          if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          } else {
            APP_LOGGER_INFO("Insufficient quote balance.");
            if (this->enableAdverseSelectionGuard && this->enableAdverseSelectionGuardByInventoryDepletion) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
            }
          }
          std::string sellPrice = AppUtil::roundInput(
              this->midPrice * (1 + AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 0, this->halfSpreadMaximum, r)),
              this->orderPriceIncrement, true);
          if (std::stod(orderQuantity) <= this->baseBalance) {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          } else {
            APP_LOGGER_INFO("Insufficient base balance.");
            if (this->enableAdverseSelectionGuard && this->enableAdverseSelectionGuardByInventoryDepletion) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
            }
          }
        } else {
          std::string buyPrice = AppUtil::roundInput(
              this->midPrice * (1 - AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 1, this->halfSpreadMaximum, r)),
              this->orderPriceIncrement, false);
          if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          } else {
            APP_LOGGER_INFO("Insufficient quote balance.");
            if (this->enableAdverseSelectionGuard && this->enableAdverseSelectionGuardByInventoryDepletion) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
            }
          }
          std::string sellPrice;
          if (this->enableAdverseSelectionGuard &&
              (this->enableAdverseSelectionGuardByInventoryLimit || this->enableAdverseSelectionGuardByInventoryDepletion) &&
              (this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::MAKE ||
               this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE)) {
            double halfSpread =
                AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum,
                                           this->enableAdverseSelectionGuardByInventoryLimit ? this->adverseSelectionGuardTriggerInventoryBasePortionMaximum
                                                                                             : 1 - this->orderQuantityProportion,
                                           this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE
                                               ? (std::stod(this->bestBidPrice) - this->midPrice) / this->midPrice
                                               : (std::stod(this->bestBidPrice) + std::stod(this->orderPriceIncrement) - this->midPrice) / this->midPrice,
                                           r);
            sellPrice = AppUtil::roundInput(this->midPrice * (1 + halfSpread), this->orderPriceIncrement, true);
          } else {
            sellPrice = AppUtil::roundInput(this->midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
          }
          if (std::stod(orderQuantity) <= this->baseBalance) {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          } else {
            APP_LOGGER_INFO("Insufficient base balance.");
            if (this->enableAdverseSelectionGuard && this->enableAdverseSelectionGuardByInventoryDepletion) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
            }
          }
        }
      }
    }
    if (this->enableAdverseSelectionGuard && adverseSelectionGuardInformedTraderSide != AdverseSelectionGuardInformedTraderSide::NONE) {
      APP_LOGGER_INFO("Adverse selection guard was triggered.");
      if (this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::MAKE ||
          this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE) {
        this->orderRefreshIntervalSeconds = this->adverseSelectionGuardActionOrderRefreshIntervalSeconds;
        requestList.clear();
        if (adverseSelectionGuardInformedTraderSide == AdverseSelectionGuardInformedTraderSide::BUY) {
          std::string buyPrice = this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE
                                     ? this->bestAskPrice
                                     : Decimal(this->bestAskPrice).subtract(Decimal(this->orderPriceIncrement)).toString();
          double conversionPrice = std::stod(buyPrice);
          std::string orderQuantity = this->adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset
                                          ? AppUtil::roundInput(this->quoteBalance / conversionPrice * this->adverseSelectionGuardActionOrderQuantityProportion,
                                                                this->orderQuantityIncrement, false)
                                          : AppUtil::roundInput(std::min((this->quoteBalance / conversionPrice + this->baseBalance) *
                                                                             this->adverseSelectionGuardActionOrderQuantityProportion,
                                                                         this->quoteBalance / conversionPrice),
                                                                this->orderQuantityIncrement, false);
          if (UtilString::normalizeDecimalString(orderQuantity) != "0") {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_BUY, buyPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          }
        } else {
          std::string sellPrice = this->adverseSelectionGuardActionType == AdverseSelectionGuardActionType::TAKE
                                      ? this->bestBidPrice
                                      : Decimal(this->bestBidPrice).add(Decimal(this->orderPriceIncrement)).toString();
          double conversionPrice = std::stod(sellPrice);
          std::string orderQuantity =
              this->adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset
                  ? AppUtil::roundInput(this->baseBalance * this->adverseSelectionGuardActionOrderQuantityProportion, this->orderQuantityIncrement, false)
                  : AppUtil::roundInput(
                        std::min((this->quoteBalance / conversionPrice + this->baseBalance) * this->adverseSelectionGuardActionOrderQuantityProportion,
                                 this->baseBalance),
                        this->orderQuantityIncrement, false);
          if (UtilString::normalizeDecimalString(orderQuantity) != "0") {
            Request request = this->createRequestForCreateOrder(CCAPI_EM_ORDER_SIDE_SELL, sellPrice, orderQuantity, now);
            requestList.emplace_back(std::move(request));
          }
        }
      }
    } else {
      this->orderRefreshIntervalSeconds = this->originalOrderRefreshIntervalSeconds;
    }
  }
  virtual void placeOrdersSingleOrderExecution(const Event& event, Session* session, std::vector<Request>& requestList, const TimePoint& now) {
    double price = 0;
    if (this->orderSide == CCAPI_EM_ORDER_SIDE_BUY) {
      price = std::min(midPrice * (1 + this->orderPriceLimitRelativeToMidPrice), this->orderPriceLimit == 0 ? INT_MAX : this->orderPriceLimit);
    } else {
      price = std::max(midPrice * (1 + this->orderPriceLimitRelativeToMidPrice), this->orderPriceLimit);
    }
    std::string priceStr = AppUtil::roundInput(price, this->orderPriceIncrement, this->orderSide == CCAPI_EM_ORDER_SIDE_SELL);
    double quantity = 0;
    int intervalStart = std::chrono::duration_cast<std::chrono::seconds>(this->startTimeTp.time_since_epoch()).count() +
                        (this->orderRefreshIntervalIndex - 1) * this->orderRefreshIntervalSeconds;
    int intervalEnd = intervalStart + this->orderRefreshIntervalSeconds;
    if (tradingStrategy == TradingStrategy::TWAP) {
      double randomization = AppUtil::generateRandomDouble(-this->twapOrderQuantityRandomizationMax, this->twapOrderQuantityRandomizationMax);
      quantity =
          (1 + randomization) * (this->quoteTotalTargetQuantity > 0 ? this->quoteTotalTargetQuantity / this->numOrderRefreshIntervals / std::stod(priceStr)
                                                                    : this->totalTargetQuantity / this->numOrderRefreshIntervals);
    }
    // else if (tradingStrategy==TradingStrategy::VWAP){
    //   if (this->orderRefreshIntervalIndex == 0) {
    //     quantity = this->quoteTotalTargetQuantity > 0 ? this->quoteTotalTargetQuantity / this->numOrderRefreshIntervals/price :
    //     this->totalTargetQuantity / this->numOrderRefreshIntervals;
    //   } else {
    //     double projectedPublicTradeVolume = 0;
    //     double projectedPublicTradeVolumeInQuote = 0;
    //     double projectedPublicTradeVwap = 0;
    //     for (const auto& kv:this->publicTradeMap){
    //       if (kv.first>=intervalStart
    //     && kv.first<intervalEnd){
    //         projectedPublicTradeVolume+=2*kv.second[APP_PUBLIC_TRADE_VOLUME];
    //         projectedPublicTradeVolumeInQuote+=2*kv.second[APP_PUBLIC_TRADE_VOLUME_IN_QUOTE];
    //       } else {
    //         projectedPublicTradeVolume+=kv.second[APP_PUBLIC_TRADE_VOLUME];
    //         projectedPublicTradeVolumeInQuote+=kv.second[APP_PUBLIC_TRADE_VOLUME_IN_QUOTE];
    //       }
    //     }
    //     if (projectedPublicTradeVolume>0){
    //       projectedPublicTradeVwap=projectedPublicTradeVolumeInQuote/projectedPublicTradeVolume;
    //       if (price!=projectedPublicTradeVwap){
    //         quantity=(projectedPublicTradeVwap*this->privateTradeSummary[APP_PUBLIC_TRADE_VOLUME]-this->privateTradeSummary[APP_PUBLIC_TRADE_VOLUME_IN_QUOTE])/(price-projectedPublicTradeVwap);
    //       }
    //     }
    //   }
    // }else if (tradingStrategy==TradingStrategy::POV){
    //   if (this->orderRefreshIntervalIndex == 0) {
    //     quantity = this->quoteTotalTargetQuantity > 0 ? this->quoteTotalTargetQuantity / this->numOrderRefreshIntervals/price :
    //     this->totalTargetQuantity / this->numOrderRefreshIntervals;
    //   } else {
    //     auto itLowerBound = this->publicTradeMap.lower_bound(intervalStart);
    //     auto itUpperBound = this->publicTradeMap.lower_bound(intervalEnd);
    //     double projectedPublicTradeVolume = 0;
    //     double projectedPublicTradeVolumeInQuote = 0;
    //     while (itLowerBound!=itUpperBound){
    //       projectedPublicTradeVolume+=*itLowerBound[APP_PUBLIC_TRADE_VOLUME];
    //       projectedPublicTradeVolumeInQuote+=*itLowerBound[APP_PUBLIC_TRADE_VOLUME_IN_QUOTE];
    //       itLowerBound++;
    //     }
    //     quantity = this->quoteTotalTargetQuantity >
    //     0?projectedPublicTradeVolumeInQuote*this->povOrderQuantityParticipationRate/price:projectedPublicTradeVolume*this->povOrderQuantityParticipationRate;
    //   }
    //
    //   // quantity =
    // }else if (tradingStrategy==TradingStrategy::IS){
    //   if (this->orderRefreshIntervalIndex > 0) {
    //     quantity =
    //     2*std::sinh(0.5*this->isKapa*this->orderRefreshIntervalSeconds)/std::sinh(this->isKapa*this->totalDurationSeconds)*std::cosh(this->isKapa*(this->totalDurationSeconds-(this->orderRefreshIntervalIndex-0.5)*this->orderRefreshIntervalSeconds))*(this->quoteTotalTargetQuantity?this->quoteTotalTargetQuantity/price:this->totalTargetQuantity);
    //   }
    //
    // }
    quantity = std::min({
        quantity,
        this->orderSide == CCAPI_EM_ORDER_SIDE_BUY ? this->quoteBalance / std::stod(priceStr) : this->baseBalance,
        this->quoteTotalTargetQuantity > 0 ? this->theoreticalQuoteRemainingQuantity / std::stod(priceStr) : this->theoreticalRemainingQuantity,
    });
    if (this->orderQuantityLimitRelativeToTarget > 0) {
      quantity = std::min({
          quantity,
          this->quoteTotalTargetQuantity > 0 ? this->quoteTotalTargetQuantity * this->orderQuantityLimitRelativeToTarget / std::stod(priceStr)
                                             : this->totalTargetQuantity * this->orderQuantityLimitRelativeToTarget,
      });
    }
    if (quantity > 0) {
      std::string quantityStr = AppUtil::roundInput(quantity, this->orderQuantityIncrement, false);
      if (UtilString::normalizeDecimalString(quantityStr) != "0") {
        if (this->quoteTotalTargetQuantity > 0) {
          this->theoreticalQuoteRemainingQuantity -= price * quantity;
          if (this->theoreticalQuoteRemainingQuantity >= 0) {
            Request request = this->createRequestForCreateOrder(this->orderSide, priceStr, quantityStr, now);
            requestList.emplace_back(std::move(request));
          }
        } else {
          this->theoreticalRemainingQuantity -= quantity;
          if (this->theoreticalRemainingQuantity >= 0) {
            Request request = this->createRequestForCreateOrder(this->orderSide, priceStr, quantityStr, now);
            requestList.emplace_back(std::move(request));
          }
        }
      }
    }
  }
  virtual Request createRequestForCreateOrder(const std::string& side, const std::string& price, const std::string& quantity, const TimePoint& now) {
    const auto& messageTimeISO = UtilTime::getISOTimestamp<std::chrono::milliseconds>(std::chrono::time_point_cast<std::chrono::milliseconds>(now));
    Request request(Request::Operation::CREATE_ORDER, this->exchange, this->instrumentRest, "CREATE_ORDER_" + side);
    if (!this->credential_2.empty() && side == CCAPI_EM_ORDER_SIDE_SELL) {
      request.setCredential(this->credential_2);
    }
    std::map<std::string, std::string> param = {
        {CCAPI_EM_ORDER_SIDE, side},
        {CCAPI_EM_ORDER_QUANTITY, quantity},
        {CCAPI_EM_ORDER_LIMIT_PRICE, price},
    };
    if (this->exchange == "kucoin") {
      param.insert({CCAPI_EM_CLIENT_ORDER_ID, AppUtil::generateUuidV4()});
    } else {
      if (this->tradingMode == TradingMode::BACKTEST || this->tradingMode == TradingMode::PAPER) {
        std::string clientOrderId;
        clientOrderId += messageTimeISO;
        clientOrderId += "_";
        clientOrderId += side;
        param.insert({CCAPI_EM_CLIENT_ORDER_ID, clientOrderId});
      }
    }
    if (this->exchange == "huobi") {
      param.insert({CCAPI_EM_ACCOUNT_ID, this->accountId});
    }
    request.appendParam(param);
    request.setTimeSent(now);
    APP_LOGGER_INFO("Place order - side: " + side + ", price: " + price + ", quantity: " + quantity + ".");
    return request;
  }
  virtual void extractOrderInfo(Element& element, const Order& order) {
    element.insert(CCAPI_EM_ORDER_ID, order.orderId);
    element.insert(CCAPI_EM_CLIENT_ORDER_ID, order.clientOrderId);
    element.insert(CCAPI_EM_ORDER_SIDE, order.side);
    element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, order.limitPrice.toString());
    element.insert(CCAPI_EM_ORDER_QUANTITY, order.quantity.toString());
    element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, order.cumulativeFilledQuantity.toString());
    element.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, order.remainingQuantity.toString());
    element.insert(CCAPI_EM_ORDER_STATUS, order.status);
  }
  virtual void checkAdverseSelectionGuardByRollCorrelationCoefficient(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap[APP_PUBLIC_TRADE_LAST].size() >= this->adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations) {
      int size = this->adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations - 1;
      double deltaPt[size];
      double deltaPtPlusOne[size];
      auto rit = this->publicTradeMap[APP_PUBLIC_TRADE_LAST].rbegin();
      std::advance(rit, size);
      int i = 0;
      double previousP = rit->second;
      while (i < size) {
        rit--;
        deltaPt[i] = (rit->second - previousP) / previousP;
        previousP = rit->second;
        i++;
      }
      std::copy(deltaPt, deltaPt + size, deltaPtPlusOne);
      double deltaPtBar = std::accumulate(deltaPt, deltaPt + size - 1, 0.0) / (size - 1);
      double deltaPtPlusOneBar = std::accumulate(deltaPt + 1, deltaPt + size, 0.0) / (size - 1);
      double deltaPtMinusDeltaPtBar[size - 1];
      double deltaPtPlusOneMinusDeltaPtPlusOneBar[size - 1];
      std::transform(deltaPt, deltaPt + size - 1, deltaPtMinusDeltaPtBar, [deltaPtBar](double a) -> double { return a - deltaPtBar; });
      std::transform(deltaPtPlusOne + 1, deltaPtPlusOne + size, deltaPtPlusOneMinusDeltaPtPlusOneBar,
                     [deltaPtPlusOneBar](double a) -> double { return a - deltaPtPlusOneBar; });
      double denominator = std::sqrt(std::accumulate(deltaPtMinusDeltaPtBar, deltaPtMinusDeltaPtBar + size - 1, 0.0,
                                                     [](double accu, double elem) -> double { return accu + std::pow(elem, 2); }) *
                                     std::accumulate(deltaPtPlusOneMinusDeltaPtPlusOneBar, deltaPtPlusOneMinusDeltaPtPlusOneBar + size - 1, 0.0,
                                                     [](double accu, double elem) -> double { return accu + std::pow(elem, 2); }));
      if (denominator > 0) {
        double numerator = std::inner_product(deltaPtMinusDeltaPtBar, deltaPtMinusDeltaPtBar + size - 1, deltaPtPlusOneMinusDeltaPtPlusOneBar, 0.0);
        double r = numerator / denominator;
        APP_LOGGER_DEBUG("Roll coefficient is " + std::to_string(r) + ".");
        if (r > this->adverseSelectionGuardTriggerRollCorrelationCoefficientMaximum) {
          if (deltaPt[size - 1] - deltaPt[0] > 0) {
            if (this->adverseSelectionGuardTriggerRollCorrelationCoefficientOrderDirectionReverse) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
            } else {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
            }
          } else {
            if (this->adverseSelectionGuardTriggerRollCorrelationCoefficientOrderDirectionReverse) {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
            } else {
              adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
            }
          }
        }
      }
    }
  }
  virtual void checkAdverseSelectionGuardByRoc(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap[APP_PUBLIC_TRADE_LAST].size() >= this->adverseSelectionGuardTriggerRocNumObservations) {
      int size = this->adverseSelectionGuardTriggerRocNumObservations - 1;
      auto rit2 = this->publicTradeMap[APP_PUBLIC_TRADE_LAST].rbegin();
      auto rit = this->publicTradeMap[APP_PUBLIC_TRADE_LAST].rbegin();
      std::advance(rit, size);
      double roc = (rit2->second - rit->second) / rit->second * 100;
      APP_LOGGER_DEBUG("ROC is " + std::to_string(roc) + ".");
      if (roc > this->adverseSelectionGuardTriggerRocMaximum) {
        if (this->adverseSelectionGuardTriggerRocOrderDirectionReverse) {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
        } else {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
        }
      } else if (roc < this->adverseSelectionGuardTriggerRocMinimum) {
        if (this->adverseSelectionGuardTriggerRocOrderDirectionReverse) {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
        } else {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
        }
      }
    }
  }
  virtual void checkAdverseSelectionGuardByRsi(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap[APP_PUBLIC_TRADE_LAST].size() >= this->adverseSelectionGuardTriggerRsiNumObservations) {
      int size = this->adverseSelectionGuardTriggerRsiNumObservations - 1;
      double deltaPt[size];
      auto rit = this->publicTradeMap[APP_PUBLIC_TRADE_LAST].rbegin();
      std::advance(rit, size);
      int i = 0;
      double previousP = rit->second;
      while (i < size) {
        rit--;
        deltaPt[i] = (rit->second - previousP) / previousP;
        previousP = rit->second;
        i++;
      }
      double sumGain = 0;
      int countGain = 0;
      double sumLoss = 0;
      int countLoss = 0;
      for (const auto& x : deltaPt) {
        if (x > 0) {
          sumGain += x;
          countGain++;
        } else if (x < 0) {
          sumLoss += (-x);
          countLoss++;
        }
      }
      double rsi;
      if (countGain == 0 && countLoss == 0) {
        rsi = 50;
      } else if (countGain == 0 && countLoss > 0) {
        rsi = 0;
      } else if (countGain > 0 && countLoss == 0) {
        rsi = 100;
      } else {
        rsi = 100 - 100 / (1 + sumGain / sumLoss);
      }
      APP_LOGGER_DEBUG("RSI is " + std::to_string(rsi) + ".");
      if (rsi > this->adverseSelectionGuardTriggerRsiMaximum) {
        if (this->adverseSelectionGuardTriggerRsiOrderDirectionReverse) {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
        } else {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
        }
      } else if (rsi < this->adverseSelectionGuardTriggerRsiMinimum) {
        if (this->adverseSelectionGuardTriggerRsiOrderDirectionReverse) {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::BUY;
        } else {
          adverseSelectionGuardInformedTraderSide = AdverseSelectionGuardInformedTraderSide::SELL;
        }
      }
    }
  }
  CsvWriter* privateTradeCsvWriter = nullptr;
  CsvWriter* orderUpdateCsvWriter = nullptr;
  CsvWriter* accountBalanceCsvWriter = nullptr;
  int64_t virtualTradeId{}, virtualOrderId{};
  std::map<int, std::map<int, double>> publicTradeMap;
  std::map<Decimal, std::string> snapshotBid, snapshotAsk;
  bool skipProcessEvent{};
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_EVENT_HANDLER_BASE_H_
