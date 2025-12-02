#ifndef INCLUDE_CCAPI_LTP_TRADING_INTERFACE_H_
#define INCLUDE_CCAPI_LTP_TRADING_INTERFACE_H_

#include "ltp_trading_interface.h"
#include "ccapi_cpp/ccapi_macro.h"

namespace ltp {

inline std::string exchangeToString(LTPExchange exchange) {
  switch (exchange) {
    case LTPExchange::BINANCE:
      return CCAPI_EXCHANGE_NAME_BINANCE;
    case LTPExchange::BINANCE_USDS_FUTURES:
      return CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES;
    case LTPExchange::BINANCE_COIN_FUTURES:
      return CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES;
    case LTPExchange::BINANCE_PORTFOLIO_MARGIN:
      return CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN;
    case LTPExchange::OKX:
      return CCAPI_EXCHANGE_NAME_OKX;
    case LTPExchange::BYBIT:
      return CCAPI_EXCHANGE_NAME_BYBIT;
    case LTPExchange::HUOBI:
      return CCAPI_EXCHANGE_NAME_HUOBI;
    case LTPExchange::HUOBI_USDT_SWAP:
      return CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP;
    case LTPExchange::HUOBI_COIN_SWAP:
      return CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP;
    case LTPExchange::COINBASE:
      return CCAPI_EXCHANGE_NAME_COINBASE;
    case LTPExchange::KRAKEN:
      return CCAPI_EXCHANGE_NAME_KRAKEN;
    case LTPExchange::BITFINEX:
      return CCAPI_EXCHANGE_NAME_BITFINEX;
    case LTPExchange::BITMEX:
      return CCAPI_EXCHANGE_NAME_BITMEX;
    case LTPExchange::DERIBIT:
      return CCAPI_EXCHANGE_NAME_DERIBIT;
    case LTPExchange::GATEIO:
      return CCAPI_EXCHANGE_NAME_GATEIO;
    case LTPExchange::KUCOIN:
      return CCAPI_EXCHANGE_NAME_KUCOIN;
    case LTPExchange::MEXC:
      return CCAPI_EXCHANGE_NAME_MEXC;
    case LTPExchange::BITGET:
      return CCAPI_EXCHANGE_NAME_BITGET;
    case LTPExchange::CRYPTOCOM:
      return CCAPI_EXCHANGE_NAME_CRYPTOCOM;
    default:
      return CCAPI_UNKNOWN;
  }
}

inline LTPExchange stringToExchange(const std::string& exchangeStr) {
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE) return LTPExchange::BINANCE;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES) return LTPExchange::BINANCE_USDS_FUTURES;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES) return LTPExchange::BINANCE_COIN_FUTURES;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN) return LTPExchange::BINANCE_PORTFOLIO_MARGIN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_OKX) return LTPExchange::OKX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BYBIT) return LTPExchange::BYBIT;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI) return LTPExchange::HUOBI;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP) return LTPExchange::HUOBI_USDT_SWAP;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP) return LTPExchange::HUOBI_COIN_SWAP;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_COINBASE) return LTPExchange::COINBASE;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_KRAKEN) return LTPExchange::KRAKEN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITFINEX) return LTPExchange::BITFINEX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITMEX) return LTPExchange::BITMEX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_DERIBIT) return LTPExchange::DERIBIT;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_GATEIO) return LTPExchange::GATEIO;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_KUCOIN) return LTPExchange::KUCOIN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_MEXC) return LTPExchange::MEXC;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITGET) return LTPExchange::BITGET;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_CRYPTOCOM) return LTPExchange::CRYPTOCOM;
  return LTPExchange::UNKNOWN;
}

inline std::string orderSideToString(LTPOrderSide side) {
  switch (side) {
    case LTPOrderSide::BUY:
      return CCAPI_EM_ORDER_SIDE_BUY;
    case LTPOrderSide::SELL:
      return CCAPI_EM_ORDER_SIDE_SELL;
    default:
      return CCAPI_UNKNOWN;
  }
}

inline LTPOrderSide stringToOrderSide(const std::string& sideStr) {
  if (sideStr == CCAPI_EM_ORDER_SIDE_BUY || sideStr == "BUY" || sideStr == "buy") {
    return LTPOrderSide::BUY;
  }
  if (sideStr == CCAPI_EM_ORDER_SIDE_SELL || sideStr == "SELL" || sideStr == "sell") {
    return LTPOrderSide::SELL;
  }
  return LTPOrderSide::UNKNOWN;
}

