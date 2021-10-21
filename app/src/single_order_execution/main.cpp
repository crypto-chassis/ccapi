#include "app/event_handler_base.h"
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
AppLogger appLogger;
AppLogger* AppLogger::logger = &appLogger;
CcapiLogger ccapiLogger;
Logger* Logger::logger = &ccapiLogger;
} /* namespace ccapi */
using ::ccapi::AppLogger;
using ::ccapi::CcapiLogger;
using ::ccapi::Element;
using ::ccapi::Event;
using ::ccapi::EventHandlerBase;
using ::ccapi::Logger;
using ::ccapi::Message;
using ::ccapi::Queue;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilString;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
int main(int argc, char** argv) {
  auto now = UtilTime::now();
  std::string exchange = UtilSystem::getEnvAsString("EXCHANGE");
  std::string instrumentRest = UtilSystem::getEnvAsString("INSTRUMENT");
  std::string instrumentWebsocket = instrumentRest;
  EventHandlerBase eventHandler;
  eventHandler.exchange = exchange;
  eventHandler.instrumentRest = instrumentRest;
  eventHandler.instrumentWebsocket = instrumentWebsocket;
  eventHandler.orderRefreshIntervalSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.privateDataDirectory = UtilSystem::getEnvAsString("PRIVATE_DATA_DIRECTORY");
  eventHandler.privateDataFilePrefix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_PREFIX");
  eventHandler.privateDataFileSuffix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_SUFFIX");
  eventHandler.privateDataOnlySaveFinalSummary = UtilString::toLower(UtilSystem::getEnvAsString("PRIVATE_DATA_ONLY_SAVE_FINAL_SUMMARY")) == "true";
  eventHandler.clockStepSeconds = UtilSystem::getEnvAsInt("CLOCK_STEP_SECONDS", 1);
  eventHandler.baseAsset = UtilSystem::getEnvAsString("BASE_ASSET_OVERRIDE");
  eventHandler.quoteAsset = UtilSystem::getEnvAsString("QUOTE_ASSET_OVERRIDE");
  eventHandler.orderPriceIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_PRICE_INCREMENT_OVERRIDE"));
  eventHandler.orderQuantityIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_QUANTITY_INCREMENT_OVERRIDE"));
  std::string startTimeStr = UtilSystem::getEnvAsString("START_TIME");
  eventHandler.startTimeTp = startTimeStr.empty() ? now : UtilTime::parse(startTimeStr);
  std::string totalDurationSecondsStr = UtilSystem::getEnvAsString("TOTAL_DURATION_SECONDS");
  eventHandler.totalDurationSeconds = std::stoi(totalDurationSecondsStr);
  eventHandler.numOrderRefreshIntervals = eventHandler.totalDurationSeconds / eventHandler.orderRefreshIntervalSeconds;
  eventHandler.orderSide = UtilString::toUpper(UtilSystem::getEnvAsString("ORDER_SIDE"));
  eventHandler.totalTargetQuantity = UtilSystem::getEnvAsDouble("TOTAL_TARGET_QUANTITY");
  eventHandler.totalTargetQuantityInQuote = UtilSystem::getEnvAsDouble("TOTAL_TARGET_QUANTITY_IN_QUOTE");
  eventHandler.theoreticalRemainingQuantity = eventHandler.totalTargetQuantity;
  eventHandler.theoreticalRemainingQuantityInQuote = eventHandler.totalTargetQuantityInQuote;
  eventHandler.orderPriceLimit = UtilSystem::getEnvAsDouble("ORDER_PRICE_LIMIT");
  eventHandler.orderPriceLimitRelativeToMidPrice = UtilSystem::getEnvAsDouble("ORDER_PRICE_LIMIT_RELATIVE_TO_MID_PRICE");
  eventHandler.orderQuantityLimitRelativeToTarget = UtilSystem::getEnvAsDouble("ORDER_QUANTITY_LIMIT_RELATIVE_TO_TARGET");
  eventHandler.twapOrderQuantityRandomizationMax = UtilSystem::getEnvAsDouble("TWAP_ORDER_QUANTITY_RANDOMIZATION_MAX");
  eventHandler.povOrderQuantityParticipationRate = UtilSystem::getEnvAsDouble("POV_ORDER_QUANTITY_PARTICIPATION_RATE");
  eventHandler.isKapa = UtilSystem::getEnvAsDouble("IS_URGENCY");
  int timeToSleepSeconds = std::chrono::duration_cast<std::chrono::seconds>(eventHandler.startTimeTp - now).count();
  if (timeToSleepSeconds > 0) {
    APP_LOGGER_INFO("About to sleep " + std::to_string(timeToSleepSeconds) + " seconds.");
    std::this_thread::sleep_for(std::chrono::seconds(timeToSleepSeconds));
  }
  eventHandler.appMode=EventHandlerBase::AppMode::SINGLE_ORDER_EXECUTION;
  std::string tradingMode = UtilSystem::getEnvAsString("TRADING_MODE");
  APP_LOGGER_INFO("******** Trading mode is " + tradingMode + "! ********");
  if (tradingMode == "paper") {
    eventHandler.tradingMode = EventHandlerBase::TradingMode::PAPER;
  } else if (tradingMode == "backtest") {
    eventHandler.tradingMode = EventHandlerBase::TradingMode::BACKTEST;
  }
  if (eventHandler.tradingMode == EventHandlerBase::TradingMode::PAPER || eventHandler.tradingMode == EventHandlerBase::TradingMode::BACKTEST) {
    eventHandler.makerFee = UtilSystem::getEnvAsDouble("MAKER_FEE");
    eventHandler.makerBuyerFeeAsset = UtilSystem::getEnvAsString("MAKER_BUYER_FEE_ASSET");
    eventHandler.makerSellerFeeAsset = UtilSystem::getEnvAsString("MAKER_SELLER_FEE_ASSET");
    eventHandler.takerFee = UtilSystem::getEnvAsDouble("TAKER_FEE");
    eventHandler.takerBuyerFeeAsset = UtilSystem::getEnvAsString("TAKER_BUYER_FEE_ASSET");
    eventHandler.takerSellerFeeAsset = UtilSystem::getEnvAsString("TAKER_SELLER_FEE_ASSET");
    eventHandler.baseBalance = UtilSystem::getEnvAsDouble("INITIAL_BASE_BALANCE") * eventHandler.baseAvailableBalanceProportion;
    eventHandler.quoteBalance = UtilSystem::getEnvAsDouble("INITIAL_QUOTE_BALANCE") * eventHandler.quoteAvailableBalanceProportion;
  }
  if (eventHandler.tradingMode == EventHandlerBase::TradingMode::BACKTEST) {
    eventHandler.historicalMarketDataStartDateTp = UtilTime::parse(UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_START_DATE"), "%F");
    if (startTimeStr.empty()) {
      eventHandler.startTimeTp = eventHandler.historicalMarketDataStartDateTp;
    }
    eventHandler.historicalMarketDataEndDateTp = UtilTime::parse(UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_END_DATE"), "%F");
    if (totalDurationSecondsStr.empty()) {
      eventHandler.totalDurationSeconds =
          std::chrono::duration_cast<std::chrono::seconds>(eventHandler.historicalMarketDataEndDateTp - eventHandler.historicalMarketDataStartDateTp).count();
    }
    eventHandler.historicalMarketDataDirectory = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_DIRECTORY");
    eventHandler.historicalMarketDataFilePrefix = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_FILE_PREFIX");
    eventHandler.historicalMarketDataFileSuffix = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_FILE_SUFFIX");
  }
  std::string tradingStrategy = UtilSystem::getEnvAsString("TRADING_STRATEGY");
  APP_LOGGER_INFO("******** Trading strategy is " + tradingStrategy + "! ********");
  if (tradingStrategy == "twap") {
    eventHandler.tradingStrategy = EventHandlerBase::TradingStrategy::TWAP;
  } else if (tradingStrategy == "vwap") {
    eventHandler.tradingStrategy = EventHandlerBase::TradingStrategy::VWAP;
  } else if (tradingStrategy == "pov") {
    eventHandler.tradingStrategy = EventHandlerBase::TradingStrategy::POV;
  } else if (tradingStrategy == "is") {
    eventHandler.tradingStrategy = EventHandlerBase::TradingStrategy::IS;
  }
  std::set<std::string> useGetAccountsToGetAccountBalancesExchangeSet{"coinbase", "kucoin"};
  if (useGetAccountsToGetAccountBalancesExchangeSet.find(eventHandler.exchange) != useGetAccountsToGetAccountBalancesExchangeSet.end()) {
    eventHandler.useGetAccountsToGetAccountBalances = true;
  }
  std::shared_ptr<std::promise<void>> promisePtr(new std::promise<void>());
  eventHandler.promisePtr = promisePtr;
  SessionOptions sessionOptions;
  sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds = 1 + eventHandler.accountBalanceRefreshWaitSeconds;
  SessionConfigs sessionConfigs;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  if (exchange == "kraken") {
    Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken", "", "Get Instrument Symbol For Websocket");
    request.appendParam({
        {"HTTP_METHOD", "GET"},
        {"HTTP_PATH", "/0/public/AssetPairs"},
        {"HTTP_QUERY_STRING", "pair=" + instrumentRest},
    });
    Queue<Event> eventQueue;
    session.sendRequest(request, &eventQueue);
    std::vector<Event> eventList = eventQueue.purge();
    for (const auto& event : eventList) {
      if (event.getType() == Event::Type::RESPONSE) {
        rj::Document document;
        document.Parse(event.getMessageList().at(0).getElementList().at(0).getValue("HTTP_BODY").c_str());
        eventHandler.instrumentWebsocket = document["result"][instrumentRest.c_str()]["wsname"].GetString();
        break;
      }
    }
  } else if (exchange.rfind("binance", 0) == 0) {
    eventHandler.instrumentWebsocket = UtilString::toLower(instrumentRest);
  }
  Request request(Request::Operation::GET_INSTRUMENT, eventHandler.exchange, eventHandler.instrumentRest, "GET_INSTRUMENT");
  if (eventHandler.tradingMode == EventHandlerBase::TradingMode::BACKTEST && !eventHandler.baseAsset.empty() && !eventHandler.quoteAsset.empty() &&
      !eventHandler.orderPriceIncrement.empty() && !eventHandler.orderQuantityIncrement.empty()) {
    Event virtualEvent;
    Message message;
    message.setTime(eventHandler.startTimeTp);
    message.setTimeReceived(eventHandler.startTimeTp);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    virtualEvent.setType(Event::Type::RESPONSE);
    message.setType(Message::Type::GET_INSTRUMENT);
    Element element;
    element.insert("BASE_ASSET", eventHandler.baseAsset);
    element.insert("QUOTE_ASSET", eventHandler.quoteAsset);
    element.insert("PRICE_INCREMENT", eventHandler.orderPriceIncrement);
    element.insert("QUANTITY_INCREMENT", eventHandler.orderQuantityIncrement);
    elementList.emplace_back(std::move(element));
    message.setElementList(elementList);
    virtualEvent.setMessageList({message});
    eventHandler.processEvent(virtualEvent, &session);
  } else {
    session.sendRequest(request);
  }
  promisePtr->get_future().wait();
  session.stop();
  return EXIT_SUCCESS;
}
