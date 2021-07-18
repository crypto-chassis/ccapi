#include "app/spot_market_making_event_handler.h"
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.

} /* namespace ccapi */
using ::ccapi::AppLogger;
using ::ccapi::CcapiLogger;
using ::ccapi::CsvWriter;
using ::ccapi::Logger;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::SpotMarketMakingEventHandler;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
int main(int argc, char** argv) {
  AppLogger appLogger;
  CcapiLogger ccapiLogger(&appLogger);
  Logger::logger = &ccapiLogger;
  std::string saveCsvDirectory = UtilSystem::getEnvAsString("SAVE_CSV_DIRECTORY");
  std::string nowISO = UtilTime::getISOTimestamp(UtilTime::now());
  std::string privateTradeCsvFilename(nowISO + "__private_trade.csv"), accountBalanceCsvFilename(nowISO + "__account_balance.csv");
  if (!saveCsvDirectory.empty()) {
    privateTradeCsvFilename = saveCsvDirectory + "/" + privateTradeCsvFilename;
    accountBalanceCsvFilename = saveCsvDirectory + "/" + accountBalanceCsvFilename;
  }
  CsvWriter privateTradeCsvWriter(privateTradeCsvFilename), accountBalanceCsvWriter(accountBalanceCsvFilename);
  privateTradeCsvWriter.writeRow({"timestamp","price", "size",
  "side", "is_maker", "fee_quantity", "fee_asset",});
  accountBalanceCsvWriter.writeRow({"timestamp", "base_asset_balance", "quote_asset_balance", "mid_price",});

  SpotMarketMakingEventHandler eventHandler(&appLogger, &privateTradeCsvWriter, &accountBalanceCsvWriter);
  eventHandler.exchange = UtilSystem::getEnvAsString("EXCHANGE");
  eventHandler.instrument = UtilSystem::getEnvAsString("INSTRUMENT");
  eventHandler.baseAvailableBalanceProportion = UtilSystem::getEnvAsDouble("BASE_AVAILABLE_BALANCE_PROPORTION");
  eventHandler.quoteAvailableBalanceProportion = UtilSystem::getEnvAsDouble("QUOTE_AVAILABLE_BALANCE_PROPORTION");
  // TODO(cryptochassis): research using inventoryBasePortionMinimum as a soft limit rather than a hard limit. Currently always 0.
  eventHandler.inventoryBasePortionMinimum = 0;
  double a = UtilSystem::getEnvAsDouble("INVENTORY_BASE_QUOTE_RATIO_TARGET");
  eventHandler.inventoryBasePortionTarget = a / (a + 1);
  // TODO(cryptochassis): research using inventoryBasePortionMaximum as a soft limit rather than a hard limit. Currently always 1.
  eventHandler.inventoryBasePortionMaximum = 1;

  eventHandler.halfSpreadMinimum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MINIMUM") / 2;
  eventHandler.halfSpreadMaximum = UtilSystem::getEnvAsDouble("SPREAD_PROPORTION_MAXIMUM") / 2;
  eventHandler.orderQuantityProportion = UtilSystem::getEnvAsDouble("ORDER_QUANTITY_PROPORTION");
  // eventHandler.orderQuantity = UtilSystem::getEnvAsString("ORDER_QUANTITY");
  eventHandler.orderRefreshIntervalSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_SECONDS");
  eventHandler.orderRefreshIntervalOffsetSeconds = UtilSystem::getEnvAsInt("ORDER_REFRESH_INTERVAL_OFFSET_SECONDS");
  eventHandler.accountBalanceRefreshWaitSeconds = UtilSystem::getEnvAsInt("ACCOUNT_BALANCE_REFRESH_WAIT_SECONDS");
  eventHandler.accountId = UtilSystem::getEnvAsString("ACCOUNT_ID");
  if (eventHandler.exchange == "coinbase") {
    eventHandler.useGetAccountsToGetAccountBalances = true;
  }

  SessionOptions sessionOptions;
  sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds = 1 + eventHandler.accountBalanceRefreshWaitSeconds;
  SessionConfigs sessionConfigs;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Request request(Request::Operation::GET_INSTRUMENT, eventHandler.exchange, eventHandler.instrument, "GET_INSTRUMENT");
  session.sendRequest(request);

  // Subscription subscription(eventHandler.exchange, eventHandler.instrument, "MARKET_DEPTH",
  //                           "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
  // session.subscribe(subscription);
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(INT_MAX));
  }
  return EXIT_SUCCESS;
}
// if too many errors per minute or loss is large then quick_exit which can register hooks
