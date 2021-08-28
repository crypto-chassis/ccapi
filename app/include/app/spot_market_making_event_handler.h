#ifndef APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#define APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#include <random>
#include <sstream>
#include "app/common.h"
#include "app/historical_market_data_event_processor.h"
#include "app/order.h"
#include "boost/optional/optional.hpp"
#include "ccapi_cpp/ccapi_session.h"
// #include "boost/uuid/uuid.hpp"
// #include "boost/uuid/uuid_generators.hpp"
// #include "boost/uuid/uuid_io.hpp"
// #include <filesystem>

namespace ccapi {
class SpotMarketMakingEventHandler : public EventHandler {
 public:
  enum class TradingMode {
    LIVE,
    PAPER,
    BACKTEST,
  };
  SpotMarketMakingEventHandler(AppLogger* appLogger, CsvWriter* privateTradeCsvWriter = nullptr, CsvWriter* orderUpdateCsvWriter = nullptr,
                               CsvWriter* accountBalanceCsvWriter = nullptr)
      : EventHandler(),
        appLogger(appLogger),
        privateTradeCsvWriter(privateTradeCsvWriter),
        orderUpdateCsvWriter(orderUpdateCsvWriter),
        accountBalanceCsvWriter(accountBalanceCsvWriter) {
    this->totalBalancePeak = 0;
  }
  bool processEvent(const Event& event, Session* session) override {
    this->appLogger->logDebug("********", this->printDebug);
    this->appLogger->logDebug("Received an event: " + event.toStringPretty(), this->printDebug);
    if (this->openBuyOrder) {
      this->appLogger->logDebug("Open buy order is " + this->openBuyOrder->toString() + ".", this->printDebug);
    }
    if (this->openSellOrder) {
      this->appLogger->logDebug("Open sell order is " + this->openSellOrder->toString() + ".", this->printDebug);
    }
    std::string baseBalanceDecimalNotation = Decimal(AppUtil::printDoubleScientific(this->baseBalance)).toString();
    std::string quoteBalanceDecimalNotation = Decimal(AppUtil::printDoubleScientific(this->quoteBalance)).toString();
    this->appLogger->logDebug("Base asset balance is " + baseBalanceDecimalNotation + ", quote asset balance is " + quoteBalanceDecimalNotation + ".",
                              this->printDebug);
    auto eventType = event.getType();
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
        } else if (message.getType() == Message::Type::MARKET_DATA_EVENTS_TRADE || message.getType() == Message::Type::MARKET_DATA_EVENTS_AGG_TRADE) {
          if (this->tradingMode == TradingMode::PAPER || this->tradingMode == TradingMode::BACKTEST) {
            auto messageTime = message.getTime();
            for (const auto& element : message.getElementList()) {
              bool isBuyerMaker = element.getValue(CCAPI_IS_BUYER_MAKER) == "1";
              auto takerPrice = Decimal(element.getValue(CCAPI_LAST_PRICE));
              Order order;
              if (isBuyerMaker && this->openBuyOrder) {
                order = this->openBuyOrder.get();
              } else if (!isBuyerMaker && this->openSellOrder) {
                order = this->openSellOrder.get();
              }
              if ((isBuyerMaker && this->openBuyOrder && takerPrice <= this->openBuyOrder.get().limitPrice) ||
                  (!isBuyerMaker && this->openSellOrder && takerPrice >= this->openSellOrder.get().limitPrice)) {
                auto takerQuantity = Decimal(element.getValue(CCAPI_LAST_SIZE));
                Decimal lastFilledQuantity;
                if (takerQuantity < order.remainingQuantity) {
                  lastFilledQuantity = takerQuantity;
                  order.cumulativeFilledQuantity = order.cumulativeFilledQuantity.add(lastFilledQuantity);
                  order.remainingQuantity = order.remainingQuantity.subtract(lastFilledQuantity);
                  order.status = "PARTIALLY_FILLED";
                  if (isBuyerMaker) {
                    this->openBuyOrder = order;
                  } else {
                    this->openSellOrder = order;
                  }
                } else {
                  lastFilledQuantity = order.remainingQuantity;
                  order.cumulativeFilledQuantity = order.quantity;
                  order.remainingQuantity = Decimal("0");
                  order.status = "FILLED";
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
                messagePrivateTrade.setCorrelationIdList({this->privateSubscriptionDataCorrelationId});
                Element elementPrivateTrade;
                elementPrivateTrade.insert("TRADE_ID", std::to_string(++this->virtualTradeId));
                elementPrivateTrade.insert("LAST_EXECUTED_PRICE", order.limitPrice.toString());
                elementPrivateTrade.insert("LAST_EXECUTED_SIZE", lastFilledQuantity.toString());
                elementPrivateTrade.insert("SIDE", order.side);
                elementPrivateTrade.insert("IS_MAKER", "1");
                elementPrivateTrade.insert("ORDER_ID", order.orderId);
                elementPrivateTrade.insert("CLIENT_ORDER_ID", order.clientOrderId);
                elementPrivateTrade.insert("FEE_QUANTITY", Decimal(AppUtil::printDoubleScientific(feeQuantity)).toString());
                elementPrivateTrade.insert("FEE_ASSET", isBuyerMaker ? this->makerBuyerFeeAsset : this->makerSellerFeeAsset);
                messagePrivateTrade.setElementList({elementPrivateTrade});
                Message messageOrderUpdate;
                messageOrderUpdate.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
                messageOrderUpdate.setTime(messageTime);
                messageOrderUpdate.setTimeReceived(messageTime);
                messageOrderUpdate.setCorrelationIdList({this->privateSubscriptionDataCorrelationId});
                Element elementOrderUpdate;
                elementOrderUpdate.insert("ORDER_ID", order.orderId);
                elementOrderUpdate.insert("CLIENT_ORDER_ID", order.clientOrderId);
                elementOrderUpdate.insert("SIDE", order.side);
                elementOrderUpdate.insert("LIMIT_PRICE", order.limitPrice.toString());
                elementOrderUpdate.insert("QUANTITY", order.quantity.toString());
                elementOrderUpdate.insert("CUMULATIVE_FILLED_QUANTITY", order.cumulativeFilledQuantity.toString());
                elementOrderUpdate.insert("REMAINING_QUANTITY", order.remainingQuantity.toString());
                elementOrderUpdate.insert("STATUS", order.status);
                messageOrderUpdate.setElementList({elementOrderUpdate});
                virtualEvent.setMessageList({messagePrivateTrade, messageOrderUpdate});
                this->appLogger->logDebug("Generated a virtual event: " + virtualEvent.toStringPretty(), this->printDebug);
                this->processEvent(virtualEvent, session);
              }
            }
          }
        }
      }
      if (index != -1) {
        auto message = messageList.at(index);
        TimePoint messageTime{std::chrono::seconds{0}};
        messageTime = message.getTime();
        for (const auto& element : message.getElementList()) {
          if (element.has(CCAPI_BEST_BID_N_PRICE)) {
            bestBidPrice = element.getValue(CCAPI_BEST_BID_N_PRICE);
          }
          if (element.has(CCAPI_BEST_ASK_N_PRICE)) {
            bestAskPrice = element.getValue(CCAPI_BEST_ASK_N_PRICE);
          }
          if (element.has(CCAPI_BEST_BID_N_SIZE)) {
            bestBidSize = element.getValue(CCAPI_BEST_BID_N_SIZE);
          }
          if (element.has(CCAPI_BEST_ASK_N_SIZE)) {
            bestAskSize = element.getValue(CCAPI_BEST_ASK_N_SIZE);
          }
        }
        std::string messageTimeISO = UtilTime::getISOTimestamp(messageTime);
        std::string messageTimeISODate = messageTimeISO.substr(0, 10);
        if (this->previousMessageTimeISODate.empty() || messageTimeISODate != previousMessageTimeISODate) {
          std::string prefix;
          if (!this->privateDataFilePrefix.empty()) {
            prefix = this->privateDataFilePrefix + "__";
          }
          std::string privateTradeCsvFilename(prefix + "private_trade__" + this->exchange + "__" + this->instrumentRest + "__" + messageTimeISODate + ".csv"),
              orderUpdateCsvFilename(prefix + "order_update__" + this->exchange + "__" + this->instrumentRest + "__" + messageTimeISODate + ".csv"),
              accountBalanceCsvFilename(prefix + "account_balance__" + this->exchange + "__" + this->instrumentRest + "__" + messageTimeISODate + ".csv");
          if (!this->privateDataDirectory.empty()) {
            // std::filesystem::create_directory(std::filesystem::path(this->privateDataDirectory.c_str()));
            privateTradeCsvFilename = this->privateDataDirectory + "/" + privateTradeCsvFilename;
            orderUpdateCsvFilename = this->privateDataDirectory + "/" + orderUpdateCsvFilename;
            accountBalanceCsvFilename = this->privateDataDirectory + "/" + accountBalanceCsvFilename;
          }
          auto privateTradeCsvWriter = new CsvWriter(privateTradeCsvFilename);
          auto orderUpdateCsvWriter = new CsvWriter(orderUpdateCsvFilename);
          auto accountBalanceCsvWriter = new CsvWriter(accountBalanceCsvFilename);
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
          accountBalanceCsvWriter->writeRow({
              "TIME",
              "BASE_AVAILABLE_BALANCE",
              "QUOTE_AVAILABLE_BALANCE",
              "BEST_BID_PRICE",
              "BEST_ASK_PRICE",
          });
          accountBalanceCsvWriter->flush();
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
          this->cancelOpenOrdersRequestCorrelationId = messageTimeISO + "-CANCEL_OPEN_ORDERS";
          Request request(Request::Operation::CANCEL_OPEN_ORDERS, this->exchange, this->instrumentRest, this->cancelOpenOrdersRequestCorrelationId);
          request.setTimeSent(messageTime);
          requestList.push_back(request);
          this->orderRefreshLastTime = messageTime;
          this->cancelOpenOrdersLastTime = messageTime;
          this->appLogger->log("Cancel open orders.");
        } else if (std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->cancelOpenOrdersLastTime).count() >=
                       this->accountBalanceRefreshWaitSeconds &&
                   this->getAccountBalancesLastTime < this->cancelOpenOrdersLastTime &&
                   this->cancelOpenOrdersLastTime + std::chrono::seconds(this->accountBalanceRefreshWaitSeconds) > this->orderRefreshLastTime) {
          this->getAccountBalancesRequestCorrelationId = messageTimeISO + "-GET_ACCOUNT_BALANCES";
          Request request(this->useGetAccountsToGetAccountBalances ? Request::Operation::GET_ACCOUNTS : Request::Operation::GET_ACCOUNT_BALANCES,
                          this->exchange, "", this->getAccountBalancesRequestCorrelationId);
          request.setTimeSent(messageTime);
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
        if (this->tradingMode == TradingMode::LIVE) {
          for (const auto& element : firstMessage.getElementList()) {
            auto asset = element.getValue("ASSET");
            if (asset == this->baseAsset) {
              this->baseBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->baseAvailableBalanceProportion;
            } else if (asset == this->quoteAsset) {
              this->quoteBalance = std::stod(element.getValue("QUANTITY_AVAILABLE_FOR_TRADING")) * this->quoteAvailableBalanceProportion;
            }
          }
        }
        std::string baseBalanceDecimalNotation = Decimal(AppUtil::printDoubleScientific(this->baseBalance)).toString();
        std::string quoteBalanceDecimalNotation = Decimal(AppUtil::printDoubleScientific(this->quoteBalance)).toString();
        this->accountBalanceCsvWriter->writeRow({
            messageTimeReceivedISO,
            baseBalanceDecimalNotation,
            quoteBalanceDecimalNotation,
            this->bestBidPrice,
            this->bestAskPrice,
        });
        this->accountBalanceCsvWriter->flush();
        this->appLogger->log(this->baseAsset + " balance is " + baseBalanceDecimalNotation + ", " + this->quoteAsset + " balance is " +
                             quoteBalanceDecimalNotation + ".");
        this->appLogger->log("Best bid price is " + this->bestBidPrice + ", best ask price is " + this->bestAskPrice + ".");
        this->placeOrders(requestList, messageTimeReceived);
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), "GET_INSTRUMENT") != correlationIdList.end()) {
        auto element = firstMessage.getElementList().at(0);
        this->baseAsset = element.getValue("BASE_ASSET");
        this->quoteAsset = element.getValue("QUOTE_ASSET");
        this->orderPriceIncrement = UtilString::normalizeDecimalString(element.getValue("PRICE_INCREMENT"));
        this->orderQuantityIncrement = UtilString::normalizeDecimalString(element.getValue("QUANTITY_INCREMENT"));
        if (this->tradingMode == TradingMode::BACKTEST) {
          HistoricalMarketDataEventProcessor historicalMarketDataEventProcessor(
              this->appLogger, std::bind(&SpotMarketMakingEventHandler::processEvent, this, std::placeholders::_1, nullptr));
          historicalMarketDataEventProcessor.exchange = this->exchange;
          historicalMarketDataEventProcessor.baseAsset = UtilString::toLower(this->baseAsset);
          historicalMarketDataEventProcessor.quoteAsset = UtilString::toLower(this->quoteAsset);
          historicalMarketDataEventProcessor.startDateTp = this->startDateTp;
          historicalMarketDataEventProcessor.endDateTp = this->endDateTp;
          historicalMarketDataEventProcessor.historicalMarketDataDirectory = this->historicalMarketDataDirectory;
          historicalMarketDataEventProcessor.printDebug = this->printDebug;
          historicalMarketDataEventProcessor.clockStepSeconds = this->clockStepSeconds;
          historicalMarketDataEventProcessor.processEvent();
          this->promisePtr->set_value();
        } else {
          std::vector<Subscription> subscriptionList;
          subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, "MARKET_DEPTH",
                                        std::string(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) + "=" + std::to_string(this->clockStepSeconds * 1000) + "&" +
                                            CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS + "=0",
                                        PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID);
          if (this->tradingMode == TradingMode::PAPER) {
            std::string field = "TRADE";
            if (this->exchange.rfind("binance", 0) == 0) {
              field = "AGG_TRADE";
            }
            subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, field, "", PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID);
          } else {
            subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, "PRIVATE_TRADE,ORDER_UPDATE", "",
                                          this->privateSubscriptionDataCorrelationId);
          }
          session->subscribe(subscriptionList);
        }
      }
    }
    if (!requestList.empty()) {
      if (this->tradingMode == TradingMode::PAPER || this->tradingMode == TradingMode::BACKTEST) {
        for (const auto& request : requestList) {
          auto now = request.getTimeSent();
          Event virtualEvent;
          Event virtualEvent_2;
          Message message;
          Message message_2;
          message.setTime(now);
          message.setTimeReceived(now);
          message.setCorrelationIdList({request.getCorrelationId()});
          message_2.setTime(now);
          message_2.setTimeReceived(now);
          message_2.setCorrelationIdList({request.getCorrelationId()});
          std::vector<Element> elementList;
          auto operation = request.getOperation();
          if (operation == Request::Operation::GET_ACCOUNT_BALANCES || operation == Request::Operation::GET_ACCOUNTS) {
            virtualEvent.setType(Event::Type::RESPONSE);
            message.setType(operation == Request::Operation::GET_ACCOUNT_BALANCES ? Message::Type::GET_ACCOUNT_BALANCES : Message::Type::GET_ACCOUNTS);
            std::vector<Element> elementList;
            {
              Element element;
              element.insert("ASSET", this->baseAsset);
              element.insert("QUANTITY_AVAILABLE_FOR_TRADING",
                             Decimal(AppUtil::printDoubleScientific(this->baseBalance / this->baseAvailableBalanceProportion)).toString());
              elementList.push_back(element);
            }
            {
              Element element;
              element.insert("ASSET", this->quoteAsset);
              element.insert("QUANTITY_AVAILABLE_FOR_TRADING",
                             Decimal(AppUtil::printDoubleScientific(this->quoteBalance / this->quoteAvailableBalanceProportion)).toString());
              elementList.push_back(element);
            }
            message.setElementList(elementList);
            virtualEvent.setMessageList({message});
          } else if (operation == Request::Operation::CREATE_ORDER) {
            auto newBaseBalance = this->baseBalance;
            auto newQuoteBalance = this->quoteBalance;
            auto param = request.getParamList().at(0);
            auto side = param.at("SIDE");
            std::string price = param.at("LIMIT_PRICE");
            std::string quantity = param.at("QUANTITY");
            std::string clientOrderId = param.at("CLIENT_ORDER_ID");
            bool sufficientBalance = false;
            if (side == "BUY") {
              double transactedAmount = std::stod(price) * std::stod(quantity);
              if (UtilString::toLower(this->makerBuyerFeeAsset) == UtilString::toLower(this->quoteAsset)) {
                transactedAmount *= 1 + this->makerFee;
              }
              newQuoteBalance -= transactedAmount;
              if (newQuoteBalance >= 0) {
                sufficientBalance = true;
              } else {
                this->appLogger->log("Insufficient quote balance.");
              }
            } else if (side == "SELL") {
              double transactedAmount = std::stod(quantity);
              if (UtilString::toLower(this->makerSellerFeeAsset) == UtilString::toLower(this->baseAsset)) {
                transactedAmount *= 1 + this->makerFee;
              }
              newBaseBalance -= transactedAmount;
              if (newBaseBalance >= 0) {
                sufficientBalance = true;
              } else {
                this->appLogger->log("Insufficient base balance.");
              }
            }
            if (sufficientBalance) {
              virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              Order order;
              order.orderId = std::to_string(++this->virtualOrderId);
              order.clientOrderId = clientOrderId;
              order.side = side;
              order.limitPrice = Decimal(price);
              order.quantity = Decimal(quantity);
              order.cumulativeFilledQuantity = Decimal("0");
              order.remainingQuantity = order.quantity;
              order.status = "NEW";
              auto element = this->extractOrderInfo(order);
              if (side == "BUY") {
                this->openBuyOrder = order;
              } else if (side == "SELL") {
                this->openSellOrder = order;
              }
              message.setElementList({element});
              virtualEvent.setMessageList({message});
              virtualEvent_2.setType(Event::Type::RESPONSE);
              message_2.setType(Message::Type::CREATE_ORDER);
              message_2.setElementList({element});
              virtualEvent_2.setMessageList({message_2});
            } else {
              virtualEvent_2.setType(Event::Type::RESPONSE);
              message_2.setType(Message::Type::RESPONSE_ERROR);
              Element element;
              element.insert("ERROR_MESSAGE", "Insufficient balance.");
              message_2.setElementList({element});
              virtualEvent_2.setMessageList({message_2});
            }
          } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
            virtualEvent.setType(Event::Type::SUBSCRIPTION_DATA);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            if (this->openBuyOrder) {
              elementList.push_back(this->extractOrderInfo(this->openBuyOrder.get()));
              this->openBuyOrder = boost::none;
            }
            if (this->openSellOrder) {
              elementList.push_back(this->extractOrderInfo(this->openSellOrder.get()));
              this->openSellOrder = boost::none;
            }
            if (!elementList.empty()) {
              message.setElementList(elementList);
              virtualEvent.setMessageList({message});
            }
            virtualEvent_2.setType(Event::Type::RESPONSE);
            message_2.setType(Message::Type::CANCEL_OPEN_ORDERS);
            message_2.setElementList(elementList);
            virtualEvent_2.setMessageList({message_2});
          }
          if (!virtualEvent.getMessageList().empty()) {
            this->appLogger->logDebug("Generated a virtual event: " + virtualEvent.toStringPretty(), this->printDebug);
            this->processEvent(virtualEvent, session);
          }
          if (!virtualEvent_2.getMessageList().empty()) {
            this->appLogger->logDebug("Generated a virtual event: " + virtualEvent_2.toStringPretty(), this->printDebug);
            this->processEvent(virtualEvent_2, session);
          }
        }
      } else {
        session->sendRequest(requestList);
      }
    }
    return true;
  }
  void placeOrders(std::vector<Request>& requestList, const TimePoint& now) {
    double midPrice;
    if (!this->bestBidPrice.empty() && !this->bestAskPrice.empty()) {
      if (this->useWeightedMidPrice) {
        midPrice = (std::stod(this->bestBidPrice) * std::stod(this->bestAskSize) + std::stod(this->bestAskPrice) * std::stod(this->bestBidSize)) /
                   (std::stod(this->bestBidSize) + std::stod(this->bestAskSize));
      } else {
        midPrice = (std::stod(this->bestBidPrice) + std::stod(this->bestAskPrice)) / 2;
      }
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
          requestList.push_back(this->createRequestForCreateOrder("BUY", buyPrice, orderQuantity, now));
        } else {
          this->appLogger->log("Insufficient quote balance.");
        }
        std::string sellPrice = AppUtil::roundInput(
            midPrice * (1 + AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 0, this->halfSpreadMaximum, r)),
            this->orderPriceIncrement, true);
        if (std::stod(orderQuantity) <= this->baseBalance) {
          requestList.push_back(this->createRequestForCreateOrder("SELL", sellPrice, orderQuantity, now));
        } else {
          this->appLogger->log("Insufficient base balance.");
        }
      } else {
        std::string buyPrice = AppUtil::roundInput(
            midPrice * (1 - AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 1, this->halfSpreadMaximum, r)),
            this->orderPriceIncrement, false);
        if (std::stod(buyPrice) * std::stod(orderQuantity) <= this->quoteBalance) {
          requestList.push_back(this->createRequestForCreateOrder("BUY", buyPrice, orderQuantity, now));
        } else {
          this->appLogger->log("Insufficient quote balance.");
        }
        std::string sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
        if (std::stod(orderQuantity) <= this->baseBalance) {
          requestList.push_back(this->createRequestForCreateOrder("SELL", sellPrice, orderQuantity, now));
        } else {
          this->appLogger->log("Insufficient base balance.");
        }
      }
    } else {
      this->appLogger->log("Account has no assets. Skip.");
      return;
    }
  }
  std::string previousMessageTimeISODate, exchange, instrumentRest, instrumentWebsocket, baseAsset, quoteAsset, accountId, orderPriceIncrement,
      orderQuantityIncrement, privateDataDirectory, privateDataFilePrefix,
      // publicSubscriptionDataMareketDepthCorrelationId{"MARKET_DEPTH"},
      // publicSubscriptionDataTradeCorrelationId{"TRADE"},
      privateSubscriptionDataCorrelationId{"PRIVATE_TRADE,ORDER_UPDATE"}, bestBidPrice, bestBidSize, bestAskPrice, bestAskSize,
      cancelOpenOrdersRequestCorrelationId, getAccountBalancesRequestCorrelationId;
  double halfSpreadMinimum, halfSpreadMaximum, inventoryBasePortionTarget, baseBalance, quoteBalance, baseAvailableBalanceProportion,
      quoteAvailableBalanceProportion, orderQuantityProportion, totalBalancePeak, killSwitchMaximumDrawdown;
  int orderRefreshIntervalSeconds, orderRefreshIntervalOffsetSeconds, accountBalanceRefreshWaitSeconds, clockStepSeconds;
  TimePoint orderRefreshLastTime{std::chrono::seconds{0}}, cancelOpenOrdersLastTime{std::chrono::seconds{0}},
      getAccountBalancesLastTime{std::chrono::seconds{0}};
  bool useGetAccountsToGetAccountBalances{}, printDebug{}, useWeightedMidPrice{};
  TradingMode tradingMode{TradingMode::LIVE};
  std::shared_ptr<std::promise<void>> promisePtr{nullptr};

  // start: only applicable to paper trade and backtest
  double makerFee;
  std::string makerBuyerFeeAsset, makerSellerFeeAsset;
  boost::optional<Order> openBuyOrder, openSellOrder;
  // end: only applicable to paper trade and backtest

  // start: only applicable to backtest
  TimePoint startDateTp, endDateTp;
  std::string historicalMarketDataDirectory;
  // end: only applicable to backtest

 private:
  static std::string generateUuidV4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
      ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
      ss << dis(gen);
    };
    return ss.str();
  }
  std::string createClientOrderId(const std::string& exchange, const std::string& instrument, const std::string& side, const std::string& price,
                                  const std::string& quantity, const TimePoint& now) {
    std::string clientOrderId;
    if (exchange == "coinbase") {
      clientOrderId = SpotMarketMakingEventHandler::generateUuidV4();
    } else {
      clientOrderId += instrument;
      clientOrderId += "_";
      clientOrderId += UtilTime::getISOTimestamp<std::chrono::seconds>(std::chrono::time_point_cast<std::chrono::seconds>(now), "%F%T");
      clientOrderId += "_";
      clientOrderId += side;
      clientOrderId += "_";
      clientOrderId += price;
      clientOrderId += "_";
      clientOrderId += quantity;
    }
    return clientOrderId;
  }
  Request createRequestForCreateOrder(const std::string& side, const std::string& price, const std::string& quantity, const TimePoint& now) {
    Request request(Request::Operation::CREATE_ORDER, this->exchange, this->instrumentRest);
    request.appendParam({
        {"SIDE", side},
        {"QUANTITY", quantity},
        {"LIMIT_PRICE", price},
        {"CLIENT_ORDER_ID", this->createClientOrderId(this->exchange, this->instrumentRest, side, price, quantity, now)},
    });
    request.setTimeSent(now);
    this->appLogger->log("Place order - side: " + side + ", price: " + price + ", quantity: " + quantity + ".");
    return request;
  }
  Element extractOrderInfo(const Order& order) {
    Element element;
    element.insert("ORDER_ID", order.orderId);
    element.insert("CLIENT_ORDER_ID", order.clientOrderId);
    element.insert("SIDE", order.side);
    element.insert("LIMIT_PRICE", order.limitPrice.toString());
    element.insert("QUANTITY", order.quantity.toString());
    element.insert("CUMULATIVE_FILLED_QUANTITY", order.cumulativeFilledQuantity.toString());
    element.insert("REMAINING_QUANTITY", order.remainingQuantity.toString());
    element.insert("STATUS", order.status);
    return element;
  }
  AppLogger* appLogger;
  CsvWriter* privateTradeCsvWriter;
  CsvWriter* orderUpdateCsvWriter;
  CsvWriter* accountBalanceCsvWriter;
  int64_t virtualTradeId;
  int64_t virtualOrderId;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
