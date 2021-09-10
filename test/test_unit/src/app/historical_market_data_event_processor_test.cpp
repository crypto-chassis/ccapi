#include "app/historical_market_data_event_processor.h"

#include "gtest/gtest.h"
namespace ccapi {
class HistoricalMarketDataEventProcessorTest : public ::testing::Test {
 public:
  void SetUp() override {
    this->eventList.clear();
    this->historicalMarketDataEventProcessor = new HistoricalMarketDataEventProcessor([that = this](const Event& event) -> bool {
      that->eventList.push_back(event);
      return true;
    });
    auto splitted = UtilString::split(UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_TEST"), ",");
    this->historicalMarketDataEventProcessor->exchange = UtilSystem::getEnvAsString("EXCHANGE", splitted.at(0));
    this->historicalMarketDataEventProcessor->baseAsset = UtilSystem::getEnvAsString("BASE_ASSET", splitted.at(1));
    this->historicalMarketDataEventProcessor->quoteAsset = UtilSystem::getEnvAsString("QUOTE_ASSET", splitted.at(2));
    this->historicalMarketDataEventProcessor->startDateTp = UtilTime::parse(UtilSystem::getEnvAsString("START_DATE", splitted.at(3)), "%F");
    this->historicalMarketDataEventProcessor->endDateTp = UtilTime::parse(UtilSystem::getEnvAsString("END_DATE", splitted.at(4)), "%F");
    this->historicalMarketDataEventProcessor->historicalMarketDataDirectory = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_DIRECTORY", splitted.at(5));
    this->historicalMarketDataEventProcessor->clockStepSeconds = UtilSystem::getEnvAsInt("CLOCK_STEP_SECONDS", 1);
  }
  void TearDown() override { delete this->historicalMarketDataEventProcessor; }
  std::vector<Event> eventList;
  HistoricalMarketDataEventProcessor* historicalMarketDataEventProcessor;
};

Message createMarketDataMessageTrade(const std::vector<std::string>& splittedLine, const std::string& exchange) {
  Message message;
  message.setType(exchange.rfind("binance", 0) == 0 ? Message::Type::MARKET_DATA_EVENTS_AGG_TRADE : Message::Type::MARKET_DATA_EVENTS_TRADE);
  message.setRecapType(Message::RecapType::NONE);
  TimePoint messageTime = UtilTime::makeTimePoint(UtilTime::divide(splittedLine.at(0)));
  message.setTime(messageTime);
  message.setTimeReceived(messageTime);
  message.setCorrelationIdList({"TRADE"});
  std::vector<Element> elementList;
  Element element;
  element.insert("LAST_PRICE", splittedLine.at(1));
  element.insert("LAST_SIZE", splittedLine.at(2));
  element.insert("IS_BUYER_MAKER", splittedLine.at(3));
  elementList.push_back(std::move(element));
  message.setElementList(elementList);
  return message;
}

Message createMarketDataMessageMarketDepth(const std::vector<std::string>& splittedLine) {
  Message message;
  message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
  message.setRecapType(Message::RecapType::NONE);
  TimePoint messageTime = UtilTime::makeTimePoint(std::make_pair(std::stol(splittedLine.at(0)), 0));
  message.setTime(messageTime);
  message.setTimeReceived(messageTime);
  message.setCorrelationIdList({"MARKET_DEPTH"});
  std::vector<Element> elementList;
  Element element;
  if (!splittedLine.at(1).empty()) {
    auto priceSize = UtilString::split(splittedLine.at(1), "_");
    element.insert("BID_PRICE", priceSize.at(0));
    element.insert("BID_SIZE", priceSize.at(1));
  }
  if (!splittedLine.at(2).empty()) {
    auto priceSize = UtilString::split(splittedLine.at(2), "_");
    element.insert("ASK_PRICE", priceSize.at(0));
    element.insert("ASK_SIZE", priceSize.at(1));
  }
  elementList.push_back(std::move(element));
  message.setElementList(elementList);
  return message;
}

TEST_F(HistoricalMarketDataEventProcessorTest, processEvent) {
  this->historicalMarketDataEventProcessor->processEvent();
  std::vector<Message> expectedMessageList;
  std::map<std::pair<long long, long long>, std::vector<Message> > expectedMessageListByTimeMap;
  auto currentDateTp = this->historicalMarketDataEventProcessor->startDateTp;
  int previousSecondsMarketDepth = 0;
  std::vector<std::string> previousSplittedMarketDepth;
  while (currentDateTp < this->historicalMarketDataEventProcessor->endDateTp) {
    int currentSeconds;
    auto currentDateISO = UtilTime::getISOTimestamp(currentDateTp, "%F");
    std::string fileNameWithDirBase = this->historicalMarketDataEventProcessor->historicalMarketDataDirectory + "/" +
                                      this->historicalMarketDataEventProcessor->exchange + "__" + this->historicalMarketDataEventProcessor->baseAsset + "-" +
                                      this->historicalMarketDataEventProcessor->quoteAsset + "__" + currentDateISO + "__";
    std::ifstream fMarketDepth;
    std::ifstream fTrade;
    fMarketDepth.open(fileNameWithDirBase + "market-depth.csv");
    fTrade.open(fileNameWithDirBase + "trade.csv");
    if (fMarketDepth && fTrade) {
      fMarketDepth.ignore(INT_MAX, '\n');
      fTrade.ignore(INT_MAX, '\n');
      std::string lineMarketDepth;
      while (std::getline(fMarketDepth, lineMarketDepth) && !lineMarketDepth.empty()) {
        auto splittedMarketDepth = UtilString::split(lineMarketDepth, ",");
        int currentSecondsMarketDepth = std::stoi(splittedMarketDepth.at(0));
        if (previousSecondsMarketDepth > 0) {
          currentSeconds += this->historicalMarketDataEventProcessor->clockStepSeconds;
          while (currentSeconds < currentSecondsMarketDepth) {
            previousSplittedMarketDepth[0] = std::to_string(currentSeconds);
            auto message = createMarketDataMessageMarketDepth(previousSplittedMarketDepth);
            expectedMessageListByTimeMap[std::make_pair(currentSeconds, 0)].push_back(std::move(message));
            currentSeconds += this->historicalMarketDataEventProcessor->clockStepSeconds;
          }
        }
        auto message = createMarketDataMessageMarketDepth(splittedMarketDepth);
        expectedMessageListByTimeMap[std::make_pair(currentSecondsMarketDepth, 0)].push_back(std::move(message));
        previousSecondsMarketDepth = currentSecondsMarketDepth;
        previousSplittedMarketDepth = std::move(splittedMarketDepth);
        currentSeconds = previousSecondsMarketDepth;
      }
      currentSeconds += this->historicalMarketDataEventProcessor->clockStepSeconds;
      while (currentSeconds < std::chrono::duration_cast<std::chrono::seconds>((currentDateTp + std::chrono::hours(24)).time_since_epoch()).count()) {
        previousSplittedMarketDepth[0] = std::to_string(currentSeconds);
        auto message = createMarketDataMessageMarketDepth(previousSplittedMarketDepth);
        expectedMessageListByTimeMap[std::make_pair(currentSeconds, 0)].push_back(std::move(message));
        currentSeconds += this->historicalMarketDataEventProcessor->clockStepSeconds;
      }
      currentSeconds -= this->historicalMarketDataEventProcessor->clockStepSeconds;
      std::string lineTrade;
      while (std::getline(fTrade, lineTrade) && !lineTrade.empty()) {
        auto splittedTrade = UtilString::split(lineTrade, ",");
        auto messageTimePair = UtilTime::divide(splittedTrade.at(0));
        auto message = createMarketDataMessageTrade(splittedTrade, this->historicalMarketDataEventProcessor->exchange);
        expectedMessageListByTimeMap[messageTimePair].push_back(std::move(message));
      }
    }
    for (auto& kv : expectedMessageListByTimeMap) {
      for (auto& message : kv.second) {
        expectedMessageList.push_back(std::move(message));
      }
    }
    expectedMessageListByTimeMap.clear();
    currentDateTp += std::chrono::hours(24);
  }
  for (int i = 0; i < expectedMessageList.size(); ++i) {
    auto messageList = this->eventList.at(i).getMessageList();
    EXPECT_EQ(messageList.size(), 1);
    auto& message = messageList.at(0);
    auto& expectedMessage = expectedMessageList.at(i);
    EXPECT_EQ(message.getType(), expectedMessage.getType());
    EXPECT_EQ(message.getRecapType(), expectedMessage.getRecapType());
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(message.getTime().time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedMessage.getTime().time_since_epoch()).count());
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(message.getTimeReceived().time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedMessage.getTimeReceived().time_since_epoch()).count());
    auto& correlationIdList = message.getCorrelationIdList();
    auto& expectedCorrelationIdList = expectedMessage.getCorrelationIdList();
    EXPECT_EQ(correlationIdList.size(), expectedCorrelationIdList.size());
    for (int j = 0; j < expectedCorrelationIdList.size(); ++j) {
      EXPECT_EQ(correlationIdList.at(j), expectedCorrelationIdList.at(j));
    }
    auto& elementList = message.getElementList();
    auto& exptectedElementList = expectedMessage.getElementList();
    EXPECT_EQ(elementList.size(), exptectedElementList.size());
    for (int j = 0; j < exptectedElementList.size(); ++j) {
      auto& nameValueMap = elementList.at(j).getNameValueMap();
      auto& expectedNameValueMap = exptectedElementList.at(j).getNameValueMap();
      EXPECT_EQ(nameValueMap.size(), expectedNameValueMap.size());
      for (auto it1 = nameValueMap.cbegin(), end1 = nameValueMap.cend(), it2 = expectedNameValueMap.cbegin(), end2 = expectedNameValueMap.cend();
           it1 != end1 || it2 != end2;) {
        if (it1 != end1 && it2 != end2) {
          EXPECT_EQ(it1->first, it2->first);
          EXPECT_EQ(it1->second, it2->second);
          ++it1;
          ++it2;
        }
      }
    }
  }
}
} /* namespace ccapi */
