#ifndef APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#define APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#include <fstream>
#include <iostream>

#include "app/common.h"
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class HistoricalMarketDataEventProcessor {
 public:
  explicit HistoricalMarketDataEventProcessor(std::function<bool(const Event& event)> eventHandler) : eventHandler(eventHandler) {}
  void processEvent() {
    this->clockSeconds = 0;
    auto currentDateTp = this->historicalMarketDataStartDateTp;
    std::string lineMarketDepth;
    std::string lineTrade;
    bool shouldContinueTrade{true};
    std::vector<std::string> previousSplittedMarketDepth;
    while (currentDateTp < this->historicalMarketDataEndDateTp) {
      const auto& currentDateISO = UtilTime::getISOTimestamp<std::chrono::seconds>(currentDateTp).substr(0, 10);
      APP_LOGGER_INFO("Start processing " + currentDateISO + ".");
      std::string fileNameWithDirBase = this->historicalMarketDataDirectory + "/" + this->historicalMarketDataFilePrefix + this->exchange + "__" +
                                        this->baseAsset + "-" + this->quoteAsset + "__" + currentDateISO + "__";
      std::ifstream fMarketDepth;
      std::ifstream fTrade;
      fMarketDepth.open(fileNameWithDirBase + "market-depth" + this->historicalMarketDataFileSuffix + ".csv");
      APP_LOGGER_INFO("Opening file " + fileNameWithDirBase + "market-depth" + this->historicalMarketDataFileSuffix + ".csv.");
      fTrade.open(fileNameWithDirBase + "trade" + this->historicalMarketDataFileSuffix + ".csv");
      APP_LOGGER_INFO("Opening file " + fileNameWithDirBase + "trade" + this->historicalMarketDataFileSuffix + ".csv.");
      if (fMarketDepth && fTrade) {
        APP_LOGGER_INFO("Opened file " + fileNameWithDirBase + "market-depth" + this->historicalMarketDataFileSuffix + ".csv.");
        APP_LOGGER_INFO("Opened file " + fileNameWithDirBase + "trade" + this->historicalMarketDataFileSuffix + ".csv.");
        fMarketDepth.ignore(INT_MAX, '\n');
        fTrade.ignore(INT_MAX, '\n');
        while (std::getline(fMarketDepth, lineMarketDepth) && !lineMarketDepth.empty()) {
          APP_LOGGER_DEBUG("File market-depth next line is " + lineMarketDepth + ".");
          auto splittedMarketDepth = UtilString::split(lineMarketDepth, ',');
          int currentSecondsMarketDepth = std::stoi(splittedMarketDepth.at(0));
          if (currentSecondsMarketDepth < std::chrono::duration_cast<std::chrono::seconds>(this->startTimeTp.time_since_epoch()).count()) {
            continue;
          }
          if (currentSecondsMarketDepth >=
              std::chrono::duration_cast<std::chrono::seconds>(this->startTimeTp.time_since_epoch()).count() + this->totalDurationSeconds) {
            return;
          }
          if (this->clockSeconds == 0) {
            this->clockSeconds = currentSecondsMarketDepth;
            APP_LOGGER_DEBUG("Clock unix timestamp is " + std::to_string(this->clockSeconds) + " seconds.");
            this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade);
            this->processMarketDataEventMarketDepth(splittedMarketDepth);
          } else {
            this->clockSeconds += this->clockStepSeconds;
            APP_LOGGER_DEBUG("Clock unix timestamp is " + std::to_string(this->clockSeconds) + " seconds.");
            while (this->clockSeconds < currentSecondsMarketDepth) {
              this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade);
              previousSplittedMarketDepth[0] = std::to_string(this->clockSeconds);
              this->processMarketDataEventMarketDepth(previousSplittedMarketDepth);
              this->clockSeconds += this->clockStepSeconds;
              APP_LOGGER_DEBUG("Clock unix timestamp is " + std::to_string(this->clockSeconds) + " seconds.");
            }
            this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade);
            this->processMarketDataEventMarketDepth(splittedMarketDepth);
          }
          previousSplittedMarketDepth = std::move(splittedMarketDepth);
        }
        this->clockSeconds += this->clockStepSeconds;
        APP_LOGGER_DEBUG("Clock unix timestamp is " + std::to_string(this->clockSeconds) + " seconds.");
        while (this->clockSeconds < std::chrono::duration_cast<std::chrono::seconds>((currentDateTp + std::chrono::hours(24)).time_since_epoch()).count()) {
          if (this->clockSeconds - this->clockStepSeconds < std::chrono::duration_cast<std::chrono::seconds>(this->startTimeTp.time_since_epoch()).count()) {
            this->clockSeconds += this->clockStepSeconds;
            continue;
          }
          if (this->clockSeconds - this->clockStepSeconds >=
              std::chrono::duration_cast<std::chrono::seconds>(this->startTimeTp.time_since_epoch()).count() + this->totalDurationSeconds) {
            return;
          }
          this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade);
          previousSplittedMarketDepth[0] = std::to_string(this->clockSeconds);
          this->processMarketDataEventMarketDepth(previousSplittedMarketDepth);
          this->clockSeconds += this->clockStepSeconds;
          APP_LOGGER_DEBUG("Clock unix timestamp is " + std::to_string(this->clockSeconds) + " seconds.");
        }
        this->advanceTradeIterator(shouldContinueTrade, fTrade, lineTrade);
        this->clockSeconds -= this->clockStepSeconds;
      } else {
        APP_LOGGER_INFO("Warning: unable to open file for date " + UtilTime::getISOTimestamp(currentDateTp));
      }
      APP_LOGGER_INFO("End processing " + currentDateISO + ".");
      currentDateTp += std::chrono::hours(24);
    }
  }
  TimePoint historicalMarketDataStartDateTp{std::chrono::seconds{0}}, historicalMarketDataEndDateTp{std::chrono::seconds{0}},
      startTimeTp{std::chrono::seconds{0}};
  std::string exchange, baseAsset, quoteAsset, historicalMarketDataDirectory, historicalMarketDataFilePrefix, historicalMarketDataFileSuffix;
  int clockStepSeconds{}, clockSeconds{}, totalDurationSeconds{};

 private:
  void advanceTradeIterator(bool& shouldContinueTrade, std::ifstream& fTrade, std::string& lineTrade) {
    if (!shouldContinueTrade && !lineTrade.empty()) {
      auto splittedTrade = UtilString::split(lineTrade, ',');
      int currentSecondsTrade = std::stoi(splittedTrade.at(0));
      if (currentSecondsTrade < this->clockSeconds) {
        this->processMarketDataEventTrade(splittedTrade);
        shouldContinueTrade = true;
      }
    }
    while (shouldContinueTrade && std::getline(fTrade, lineTrade) && !lineTrade.empty()) {
      APP_LOGGER_DEBUG("File trade next line is " + lineTrade + ".");
      auto splittedTrade = UtilString::split(lineTrade, ',');
      int currentSecondsTrade = std::stoi(splittedTrade.at(0));
      if (currentSecondsTrade < this->clockSeconds) {
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
    elementList.emplace_back(std::move(element));
    message.setElementList(elementList);
    event.addMessage(message);
    APP_LOGGER_DEBUG("Generated a backtest event: " + event.toStringPretty());
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
    if (!splittedLine.at(1).empty()) {
      auto levels = UtilString::split(splittedLine.at(1), '|');
      for (const auto& level : levels) {
        auto found = level.find('_');
        Element element;
        element.insert(CCAPI_BEST_BID_N_PRICE, level.substr(0, found));
        element.insert(CCAPI_BEST_BID_N_SIZE, level.substr(found + 1));
        elementList.emplace_back(std::move(element));
      }
    }
    if (!splittedLine.at(2).empty()) {
      auto levels = UtilString::split(splittedLine.at(2), '|');
      for (const auto& level : levels) {
        auto found = level.find('_');
        Element element;
        element.insert(CCAPI_BEST_ASK_N_PRICE, level.substr(0, found));
        element.insert(CCAPI_BEST_ASK_N_SIZE, level.substr(found + 1));
        elementList.emplace_back(std::move(element));
      }
    }
    message.setElementList(elementList);
    event.addMessage(message);
    APP_LOGGER_DEBUG("Generated a backtest event: " + event.toStringPretty());
    this->eventHandler(event);
  }

  std::function<bool(const Event& event)> eventHandler;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