inline std::string orderTypeToString(LTPOrderType type) {
  switch (type) {
    case LTPOrderType::LIMIT:
      return "LIMIT";
    case LTPOrderType::MARKET:
      return "MARKET";
    case LTPOrderType::STOP_LOSS:
      return "STOP_LOSS";
    case LTPOrderType::STOP_LOSS_LIMIT:
      return "STOP_LOSS_LIMIT";
    case LTPOrderType::TAKE_PROFIT:
      return "TAKE_PROFIT";
    case LTPOrderType::TAKE_PROFIT_LIMIT:
      return "TAKE_PROFIT_LIMIT";
    case LTPOrderType::LIMIT_MAKER:
      return "LIMIT_MAKER";
    default:
      return CCAPI_UNKNOWN;
  }
}

inline std::string timeInForceToString(LTPTimeInForce tif) {
  switch (tif) {
    case LTPTimeInForce::GTC:
      return "GTC";
    case LTPTimeInForce::IOC:
      return "IOC";
    case LTPTimeInForce::FOK:
      return "FOK";
    case LTPTimeInForce::GTX:
      return "GTX";
    default:
      return "GTC";  // 默认GTC
  }
}

inline LTPOrderType stringToOrderType(const std::string& typeStr) {
  if (typeStr == "LIMIT") return LTPOrderType::LIMIT;
  if (typeStr == "MARKET") return LTPOrderType::MARKET;
  if (typeStr == "STOP_LOSS" || typeStr == "STOP") return LTPOrderType::STOP_LOSS;
  if (typeStr == "STOP_LOSS_LIMIT") return LTPOrderType::STOP_LOSS_LIMIT;
  if (typeStr == "TAKE_PROFIT") return LTPOrderType::TAKE_PROFIT;
  if (typeStr == "TAKE_PROFIT_LIMIT") return LTPOrderType::TAKE_PROFIT_LIMIT;
  if (typeStr == "LIMIT_MAKER") return LTPOrderType::LIMIT_MAKER;
  return LTPOrderType::UNKNOWN;
}

inline LTPOrderStatus stringToOrderStatus(const std::string& statusStr) {
  // 币安状态
  if (statusStr == "NEW" || statusStr == "PENDING") return LTPOrderStatus::NEW;
  if (statusStr == "PARTIALLY_FILLED" || statusStr == "PARTIAL_FILL") return LTPOrderStatus::PARTIALLY_FILLED;
  if (statusStr == "FILLED" || statusStr == "FILL") return LTPOrderStatus::FILLED;
  if (statusStr == "CANCELED" || statusStr == "CANCELLED") return LTPOrderStatus::CANCELED;
  if (statusStr == "PENDING_CANCEL") return LTPOrderStatus::PENDING_CANCEL;
  if (statusStr == "REJECTED" || statusStr == "REJECT") return LTPOrderStatus::REJECTED;
  if (statusStr == "EXPIRED" || statusStr == "EXPIRE") return LTPOrderStatus::EXPIRED;

  // OKX状态
  if (statusStr == "live") return LTPOrderStatus::NEW;
  if (statusStr == "partially_filled") return LTPOrderStatus::PARTIALLY_FILLED;
  if (statusStr == "filled") return LTPOrderStatus::FILLED;
  if (statusStr == "canceled") return LTPOrderStatus::CANCELED;
  if (statusStr == "mmp_canceled") return LTPOrderStatus::CANCELED;

  return LTPOrderStatus::UNKNOWN;
}

inline LTPTimeInForce stringToTimeInForce(const std::string& tifStr) {
  if (tifStr == "GTC") return LTPTimeInForce::GTC;
  if (tifStr == "IOC") return LTPTimeInForce::IOC;
  if (tifStr == "FOK") return LTPTimeInForce::FOK;
  if (tifStr == "GTX") return LTPTimeInForce::GTX;
  return LTPTimeInForce::UNKNOWN;
}

} /* namespace ltp */

#endif  // INCLUDE_CCAPI_LTP_TRADING_INTERFACE_H_