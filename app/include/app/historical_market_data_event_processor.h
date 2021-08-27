#ifndef APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#define APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#include <fstream>
#include <iostream>
#include "app/common.h"
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class HistoricalMarketDataEventProcessor {
 public:
  HistoricalMarketDataEventProcessor(AppLogger* appLogger, std::function<bool(const Event& event)> eventHandler)
      : appLogger(appLogger), eventHandler(eventHandler) {}
  void processEvent() {
    int clockSeconds = 0;
    auto currentDateTp = this->startDateTp;
    std::string lineMarketDepth;
    std::string lineTrade;
    Event previousEventMarketDepth;
    Event previousEventTrade;
    bool shouldContinueTrade{true};
    std::vector<std::string> previousSplittedMarketDepth;
    while (currentDateTp < this->endDateTp) {
      auto currentDateISO = UtilTime::getISOTimestamp(currentDateTp, "%F");
      this->appLogger->log("Start processing " + currentDateISO + ".");
      std::string fileNameWithDirBase =
          this->historicalMarketDataDirectory + "/" + this->exchange + "__" + this->baseAsset + "-" + this->quoteAsset + "__" + currentDateISO + "__";
      std::ifstream fMarketDepth;
      std::ifstream fTrade;
      fMarketDepth.open(fileNameWithDirBase + "market-depth.csv");
      fTrade.open(fileNameWithDirBase + "trade.csv");
      if (fMarketDepth && fTrade) {
        this->appLogger->log("Opened file " + fileNameWithDirBase + "market-depth.csv.");
        this->appLogger->log("Opened file " + fileNameWithDirBase + "trade.csv.");
        fMarketDepth.ignore(INT_MAX, '\n');
        fTrade.ignore(INT_MAX, '\n');
        while (std::getline(fMarketDepth, lineMarketDepth) && !lineMarketDepth.empty()) {
          this->appLogger->logDebug("File market-depth next line is " + lineMarketDepth + ".", this->printDebug);
          auto splittedMarketDepth = UtilString::split(lineMarketDepth, ",");
          int currentSecondsMarketDepth = std::stoi(splittedMarketDepth.at(0));
          if (clockSeconds == 0) {
            clockSeconds = currentSecondsMarketDepth;
            this->appLogger->logDebug("Clock unix timestamp is " + std::to_string(clockSeconds) + " seconds.", this->printDebug);
            this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade, clockSeconds);
            this->processMarketDataEventMarketDepth(splittedMarketDepth);
          } else {
            clockSeconds += this->clockStepSeconds;
            this->appLogger->logDebug("Clock unix timestamp is " + std::to_string(clockSeconds) + " seconds.", this->printDebug);
            while (clockSeconds < currentSecondsMarketDepth) {
              this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade, clockSeconds);
              previousSplittedMarketDepth[0] = std::to_string(clockSeconds);
              this->processMarketDataEventMarketDepth(previousSplittedMarketDepth);
              clockSeconds += this->clockStepSeconds;
              this->appLogger->logDebug("Clock unix timestamp is " + std::to_string(clockSeconds) + " seconds.", this->printDebug);
            }
            this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade, clockSeconds);
            this->processMarketDataEventMarketDepth(splittedMarketDepth);
          }
          previousSplittedMarketDepth = std::move(splittedMarketDepth);
        }
        clockSeconds += this->clockStepSeconds;
        this->appLogger->logDebug("Clock unix timestamp is " + std::to_string(clockSeconds) + " seconds.", this->printDebug);
        while (clockSeconds < std::chrono::duration_cast<std::chrono::seconds>((currentDateTp + std::chrono::hours(24)).time_since_epoch()).count()) {
          this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade, clockSeconds);
          previousSplittedMarketDepth[0] = std::to_string(clockSeconds);
          this->processMarketDataEventMarketDepth(previousSplittedMarketDepth);
          clockSeconds += this->clockStepSeconds;
          this->appLogger->logDebug("Clock unix timestamp is " + std::to_string(clockSeconds) + " seconds.", this->printDebug);
        }
        this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade, clockSeconds);
        clockSeconds -= this->clockStepSeconds;
      } else {
        this->appLogger->log("Warning: unable to open file for date " + UtilTime::getISOTimestamp(currentDateTp));
      }
      this->appLogger->log("End processing " + currentDateISO + ".");
      currentDateTp += std::chrono::hours(24);
    }
  }
  TimePoint startDateTp, endDateTp;
  std::string exchange, baseAsset, quoteAsset, historicalMarketDataDirectory;
  bool printDebug{};
  int clockStepSeconds;

 private:
  void advanceTradeIterator(bool& shouldContinueTrade, std::ifstream& fTrade, std::string& lineTrade, int clockSeconds) {
    if (!shouldContinueTrade && !lineTrade.empty()) {
      auto splittedTrade = UtilString::split(lineTrade, ",");
      int currentSecondsTrade = std::stoi(splittedTrade.at(0));
      if (currentSecondsTrade < clockSeconds) {
        this->processMarketDataEventTrade(splittedTrade);
        shouldContinueTrade = true;
      }
    }
    while (shouldContinueTrade && std::getline(fTrade, lineTrade) && !lineTrade.empty()) {
      this->appLogger->logDebug("File trade next line is " + lineTrade + ".", this->printDebug);
      auto splittedTrade = UtilString::split(lineTrade, ",");
      int currentSecondsTrade = std::stoi(splittedTrade.at(0));
      if (currentSecondsTrade < clockSeconds) {
        this->processMarketDataEventTrade(splittedTrade);
      } else {
        shouldContinueTrade = false;
      }
    }
  }
  void processMarketDataEventTrade(const std::vector<std::string>& splittedLine) {
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_DATA);
    Message message;
    message.setType(exchange.rfind("binance", 0) == 0 ? Message::Type::MARKET_DATA_EVENTS_AGG_TRADE : Message::Type::MARKET_DATA_EVENTS_TRADE);
    message.setRecapType(Message::RecapType::NONE);
    TimePoint messageTime = UtilTime::makeTimePoint(UtilTime::divide(splittedLine.at(0)));
    message.setTime(messageTime);
    message.setTimeReceived(messageTime);
    message.setCorrelationIdList({PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID});
    std::vector<Element> elementList;
    Element element;
    element.insert(CCAPI_LAST_PRICE, splittedLine.at(1));
    element.insert(CCAPI_LAST_SIZE, splittedLine.at(2));
    element.insert(CCAPI_IS_BUYER_MAKER, splittedLine.at(3));
    elementList.push_back(std::move(element));
    message.setElementList(elementList);
    event.addMessage(message);
    this->appLogger->logDebug("Generated a backtest event: " + event.toStringPretty(), this->printDebug);
    this->eventHandler(event);
  }
  void processMarketDataEventMarketDepth(const std::vector<std::string>& splittedLine) {
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_DATA);
    Message message;
    message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
    message.setRecapType(Message::RecapType::NONE);
    TimePoint messageTime = UtilTime::makeTimePoint(std::make_pair(std::stol(splittedLine.at(0)), 0));
    message.setTime(messageTime);
    message.setTimeReceived(messageTime);
    message.setCorrelationIdList({PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID});
    std::vector<Element> elementList;
    Element element;
    if (!splittedLine.at(1).empty()) {
      auto priceSize = UtilString::split(splittedLine.at(1), "_");
      element.insert(CCAPI_BEST_BID_N_PRICE, priceSize.at(0));
      element.insert(CCAPI_BEST_BID_N_SIZE, priceSize.at(1));
    }
    if (!splittedLine.at(2).empty()) {
      auto priceSize = UtilString::split(splittedLine.at(2), "_");
      element.insert(CCAPI_BEST_ASK_N_PRICE, priceSize.at(0));
      element.insert(CCAPI_BEST_ASK_N_SIZE, priceSize.at(1));
    }
    elementList.push_back(std::move(element));
    message.setElementList(elementList);
    event.addMessage(message);
    this->appLogger->logDebug("Generated a backtest event: " + event.toStringPretty(), this->printDebug);
    this->eventHandler(event);
  }

  std::function<bool(const Event& event)> eventHandler;
  AppLogger* appLogger;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
