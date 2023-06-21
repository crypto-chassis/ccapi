#ifdef CCAPI_APP_USE_CUSTOM_EVENT_HANDLER
#include "custom_event_handler.h"
#else
#include "app/event_handler_base.h"
#endif
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
#ifdef CCAPI_APP_USE_CUSTOM_EVENT_HANDLER
using ::ccapi::CustomEventHandler;
#endif
using ::ccapi::EventHandlerBase;
using ::ccapi::Logger;
using ::ccapi::Message;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::Subscription;
using ::ccapi::UtilString;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
#ifndef CCAPI_APP_IS_BACKTEST
using ::ccapi::Queue;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
#endif
int main(int argc, char** argv) {
  auto now = UtilTime::now();
  std::string exchange = UtilSystem::getEnvAsString("EXCHANGE");
  std::string instrumentRest = UtilSystem::getEnvAsString("INSTRUMENT");
  std::string instrumentWebsocket = instrumentRest;
#ifdef CCAPI_APP_USE_CUSTOM_EVENT_HANDLER
  CustomEventHandler eventHandler;
#else
  EventHandlerBase eventHandler;
#endif
  eventHandler.exchange = exchange;
  eventHandler.instrumentRest = instrumentRest;
  eventHandler.instrumentWebsocket = instrumentWebsocket;
  eventHandler.baseAvailableBalanceProportion = UtilSystem::getEnvAsDouble("BASE_AVAILABLE_BALANCE_PROPORTION");
  eventHandler.quoteAvailableBalanceProportion = UtilSystem::getEnvAsDouble("QUOTE_AVAILABLE_BALANCE_PROPORTION");
  double a = UtilSystem::getEnvAsDouble("INVENTORY_BASE_QUOTE_RATIO_TARGET");
  eventHandler.inventoryBasePortionTarget = a / (a + 1);
  eventHandler.useWeightedMidPrice = UtilString::toLower(UtilSystem::getEnvAsString("USE_WEIGHTED_MID_PRICE")) == "true";
  eventHandler.halfSpreadMinimum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MINIMUM") / 2;
  eventHandler.halfSpreadMaximum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MAXIMUM") / 2;
  eventHandler.orderQuantityProportion = UtilSystem::getEnvAsDouble("ORDER_QUANTITY_PROPORTION");
  eventHandler.originalOrderRefreshIntervalSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.orderRefreshIntervalSeconds = eventHandler.originalOrderRefreshIntervalSeconds;
  eventHandler.orderRefreshIntervalOffsetSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_OFFSET_SECONDS") % eventHandler.orderRefreshIntervalSeconds;
  eventHandler.immediatelyPlaceNewOrders = UtilString::toLower(UtilSystem::getEnvAsString("IMMEDIATELY_PLACE_NEW_ORDERS")) == "true";
  eventHandler.enableUpdateOrderBookTickByTick = UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_UPDATE_ORDER_BOOK_TICK_BY_TICK")) == "true";
  eventHandler.accountBalanceRefreshWaitSeconds = UtilSystem::getEnvAsInt("ACCOUNT_BALANCE_REFRESH_WAIT_SECONDS");
  eventHandler.accountId = UtilSystem::getEnvAsString("ACCOUNT_ID");
  eventHandler.privateDataDirectory = UtilSystem::getEnvAsString("PRIVATE_DATA_DIRECTORY");
  eventHandler.privateDataFilePrefix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_PREFIX");
  eventHandler.privateDataFileSuffix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_SUFFIX");
  eventHandler.privateDataOnlySaveFinalSummary = UtilString::toLower(UtilSystem::getEnvAsString("PRIVATE_DATA_ONLY_SAVE_FINAL_SUMMARY")) == "true";
  eventHandler.killSwitchMaximumDrawdown = UtilSystem::getEnvAsDouble("KILL_SWITCH_MAXIMUM_DRAWDOWN");
  eventHandler.clockStepMilliseconds = UtilSystem::getEnvAsInt("CLOCK_STEP_MILLISECONDS", 1000);
  eventHandler.enableAdverseSelectionGuard = UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD")) == "true";
  eventHandler.adverseSelectionGuardMarketDataSampleIntervalSeconds = UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_MARKET_DATA_SAMPLE_INTERVAL_SECONDS");
  eventHandler.adverseSelectionGuardMarketDataSampleBufferSizeSeconds =
      UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_MARKET_DATA_SAMPLE_BUFFER_SIZE_SECONDS");
  eventHandler.enableAdverseSelectionGuardByRoc = UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD_BY_ROC")) == "true";
  eventHandler.adverseSelectionGuardTriggerRocNumObservations = UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_TRIGGER_ROC_NUM_OBSERVATIONS");
  eventHandler.adverseSelectionGuardTriggerRocMinimum = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_ROC_MINIMUM");
  eventHandler.adverseSelectionGuardTriggerRocMaximum = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_ROC_MAXIMUM");
  eventHandler.adverseSelectionGuardTriggerRocOrderDirectionReverse =
      UtilString::toLower(UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_TRIGGER_ROC_ORDER_DIRECTION_REVERSE")) == "true";
  eventHandler.enableAdverseSelectionGuardByRsi = UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD_BY_RSI")) == "true";
  eventHandler.adverseSelectionGuardTriggerRsiNumObservations = UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_TRIGGER_RSI_NUM_OBSERVATIONS");
  eventHandler.adverseSelectionGuardTriggerRsiMinimum = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_RSI_MINIMUM");
  eventHandler.adverseSelectionGuardTriggerRsiMaximum = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_RSI_MAXIMUM");
  eventHandler.adverseSelectionGuardTriggerRsiOrderDirectionReverse =
      UtilString::toLower(UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_TRIGGER_RSI_ORDER_DIRECTION_REVERSE")) == "true";
  eventHandler.enableAdverseSelectionGuardByRollCorrelationCoefficient =
      UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD_BY_ROLL_CORRELATION_COEFFICIENT")) == "true";
  eventHandler.adverseSelectionGuardTriggerRollCorrelationCoefficientNumObservations =
      UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_TRIGGER_ROLL_CORRELATION_COEFFICIENT_NUM_OBSERVATIONS");
  eventHandler.adverseSelectionGuardTriggerRollCorrelationCoefficientMaximum =
      UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_ROLL_CORRELATION_COEFFICIENT_MAXIMUM");
  eventHandler.adverseSelectionGuardTriggerRollCorrelationCoefficientOrderDirectionReverse =
      UtilString::toLower(UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_TRIGGER_ROLL_CORRELATION_COEFFICIENT_ORDER_DIRECTION_REVERSE")) == "true";
  eventHandler.enableAdverseSelectionGuardByInventoryLimit =
      UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD_BY_INVENTORY_LIMIT")) == "true";
  a = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_INVENTORY_BASE_QUOTE_RATIO_MINIMUM");
  eventHandler.adverseSelectionGuardTriggerInventoryBasePortionMinimum = a / (a + 1);
  a = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_TRIGGER_INVENTORY_BASE_QUOTE_RATIO_MAXIMUM");
  eventHandler.adverseSelectionGuardTriggerInventoryBasePortionMaximum = a / (a + 1);
  eventHandler.enableAdverseSelectionGuardByInventoryDepletion =
      UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_ADVERSE_SELECTION_GUARD_BY_INVENTORY_DEPLETION")) == "true";
  std::string adverseSelectionGuardActionType = UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_ACTION_TYPE");
  if (adverseSelectionGuardActionType == "take") {
    eventHandler.adverseSelectionGuardActionType = EventHandlerBase::AdverseSelectionGuardActionType::TAKE;
  } else if (adverseSelectionGuardActionType == "make") {
    eventHandler.adverseSelectionGuardActionType = EventHandlerBase::AdverseSelectionGuardActionType::MAKE;
  } else {
    eventHandler.adverseSelectionGuardActionType = EventHandlerBase::AdverseSelectionGuardActionType::NONE;
  }
  eventHandler.adverseSelectionGuardActionOrderQuantityProportion = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_ACTION_ORDER_QUANTITY_PROPORTION");
  eventHandler.adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset =
      UtilString::toLower(UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_ACTION_ORDER_QUANTITY_PROPORTION_RELATIVE_TO_ONE_ASSET")) == "true";
  eventHandler.adverseSelectionGuardActionOrderRefreshIntervalSeconds =
      UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_ACTION_ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.enableMarketMaking = UtilString::toLower(UtilSystem::getEnvAsString("ENABLE_MARKET_MAKING", "true")) == "true";
  eventHandler.baseAsset = UtilSystem::getEnvAsString("BASE_ASSET_OVERRIDE");
  eventHandler.quoteAsset = UtilSystem::getEnvAsString("QUOTE_ASSET_OVERRIDE");
  eventHandler.orderPriceIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_PRICE_INCREMENT_OVERRIDE"));
  eventHandler.orderQuantityIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_QUANTITY_INCREMENT_OVERRIDE"));
  std::string startTimeStr = UtilSystem::getEnvAsString("START_TIME");
  eventHandler.startTimeTp = startTimeStr.empty() ? now : UtilTime::parse(startTimeStr);
  std::string totalDurationSecondsStr = UtilSystem::getEnvAsString("TOTAL_DURATION_SECONDS");
  eventHandler.totalDurationSeconds =
      (totalDurationSecondsStr.empty() || std::stoi(totalDurationSecondsStr) <= 0) ? INT_MAX : std::stoi(totalDurationSecondsStr);
  int timeToSleepSeconds = std::chrono::duration_cast<std::chrono::seconds>(eventHandler.startTimeTp - now).count();
  if (timeToSleepSeconds > 0) {
    APP_LOGGER_INFO("About to sleep " + std::to_string(timeToSleepSeconds) + " seconds.");
    std::this_thread::sleep_for(std::chrono::seconds(timeToSleepSeconds));
  }
  eventHandler.appMode = EventHandlerBase::AppMode::MARKET_MAKING;
#ifdef CCAPI_APP_IS_BACKTEST
  APP_LOGGER_INFO("CCAPI_APP_IS_BACKTEST is defined!");
#endif
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
    eventHandler.marketImpfactFactor = UtilSystem::getEnvAsDouble("MARKET_IMPACT_FACTOR");
  }
  if (eventHandler.tradingMode == EventHandlerBase::TradingMode::BACKTEST) {
    eventHandler.historicalMarketDataStartDateTp = UtilTime::parse(UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_START_DATE"));
    if (startTimeStr.empty()) {
      eventHandler.startTimeTp = eventHandler.historicalMarketDataStartDateTp;
    }
    eventHandler.historicalMarketDataEndDateTp = UtilTime::parse(UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_END_DATE"));
    if (totalDurationSecondsStr.empty()) {
      eventHandler.totalDurationSeconds =
          std::chrono::duration_cast<std::chrono::seconds>(eventHandler.historicalMarketDataEndDateTp - eventHandler.historicalMarketDataStartDateTp).count();
    }
    eventHandler.historicalMarketDataDirectory = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_DIRECTORY");
    eventHandler.historicalMarketDataFilePrefix = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_FILE_PREFIX");
    eventHandler.historicalMarketDataFileSuffix = UtilSystem::getEnvAsString("HISTORICAL_MARKET_DATA_FILE_SUFFIX");
  }
  std::set<std::string> useGetAccountsToGetAccountBalancesExchangeSet{"coinbase", "kucoin"};
  if (useGetAccountsToGetAccountBalancesExchangeSet.find(eventHandler.exchange) != useGetAccountsToGetAccountBalancesExchangeSet.end()) {
    eventHandler.useGetAccountsToGetAccountBalances = true;
  }
  std::shared_ptr<std::promise<void>> promisePtr(new std::promise<void>());
  eventHandler.promisePtr = promisePtr;
#ifndef CCAPI_APP_IS_BACKTEST
  SessionOptions sessionOptions;
  sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds = 1 + eventHandler.accountBalanceRefreshWaitSeconds;
  sessionOptions.httpMaxNumRetry = 0;
  sessionOptions.httpMaxNumRedirect = 0;
  SessionConfigs sessionConfigs;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  eventHandler.onInit(&session);
#else
  Session session;
#endif
  if (exchange == "kraken") {
#ifndef CCAPI_APP_IS_BACKTEST
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
#else
    eventHandler.instrumentWebsocket = UtilSystem::getEnvAsString("INSTRUMENT_WEBSOCKET");
#endif
  } else if (exchange.rfind("binance", 0) == 0) {
    eventHandler.instrumentWebsocket = UtilString::toLower(instrumentRest);
  }
  std::set<std::string> needSecondCredentialExchangeSet{"gemini", "kraken"};
  if (needSecondCredentialExchangeSet.find(eventHandler.exchange) != needSecondCredentialExchangeSet.end()) {
    const auto& exchangeUpper = UtilString::toUpper(eventHandler.exchange);
    eventHandler.credential_2 = {
        {exchangeUpper + "_API_KEY", UtilSystem::getEnvAsString(exchangeUpper + "_API_KEY")},
        {exchangeUpper + "_API_SECRET", UtilSystem::getEnvAsString(exchangeUpper + "_API_SECRET")},
    };
  }
  std::set<std::string> useCancelOrderToCancelOpenOrdersExchangeSet{"gemini", "kraken", "bitfinex", "okx"};
  if (useCancelOrderToCancelOpenOrdersExchangeSet.find(eventHandler.exchange) != useCancelOrderToCancelOpenOrdersExchangeSet.end()) {
    eventHandler.useCancelOrderToCancelOpenOrders = true;
  }
  std::set<std::string> useWebsocketToExecuteOrderExchangeSet{"bitfinex", "okx"};
  if (useWebsocketToExecuteOrderExchangeSet.find(eventHandler.exchange) != useWebsocketToExecuteOrderExchangeSet.end()) {
    eventHandler.useWebsocketToExecuteOrder = true;
  }
  Request request(Request::Operation::GET_INSTRUMENT, eventHandler.exchange, eventHandler.instrumentRest, "GET_INSTRUMENT");
  if (exchange == "okx") {
    request.appendParam({
        {"instType", "SPOT"},
    });
  }
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
