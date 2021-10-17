#ifndef APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#define APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
#ifndef APP_SPOT_MARKET_MAKING_ORDER_STATUS_NEW
#define APP_SPOT_MARKET_MAKING_ORDER_STATUS_NEW "NEW"
#endif
#ifndef APP_SPOT_MARKET_MAKING_ORDER_STATUS_CANCELED
#define APP_SPOT_MARKET_MAKING_ORDER_STATUS_CANCELED "CANCELED"
#endif
#ifndef APP_SPOT_MARKET_MAKING_ORDER_STATUS_PARTIALLY_FILLED
#define APP_SPOT_MARKET_MAKING_ORDER_STATUS_PARTIALLY_FILLED "PARTIALLY_FILLED"
#endif
#ifndef APP_SPOT_MARKET_MAKING_ORDER_STATUS_FILLED
#define APP_SPOT_MARKET_MAKING_ORDER_STATUS_FILLED "FILLED"
#endif
#include <sys/stat.h>

#include <random>
#include <sstream>

#include "app/common.h"
#include "app/historical_market_data_event_processor.h"
#include "app/order.h"
#include "boost/optional/optional.hpp"
#include "ccapi_cpp/ccapi_session.h"
// #include <filesystem>
namespace ccapi {
class SpotMarketMakingEventHandler : public EventHandler {
 public:
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
    std::string baseBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString();
    std::string quoteBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString();
    APP_LOGGER_DEBUG("Base asset balance is " + baseBalanceDecimalNotation + ", quote asset balance is " + quoteBalanceDecimalNotation + ".");
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
          if (!this->privateDataOnlySaveFinalSummary) {
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
            double feeQuantity = std::stod(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY));
            std::string feeAsset = element.getValue(CCAPI_EM_ORDER_FEE_ASSET);
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
                filled = UtilString::normalizeDecimalString(quantity) == UtilString::normalizeDecimalString(cumulativeFilledQuantity);
              } else if (!remainingQuantity.empty()) {
                filled = UtilString::normalizeDecimalString(remainingQuantity) == "0";
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
                if (this->accountBalanceRefreshWaitSeconds == 0) {
                  this->getAccountBalances(requestList, messageTimeReceived, messageTimeReceivedISO);
                }
              }
            }
          }
          if (!this->privateDataOnlySaveFinalSummary) {
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
                  order.status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_PARTIALLY_FILLED;
                  if (isBuyerMaker) {
                    this->openBuyOrder = order;
                  } else {
                    this->openSellOrder = order;
                  }
                } else {
                  lastFilledQuantity = order.remainingQuantity;
                  order.cumulativeFilledQuantity = order.quantity;
                  order.remainingQuantity = Decimal("0");
                  order.status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_FILLED;
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
          int intervalStart = UtilTime::getUnixTimestamp(messageTime) / this->adverseSelectionGuardMarketDataSampleIntervalSeconds *
                              this->adverseSelectionGuardMarketDataSampleIntervalSeconds;
          this->publicTradeMap.erase(this->publicTradeMap.begin(),
                                     this->publicTradeMap.upper_bound(intervalStart - this->adverseSelectionGuardMarketDataSampleBufferSizeSeconds));
          const auto& elementList = message.getElementList();
          auto rit = elementList.rbegin();
          if (rit != elementList.rend()) {
            this->publicTradeMap[intervalStart] = std::stod(rit->getValue(CCAPI_LAST_PRICE));
          }
        }
      }
      if (index != -1) {
        const auto& message = messageList.at(index);
        const auto& messageTime = message.getTime();
        for (const auto& element : message.getElementList()) {
          const auto& elementNameValueMap = element.getNameValueMap();
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_BID_N_PRICE);
            if (it != elementNameValueMap.end()) {
              this->bestBidPrice = it->second;
            }
          }
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_ASK_N_PRICE);
            if (it != elementNameValueMap.end()) {
              this->bestAskPrice = it->second;
            }
          }
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_BID_N_SIZE);
            if (it != elementNameValueMap.end()) {
              this->bestBidSize = it->second;
            }
          }
          {
            auto it = elementNameValueMap.find(CCAPI_BEST_ASK_N_SIZE);
            if (it != elementNameValueMap.end()) {
              this->bestAskSize = it->second;
            }
          }
        }
        const std::string& messageTimeISO = UtilTime::getISOTimestamp(messageTime);
        const std::string& messageTimeISODate = messageTimeISO.substr(0, 10);
        if (this->previousMessageTimeISODate.empty() || messageTimeISODate != previousMessageTimeISODate) {
          std::string prefix;
          if (!this->privateDataFilePrefix.empty()) {
            prefix = this->privateDataFilePrefix + "__";
          }
          std::string suffix;
          if (!this->privateDataFileSuffix.empty()) {
            suffix = "__" + this->privateDataFileSuffix;
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
          if (this->numOpenOrders != 0) {
#ifdef CANCEL_OPEN_ORDERS_REQUEST_CORRELATION_ID
            this->cancelOpenOrdersRequestCorrelationId = CANCEL_OPEN_ORDERS_REQUEST_CORRELATION_ID;
#else
            this->cancelOpenOrdersRequestCorrelationId = messageTimeISO + "-CANCEL_OPEN_ORDERS";
#endif
            Request request(Request::Operation::CANCEL_OPEN_ORDERS, this->exchange, this->instrumentRest, this->cancelOpenOrdersRequestCorrelationId);
            request.setTimeSent(messageTime);
            requestList.emplace_back(std::move(request));
            this->numOpenOrders = 0;
            APP_LOGGER_INFO("Cancel open orders.");
          }
          this->orderRefreshLastTime = messageTime;
          this->cancelOpenOrdersLastTime = messageTime;
          if (this->accountBalanceRefreshWaitSeconds == 0) {
            this->getAccountBalances(requestList, messageTime, messageTimeISO);
          }
        } else if (std::chrono::duration_cast<std::chrono::seconds>(messageTime - this->cancelOpenOrdersLastTime).count() >=
                       this->accountBalanceRefreshWaitSeconds &&
                   this->getAccountBalancesLastTime < this->cancelOpenOrdersLastTime &&
                   this->cancelOpenOrdersLastTime + std::chrono::seconds(this->accountBalanceRefreshWaitSeconds) >= this->orderRefreshLastTime) {
          this->getAccountBalances(requestList, messageTime, messageTimeISO);
        }
      }
    } else if (eventType == Event::Type::RESPONSE) {
      const auto& firstMessage = event.getMessageList().at(0);
      const auto& correlationIdList = firstMessage.getCorrelationIdList();
      const auto& messageTimeReceived = firstMessage.getTimeReceived();
      const auto& messageTimeReceivedISO = UtilTime::getISOTimestamp(messageTimeReceived);
      if (firstMessage.getType() == Message::Type::RESPONSE_ERROR) {
        for (const auto& element : firstMessage.getElementList()) {
          APP_LOGGER_INFO("Received an error: " + element.getValue(CCAPI_ERROR_MESSAGE) + ".");
        }
      }
      if (std::find(correlationIdList.begin(), correlationIdList.end(), this->getAccountBalancesRequestCorrelationId) != correlationIdList.end()) {
        if (this->tradingMode == TradingMode::LIVE) {
          for (const auto& element : firstMessage.getElementList()) {
            const auto& asset = element.getValue(CCAPI_EM_ASSET);
            if (asset == this->baseAsset) {
              this->baseBalance = std::stod(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING)) * this->baseAvailableBalanceProportion;
            } else if (asset == this->quoteAsset) {
              this->quoteBalance = std::stod(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING)) * this->quoteAvailableBalanceProportion;
            }
          }
        }
        const auto& baseBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->baseBalance)).toString();
        const auto& quoteBalanceDecimalNotation = Decimal(UtilString::printDoubleScientific(this->quoteBalance)).toString();
        if (!this->privateDataOnlySaveFinalSummary) {
          this->accountBalanceCsvWriter->writeRow({
              messageTimeReceivedISO,
              baseBalanceDecimalNotation,
              quoteBalanceDecimalNotation,
              this->bestBidPrice,
              this->bestAskPrice,
          });
          this->accountBalanceCsvWriter->flush();
        }
        APP_LOGGER_INFO(this->baseAsset + " balance is " + baseBalanceDecimalNotation + ", " + this->quoteAsset + " balance is " + quoteBalanceDecimalNotation +
                        ".");
        APP_LOGGER_INFO("Best bid price is " + this->bestBidPrice + ", best bid size is " + this->bestBidSize + ", best ask price is " + this->bestAskPrice +
                        ", best ask size is " + this->bestAskSize + ".");
        this->placeOrders(requestList, messageTimeReceived);
        this->numOpenOrders = requestList.size();
      } else if (std::find(correlationIdList.begin(), correlationIdList.end(), "GET_INSTRUMENT") != correlationIdList.end()) {
        const auto& element = firstMessage.getElementList().at(0);
        this->baseAsset = element.getValue("BASE_ASSET");
        APP_LOGGER_INFO("Base asset is " + this->baseAsset);
        this->quoteAsset = element.getValue("QUOTE_ASSET");
        APP_LOGGER_INFO("Quote asset is " + this->quoteAsset);
        this->orderPriceIncrement = UtilString::normalizeDecimalString(element.getValue("PRICE_INCREMENT"));
        APP_LOGGER_INFO("Order price increment is " + this->orderPriceIncrement);
        this->orderQuantityIncrement = UtilString::normalizeDecimalString(element.getValue("QUANTITY_INCREMENT"));
        APP_LOGGER_INFO("Order quantity increment is " + this->orderQuantityIncrement);
        if (this->tradingMode == TradingMode::BACKTEST) {
          HistoricalMarketDataEventProcessor historicalMarketDataEventProcessor(
              std::bind(&SpotMarketMakingEventHandler::processEvent, this, std::placeholders::_1, nullptr));
          historicalMarketDataEventProcessor.exchange = this->exchange;
          historicalMarketDataEventProcessor.baseAsset = UtilString::toLower(this->baseAsset);
          historicalMarketDataEventProcessor.quoteAsset = UtilString::toLower(this->quoteAsset);
          historicalMarketDataEventProcessor.startDateTp = this->startDateTp;
          historicalMarketDataEventProcessor.endDateTp = this->endDateTp;
          historicalMarketDataEventProcessor.historicalMarketDataDirectory = this->historicalMarketDataDirectory;
          historicalMarketDataEventProcessor.historicalMarketDataFilePrefix = this->historicalMarketDataFilePrefix;
          historicalMarketDataEventProcessor.historicalMarketDataFileSuffix = this->historicalMarketDataFileSuffix;
          historicalMarketDataEventProcessor.clockStepSeconds = this->clockStepSeconds;
          historicalMarketDataEventProcessor.processEvent();
          std::string prefix;
          if (!this->privateDataFilePrefix.empty()) {
            prefix = this->privateDataFilePrefix + "__";
          }
          std::string suffix;
          if (!this->privateDataFileSuffix.empty()) {
            suffix = "__" + this->privateDataFileSuffix;
          }
          std::string privateDataSummaryCsvFilename(prefix + this->exchange + "__" + UtilString::toLower(this->baseAsset) + "-" +
                                                    UtilString::toLower(this->quoteAsset) + "__" + UtilTime::getISOTimestamp(this->startDateTp).substr(0, 10) +
                                                    "__" + UtilTime::getISOTimestamp(this->endDateTp).substr(0, 10) + "__summary" + suffix + ".csv");
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
          this->promisePtr->set_value();
        } else {
          std::vector<Subscription> subscriptionList;
          std::string options;
          if (!this->enableUpdateOrderBookTickByTick) {
            options = std::string(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) + "=" + std::to_string(this->clockStepSeconds * 1000) + "&" +
                      CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS + "=0";
          }
          subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, "MARKET_DEPTH", options,
                                        PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID);
          if (this->tradingMode == TradingMode::PAPER || this->enableAdverseSelectionGuard) {
            std::string field = "TRADE";
            if (this->exchange.rfind("binance", 0) == 0) {
              field = "AGG_TRADE";
            }
            subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, field, "", PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID);
          } else {
            subscriptionList.emplace_back(this->exchange, this->instrumentWebsocket, "PRIVATE_TRADE,ORDER_UPDATE", "",
                                          PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID);
          }
          session->subscribe(subscriptionList);
        }
      }
    }
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
              order.status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_NEW;
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
              element.insert("ERROR_MESSAGE", "Insufficient balance.");
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
              this->openBuyOrder.get().status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_CANCELED;
              Element element;
              this->extractOrderInfo(element, this->openBuyOrder.get());
              elementList.emplace_back(std::move(element));
              this->openBuyOrder = boost::none;
            }
            if (this->openSellOrder) {
              this->openSellOrder.get().status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_CANCELED;
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
            if ((createdBuyOrder && this->openBuyOrder->limitPrice >= Decimal(this->bestAskPrice)) ||
                (!createdBuyOrder && this->openSellOrder->limitPrice <= Decimal(this->bestBidPrice))) {
              Order matchedOrder = createdBuyOrder ? this->openBuyOrder.get() : this->openSellOrder.get();
              Decimal quantityToMatch = Decimal(createdBuyOrder ? this->bestAskSize : this->bestBidSize);
              if (quantityToMatch >= matchedOrder.quantity) {
                matchedOrder.cumulativeFilledQuantity = matchedOrder.quantity;
                matchedOrder.remainingQuantity = Decimal("0");
                matchedOrder.status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_FILLED;
                if (createdBuyOrder) {
                  this->openBuyOrder = boost::none;
                } else {
                  this->openSellOrder = boost::none;
                }
              } else {
                matchedOrder.cumulativeFilledQuantity = quantityToMatch;
                matchedOrder.remainingQuantity = matchedOrder.quantity.subtract(quantityToMatch);
                matchedOrder.status = APP_SPOT_MARKET_MAKING_ORDER_STATUS_PARTIALLY_FILLED;
                if (createdBuyOrder) {
                  this->openBuyOrder = matchedOrder;
                } else {
                  this->openSellOrder = matchedOrder;
                }
              }
              if (createdBuyOrder) {
                this->baseBalance += matchedOrder.cumulativeFilledQuantity.toDouble();
                this->quoteBalance -= matchedOrder.limitPrice.toDouble() * matchedOrder.cumulativeFilledQuantity.toDouble();
              } else {
                this->baseBalance -= matchedOrder.cumulativeFilledQuantity.toDouble();
                this->quoteBalance += matchedOrder.limitPrice.toDouble() * matchedOrder.cumulativeFilledQuantity.toDouble();
              }
              double feeQuantity = 0;
              if ((createdBuyOrder && UtilString::toLower(this->takerBuyerFeeAsset) == UtilString::toLower(this->baseAsset)) ||
                  (!createdBuyOrder && UtilString::toLower(this->takerSellerFeeAsset) == UtilString::toLower(this->baseAsset))) {
                feeQuantity = matchedOrder.cumulativeFilledQuantity.toDouble() * this->takerFee;
                this->baseBalance -= feeQuantity;
              } else if ((createdBuyOrder && UtilString::toLower(this->takerBuyerFeeAsset) == UtilString::toLower(this->quoteAsset)) ||
                         (!createdBuyOrder && UtilString::toLower(this->takerSellerFeeAsset) == UtilString::toLower(this->quoteAsset))) {
                feeQuantity = matchedOrder.limitPrice.toDouble() * matchedOrder.cumulativeFilledQuantity.toDouble() * this->takerFee;
                this->quoteBalance -= feeQuantity;
              }
              virtualEvent_3.setType(Event::Type::SUBSCRIPTION_DATA);
              std::vector<Message> messageList;
              {
                Message message;
                message.setCorrelationIdList({PRIVATE_SUBSCRIPTION_DATA_CORRELATION_ID});
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                message.setTime(now);
                message.setTimeReceived(now);
                Element element;
                element.insert(CCAPI_TRADE_ID, std::to_string(++this->virtualTradeId));
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, matchedOrder.limitPrice.toString());
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, matchedOrder.cumulativeFilledQuantity.toString());
                element.insert(CCAPI_EM_ORDER_SIDE, matchedOrder.side);
                element.insert(CCAPI_IS_MAKER, "0");
                element.insert(CCAPI_EM_ORDER_ID, matchedOrder.orderId);
                element.insert(CCAPI_EM_CLIENT_ORDER_ID, matchedOrder.clientOrderId);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, Decimal(UtilString::printDoubleScientific(feeQuantity)).toString());
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, createdBuyOrder ? this->takerBuyerFeeAsset : this->takerSellerFeeAsset);
                std::vector<Element> elementList;
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
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
          if (!virtualEvent_3.getMessageList().empty()) {
            APP_LOGGER_DEBUG("Generated a virtual event: " + virtualEvent_3.toStringPretty());
            this->processEvent(virtualEvent_3, session);
          }
        }
      } else {
        session->sendRequest(requestList);
      }
    }
    return true;
  }
  void getAccountBalances(std::vector<Request>& requestList, const TimePoint& messageTime, const std::string& messageTimeISO) {
#ifdef GET_ACCOUNT_BALANCES_REQUEST_CORRELATION_ID
    this->getAccountBalancesRequestCorrelationId = GET_ACCOUNT_BALANCES_REQUEST_CORRELATION_ID;
#else
    this->getAccountBalancesRequestCorrelationId = messageTimeISO + "-GET_ACCOUNT_BALANCES";
#endif
    Request request(this->useGetAccountsToGetAccountBalances ? Request::Operation::GET_ACCOUNTS : Request::Operation::GET_ACCOUNT_BALANCES, this->exchange, "",
                    this->getAccountBalancesRequestCorrelationId);
    request.setTimeSent(messageTime);
    if (!this->accountId.empty()) {
      request.appendParam({
          {CCAPI_EM_ACCOUNT_ID, this->accountId},
      });
    }
    requestList.emplace_back(std::move(request));
    this->getAccountBalancesLastTime = messageTime;
    APP_LOGGER_INFO("Get account balances.");
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
      APP_LOGGER_INFO("At least one side of the order book is empty. Skip.");
      return;
    }
    if (this->baseBalance > 0 || this->quoteBalance > 0) {
      double totalBalance = this->baseBalance * midPrice + this->quoteBalance;
      if (totalBalance > this->totalBalancePeak) {
        this->totalBalancePeak = totalBalance;
      }
      if ((this->totalBalancePeak - totalBalance) / this->totalBalancePeak > this->killSwitchMaximumDrawdown) {
        APP_LOGGER_INFO("Kill switch triggered - Maximum drawdown. Exit.");
        this->promisePtr->set_value();
        this->skipProcessEvent = true;
        return;
      }
      double r = this->baseBalance * midPrice / totalBalance;
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
              AppUtil::roundInput((this->quoteBalance / midPrice + this->baseBalance) * this->orderQuantityProportion, this->orderQuantityIncrement, false);
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
                                                  ? (std::stod(this->bestAskPrice) - midPrice) / midPrice
                                                  : (std::stod(this->bestAskPrice) - std::stod(this->orderPriceIncrement) - midPrice) / midPrice,
                                              r);
              buyPrice = AppUtil::roundInput(midPrice * (1 - halfSpread), this->orderPriceIncrement, false);
            } else {
              buyPrice = AppUtil::roundInput(midPrice * (1 - this->halfSpreadMinimum), this->orderPriceIncrement, false);
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
                midPrice * (1 + AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 0, this->halfSpreadMaximum, r)),
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
                midPrice * (1 - AppUtil::linearInterpolate(this->inventoryBasePortionTarget, this->halfSpreadMinimum, 1, this->halfSpreadMaximum, r)),
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
                                                 ? (std::stod(this->bestBidPrice) - midPrice) / midPrice
                                                 : (std::stod(this->bestBidPrice) + std::stod(this->orderPriceIncrement) - midPrice) / midPrice,
                                             r);
              sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpread), this->orderPriceIncrement, true);
            } else {
              sellPrice = AppUtil::roundInput(midPrice * (1 + halfSpreadMinimum), this->orderPriceIncrement, true);
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
            std::string orderQuantity =
                this->adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset
                    ? AppUtil::roundInput(this->quoteBalance / conversionPrice * this->adverseSelectionGuardActionOrderQuantityProportion,
                                          this->orderQuantityIncrement, false)
                    : AppUtil::roundInput(
                          std::min((this->quoteBalance / conversionPrice + this->baseBalance) * this->adverseSelectionGuardActionOrderQuantityProportion,
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
    } else {
      APP_LOGGER_INFO("Account has no assets. Skip.");
    }
  }
  std::string previousMessageTimeISODate, exchange, instrumentRest, instrumentWebsocket, baseAsset, quoteAsset, accountId, orderPriceIncrement,
      orderQuantityIncrement, privateDataDirectory, privateDataFilePrefix, privateDataFileSuffix, bestBidPrice, bestBidSize, bestAskPrice, bestAskSize,
      cancelOpenOrdersRequestCorrelationId, getAccountBalancesRequestCorrelationId;
  double halfSpreadMinimum{}, halfSpreadMaximum{}, inventoryBasePortionTarget{}, baseBalance{}, quoteBalance{}, baseAvailableBalanceProportion{},
      quoteAvailableBalanceProportion{}, orderQuantityProportion{}, totalBalancePeak{}, killSwitchMaximumDrawdown{},
      adverseSelectionGuardTriggerInventoryBasePortionMinimum{}, adverseSelectionGuardTriggerInventoryBasePortionMaximum{},
      adverseSelectionGuardActionOrderQuantityProportion{}, adverseSelectionGuardTriggerRollCorrelationCoefficientMaximum{},
      adverseSelectionGuardTriggerRocMinimum{}, adverseSelectionGuardTriggerRocMaximum{}, adverseSelectionGuardTriggerRsiMinimum{},
      adverseSelectionGuardTriggerRsiMaximum{}, privateTradeVolumeInBaseSum{}, privateTradeVolumeInQuoteSum{}, privateTradeFeeInBaseSum{},
      privateTradeFeeInQuoteSum{};
  int orderRefreshIntervalSeconds{}, orderRefreshIntervalOffsetSeconds{}, accountBalanceRefreshWaitSeconds{}, clockStepSeconds{},
      adverseSelectionGuardActionOrderRefreshIntervalSeconds{}, originalOrderRefreshIntervalSeconds{}, adverseSelectionGuardMarketDataSampleIntervalSeconds{},
      adverseSelectionGuardMarketDataSampleBufferSizeSeconds{}, adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations{},
      adverseSelectionGuardTriggerRocNumObservations{}, adverseSelectionGuardTriggerRsiNumObservations{};
  TimePoint orderRefreshLastTime{std::chrono::seconds{0}}, cancelOpenOrdersLastTime{std::chrono::seconds{0}},
      getAccountBalancesLastTime{std::chrono::seconds{0}};
  bool useGetAccountsToGetAccountBalances{}, useWeightedMidPrice{}, privateDataOnlySaveFinalSummary{}, enableAdverseSelectionGuard{},
      enableAdverseSelectionGuardByInventoryLimit{}, enableAdverseSelectionGuardByInventoryDepletion{},
      enableAdverseSelectionGuardByRollCorrelationCoefficient{}, adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset{},
      enableAdverseSelectionGuardByRoc{}, enableAdverseSelectionGuardByRsi{}, enableUpdateOrderBookTickByTick{}, immediatelyPlaceNewOrders{},
      adverseSelectionGuardTriggerRocOrderDirectionReverse{}, adverseSelectionGuardTriggerRsiOrderDirectionReverse{},
      adverseSelectionGuardTriggerRollCorrelationCoefficientOrderDirectionReverse{}, enableMarketMaking{};
  TradingMode tradingMode{TradingMode::LIVE};
  AdverseSelectionGuardActionType adverseSelectionGuardActionType{AdverseSelectionGuardActionType::NONE};
  std::shared_ptr<std::promise<void>> promisePtr{nullptr};
  int numOpenOrders;
  boost::optional<Order> openBuyOrder, openSellOrder;

  // start: only applicable to paper trade and backtest
  double makerFee{}, takerFee{};
  std::string makerBuyerFeeAsset, makerSellerFeeAsset, takerBuyerFeeAsset, takerSellerFeeAsset;
  // end: only applicable to paper trade and backtest

  // start: only applicable to backtest
  TimePoint startDateTp, endDateTp;
  std::string historicalMarketDataDirectory, historicalMarketDataFilePrefix, historicalMarketDataFileSuffix;
  // end: only applicable to backtest

 protected:
  std::string createClientOrderId(const std::string& exchange, const std::string& instrument, const std::string& side, const std::string& price,
                                  const std::string& quantity, const TimePoint& now) {
    std::string clientOrderId;
    if (this->tradingMode == TradingMode::BACKTEST) {
      clientOrderId += UtilTime::getISOTimestamp<std::chrono::seconds>(std::chrono::time_point_cast<std::chrono::seconds>(now), "%F%T");
      clientOrderId += "_";
      clientOrderId += side;
    } else {
      if (exchange == "coinbase") {
        clientOrderId = AppUtil::generateUuidV4();
      } else if (exchange == "kraken") {
        clientOrderId = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
      } else if (exchange == "gateio") {
        clientOrderId = "t-" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
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
    }
    return clientOrderId;
  }
  Request createRequestForCreateOrder(const std::string& side, const std::string& price, const std::string& quantity, const TimePoint& now) {
    Request request(Request::Operation::CREATE_ORDER, this->exchange, this->instrumentRest);
    request.appendParam({
        {CCAPI_EM_ORDER_SIDE, side},
        {CCAPI_EM_ORDER_QUANTITY, quantity},
        {CCAPI_EM_ORDER_LIMIT_PRICE, price},
        {CCAPI_EM_CLIENT_ORDER_ID, this->createClientOrderId(this->exchange, this->instrumentRest, side, price, quantity, now)},
    });
    request.setTimeSent(now);
    APP_LOGGER_INFO("Place order - side: " + side + ", price: " + price + ", quantity: " + quantity + ".");
    return request;
  }
  void extractOrderInfo(Element& element, const Order& order) {
    element.insert(CCAPI_EM_ORDER_ID, order.orderId);
    element.insert(CCAPI_EM_CLIENT_ORDER_ID, order.clientOrderId);
    element.insert(CCAPI_EM_ORDER_SIDE, order.side);
    element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, order.limitPrice.toString());
    element.insert(CCAPI_EM_ORDER_QUANTITY, order.quantity.toString());
    element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, order.cumulativeFilledQuantity.toString());
    element.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, order.remainingQuantity.toString());
    element.insert(CCAPI_EM_ORDER_STATUS, order.status);
  }
  void checkAdverseSelectionGuardByRollCorrelationCoefficient(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap.size() >= this->adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations) {
      int size = this->adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations - 1;
      double deltaPt[size];
      double deltaPtPlusOne[size];
      auto rit = this->publicTradeMap.rbegin();
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
  void checkAdverseSelectionGuardByRoc(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap.size() >= this->adverseSelectionGuardTriggerRocNumObservations) {
      int size = this->adverseSelectionGuardTriggerRocNumObservations - 1;
      auto rit2 = this->publicTradeMap.rbegin();
      auto rit = this->publicTradeMap.rbegin();
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
  void checkAdverseSelectionGuardByRsi(AdverseSelectionGuardInformedTraderSide& adverseSelectionGuardInformedTraderSide) {
    if (this->publicTradeMap.size() >= this->adverseSelectionGuardTriggerRsiNumObservations) {
      int size = this->adverseSelectionGuardTriggerRsiNumObservations - 1;
      double deltaPt[size];
      auto rit = this->publicTradeMap.rbegin();
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
  int64_t virtualTradeId;
  int64_t virtualOrderId;
  std::map<int, double> publicTradeMap;
  bool skipProcessEvent{};
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_SPOT_MARKET_MAKING_EVENT_HANDLER_H_
