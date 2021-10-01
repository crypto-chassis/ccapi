#include "app/spot_market_making_event_handler.h"
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
using ::ccapi::Logger;
using ::ccapi::Message;
using ::ccapi::Queue;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::SpotMarketMakingEventHandler;
using ::ccapi::Subscription;
using ::ccapi::UtilString;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
int main(int argc, char** argv) {
  std::string exchange = UtilSystem::getEnvAsString("EXCHANGE");
  std::string instrumentRest = UtilSystem::getEnvAsString("INSTRUMENT");
  std::string instrumentWebsocket = instrumentRest;
  SpotMarketMakingEventHandler eventHandler;
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
  eventHandler.accountBalanceRefreshWaitSeconds = UtilSystem::getEnvAsInt("ACCOUNT_BALANCE_REFRESH_WAIT_SECONDS");
  eventHandler.accountId = UtilSystem::getEnvAsString("ACCOUNT_ID");
  eventHandler.privateDataDirectory = UtilSystem::getEnvAsString("PRIVATE_DATA_DIRECTORY");
  eventHandler.privateDataFilePrefix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_PREFIX");
  eventHandler.privateDataFileSuffix = UtilSystem::getEnvAsString("PRIVATE_DATA_FILE_SUFFIX");
  eventHandler.privateDataOnlySaveFinalSummary = UtilString::toLower(UtilSystem::getEnvAsString("PRIVATE_DATA_ONLY_SAVE_FINAL_SUMMARY")) == "true";
  eventHandler.killSwitchMaximumDrawdown = UtilSystem::getEnvAsDouble("KILL_SWITCH_MAXIMUM_DRAWDOWN");
  eventHandler.clockStepSeconds = UtilSystem::getEnvAsInt("CLOCK_STEP_SECONDS", 1);
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
    eventHandler.adverseSelectionGuardActionType = SpotMarketMakingEventHandler::AdverseSelectionGuardActionType::TAKE;
  } else if (adverseSelectionGuardActionType == "make") {
    eventHandler.adverseSelectionGuardActionType = SpotMarketMakingEventHandler::AdverseSelectionGuardActionType::MAKE;
  } else {
    eventHandler.adverseSelectionGuardActionType = SpotMarketMakingEventHandler::AdverseSelectionGuardActionType::NONE;
  }
  eventHandler.adverseSelectionGuardActionOrderQuantityProportion = UtilSystem::getEnvAsDouble("ADVERSE_SELECTION_GUARD_ACTION_ORDER_QUANTITY_PROPORTION");
  eventHandler.adverseSelectionGuardActionOrderQuantityProportionRelativeToOneAsset =
      UtilString::toLower(UtilSystem::getEnvAsString("ADVERSE_SELECTION_GUARD_ACTION_ORDER_QUANTITY_PROPORTION_RELATIVE_TO_ONE_ASSET")) == "true";
  eventHandler.adverseSelectionGuardActionOrderRefreshIntervalSeconds =
      UtilSystem::getEnvAsInt("ADVERSE_SELECTION_GUARD_ACTION_ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.baseAsset = UtilSystem::getEnvAsString("BASE_ASSET_OVERRIDE");
  eventHandler.quoteAsset = UtilSystem::getEnvAsString("QUOTE_ASSET_OVERRIDE");
  eventHandler.orderPriceIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_PRICE_INCREMENT_OVERRIDE"));
  eventHandler.orderQuantityIncrement = UtilString::normalizeDecimalString(UtilSystem::getEnvAsString("ORDER_QUANTITY_INCREMENT_OVERRIDE"));
  std::string tradingMode = UtilSystem::getEnvAsString("TRADING_MODE");
  APP_LOGGER_INFO("******** Trading mode is " + tradingMode + "! ********");
  if (tradingMode == "paper") {
    eventHandler.tradingMode = SpotMarketMakingEventHandler::TradingMode::PAPER;
  } else if (tradingMode == "backtest") {
    eventHandler.tradingMode = SpotMarketMakingEventHandler::TradingMode::BACKTEST;
  }
  if (eventHandler.tradingMode == SpotMarketMakingEventHandler::TradingMode::PAPER ||
      eventHandler.tradingMode == SpotMarketMakingEventHandler::TradingMode::BACKTEST) {
    eventHandler.makerFee = UtilSystem::getEnvAsDouble("MAKER_FEE");
    eventHandler.makerBuyerFeeAsset = UtilSystem::getEnvAsString("MAKER_BUYER_FEE_ASSET");
    eventHandler.makerSellerFeeAsset = UtilSystem::getEnvAsString("MAKER_SELLER_FEE_ASSET");
    eventHandler.baseBalance = UtilSystem::getEnvAsDouble("INITIAL_BASE_BALANCE");
    eventHandler.quoteBalance = UtilSystem::getEnvAsDouble("INITIAL_QUOTE_BALANCE");
  }
  if (eventHandler.tradingMode == SpotMarketMakingEventHandler::TradingMode::BACKTEST) {
    eventHandler.startDateTp = UtilTime::parse(UtilSystem::getEnvAsString("START_DATE"), "%F");
    eventHandler.endDateTp = UtilTime::parse(UtilSystem::getEnvAsString("END_DATE"), "%F");
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
  if (eventHandler.tradingMode == SpotMarketMakingEventHandler::TradingMode::BACKTEST && !eventHandler.baseAsset.empty() && !eventHandler.quoteAsset.empty() &&
      !eventHandler.orderPriceIncrement.empty() && !eventHandler.orderQuantityIncrement.empty()) {
    Event virtualEvent;
    Message message;
    message.setTime(eventHandler.startDateTp);
    message.setTimeReceived(eventHandler.startDateTp);
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
