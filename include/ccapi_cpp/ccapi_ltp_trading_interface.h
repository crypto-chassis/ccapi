#ifndef INCLUDE_LTP_TRADING_INTERFACE_H_
#define INCLUDE_LTP_TRADING_INTERFACE_H_

#include <string>
#include <map>
#include <vector>
#include "ccapi_cpp/ccapi_macro.h"

namespace ltp {

/**
 * LTP交易接口 - 为客户提供跨交易所的统一下单和撤单接口
 *
 * 这个接口抽象了不同交易所的参数差异，客户只需要使用统一的参数结构
 * 就可以在任何支持的交易所进行交易操作
 */

// ====================================================================
// 枚举定义 - 统一的交易所、订单类型、订单方向等
// ====================================================================

/**
 * 支持的交易所枚举
 */
enum class LTPExchange {
  BINANCE,                    // 币安现货/杠杆（经典账户）
  BINANCE_USDS_FUTURES,       // 币安U本位合约（经典账户）
  BINANCE_COIN_FUTURES,       // 币安币本位合约（经典账户）
  BINANCE_PORTFOLIO_MARGIN,   // 币安统一账户
  OKX,                        // OKX
  BYBIT,                      // Bybit
  HUOBI,                      // 火币现货
  HUOBI_USDT_SWAP,            // 火币U本位合约
  HUOBI_COIN_SWAP,            // 火币币本位合约
  COINBASE,                   // Coinbase
  KRAKEN,                     // Kraken
  BITFINEX,                   // Bitfinex
  BITMEX,                     // BitMEX
  DERIBIT,                    // Deribit
  GATEIO,                     // Gate.io
  KUCOIN,                     // KuCoin
  FTX,                        // FTX (已关闭，保留用于历史数据)
  MEXC,                       // MEXC
  BITGET,                     // Bitget
  CRYPTOCOM,                  // Crypto.com
  UNKNOWN                     // 未知交易所
};

/**
 * 订单方向
 */
enum class LTPOrderSide {
  BUY,      // 买入
  SELL,     // 卖出
  UNKNOWN
};

/**
 * 订单类型
 */
enum class LTPOrderType {
  LIMIT,              // 限价单
  MARKET,             // 市价单
  STOP_LOSS,          // 止损单
  STOP_LOSS_LIMIT,    // 止损限价单
  TAKE_PROFIT,        // 止盈单
  TAKE_PROFIT_LIMIT,  // 止盈限价单
  LIMIT_MAKER,        // 只做Maker限价单
  UNKNOWN
};

/**
 * 订单状态
 */
enum class LTPOrderStatus {
  NEW,                // 新建订单
  PARTIALLY_FILLED,   // 部分成交
  FILLED,             // 完全成交
  CANCELED,           // 已取消
  PENDING_CANCEL,     // 待取消
  REJECTED,           // 已拒绝
  EXPIRED,            // 已过期
  UNKNOWN
};

/**
 * 时间有效性类型
 */
enum class LTPTimeInForce {
  GTC,    // Good Till Cancel - 一直有效直到取消
  IOC,    // Immediate or Cancel - 立即成交或取消
  FOK,    // Fill or Kill - 全部成交或取消
  GTX,    // Good Till Crossing - 只做Maker
  UNKNOWN
};

// ====================================================================
// 统一的请求参数结构体
// ====================================================================

/**
 * 统一的下单请求参数
 */
struct LTPCreateOrderRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所
  std::string symbol;                 // 交易对，如 "BTCUSDT"
  LTPOrderSide side;              // 订单方向
  LTPOrderType type;              // 订单类型

  // 数量相关（根据订单类型，quantity或quoteOrderQty至少填一个）
  std::string quantity;               // 下单数量（基础货币）
  std::string quoteOrderQty;          // 下单金额（报价货币，仅市价单）

  // 价格相关（限价单必填）
  std::string price;                  // 委托价格
  std::string stopPrice;              // 触发价格（止损/止盈单）

  // 可选字段
  std::string clientOrderId;          // 客户端订单ID（自定义）
  LTPTimeInForce timeInForce;     // 时间有效性

  // 高级选项
  bool reduceOnly;                    // 只减仓（合约）
  bool postOnly;                      // 只做Maker
  std::string leverage;               // 杠杆倍数（合约）
  std::string marginMode;             // 保证金模式：CROSS/ISOLATED（合约）
  std::string positionSide;           // 持仓方向：LONG/SHORT/BOTH（合约）

  // 其他参数（交易所特定参数可以放在这里）
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPCreateOrderRequest()
      : exchange(LTPExchange::UNKNOWN),
        side(LTPOrderSide::UNKNOWN),
        type(LTPOrderType::UNKNOWN),
        timeInForce(LTPTimeInForce::GTC),
        reduceOnly(false),
        postOnly(false) {}
};

/**
 * 统一的撤单请求参数
 */
struct LTPCancelOrderRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所
  std::string symbol;                 // 交易对

  // 订单标识（orderId和clientOrderId至少填一个）
  std::string orderId;                // 交易所订单ID
  std::string clientOrderId;          // 客户端订单ID

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPCancelOrderRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

/**
 * 统一的批量撤单请求参数
 */
struct LTPCancelAllOrdersRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所

  // 可选字段（用于过滤）
  std::string symbol;                 // 交易对（为空则撤销所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPCancelAllOrdersRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

/**
 * 统一的查询订单请求参数
 */
struct LTPGetOrderRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所
  std::string symbol;                 // 交易对

  // 订单标识（orderId和clientOrderId至少填一个）
  std::string orderId;                // 交易所订单ID
  std::string clientOrderId;          // 客户端订单ID

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPGetOrderRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

/**
 * 统一的查询开放订单请求参数
 */
struct LTPGetOpenOrdersRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所

  // 可选字段
  std::string symbol;                 // 交易对（为空则查询所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPGetOpenOrdersRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

/**
 * 统一的查询账户余额请求参数
 */
struct LTPGetAccountBalancesRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPGetAccountBalancesRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

/**
 * 统一的查询持仓请求参数
 */
struct LTPGetAccountPositionsRequest {
  // 必填字段
  LTPExchange exchange;           // 交易所

  // 可选字段
  std::string symbol;                 // 交易对（为空则查询所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  LTPGetAccountPositionsRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

// ====================================================================
// 统一的响应结构体
// ====================================================================

/**
 * 统一的账户余额信息
 */
struct LTPBalanceInfo {
  std::string asset;                      // 资产名称（如BTC、USDT）
  std::string availableBalance;           // 可用余额
  std::string totalBalance;               // 总余额
  std::string frozenBalance;              // 冻结余额

  // 其他信息
  std::map<std::string, std::string> extraInfo;

  // 构造函数
  LTPBalanceInfo() {}
};

/**
 * 统一的持仓信息
 */
struct LTPPositionInfo {
  std::string symbol;                     // 交易对
  std::string positionSide;               // 持仓方向（LONG/SHORT/BOTH）
  std::string positionAmount;             // 持仓数量
  std::string entryPrice;                 // 开仓均价
  // std::string markPrice;                  // 标记价格
  std::string unrealizedProfit;           // 未实现盈亏
  std::string leverage;                   // 杠杆倍数
  std::string marginType;                 // 保证金模式（cross/isolated）

  // 其他信息
  std::map<std::string, std::string> extraInfo;

  // 构造函数
  LTPPositionInfo() {}
};

/**
 * 统一的订单信息
 */
struct LTPOrderInfo {
  std::string orderId;                    // 交易所订单ID
  std::string clientOrderId;              // 客户端订单ID
  std::string symbol;                     // 交易对
  LTPOrderSide side;                  // 订单方向
  LTPOrderType type;                  // 订单类型
  LTPOrderStatus status;              // 订单状态

  std::string price;                      // 委托价格
  std::string quantity;                   // 委托数量
  std::string executedQty;                // 已成交数量
  std::string cumulativeQuoteQty;         // 已成交金额
  std::string avgPrice;                   // 平均成交价格

  std::string stopPrice;                  // 触发价格
  LTPTimeInForce timeInForce;         // 时间有效性

  std::string createTime;                 // 创建时间（毫秒时间戳）
  std::string updateTime;                 // 更新时间（毫秒时间戳）

  // 费用信息
  std::string commission;                 // 手续费
  std::string commissionAsset;            // 手续费币种

  // 其他信息
  std::map<std::string, std::string> extraInfo;

  // 构造函数
  LTPOrderInfo()
      : side(LTPOrderSide::UNKNOWN),
        type(LTPOrderType::UNKNOWN),
        status(LTPOrderStatus::UNKNOWN),
        timeInForce(LTPTimeInForce::UNKNOWN) {}
};

/**
 * 事件类型枚举
 */
enum class LTPEventType {
  RESPONSE,              // 交易响应（下单、撤单等）
  SUBSCRIPTION_DATA,     // 订阅数据（实时推送的订单更新、成交等）
  SESSION_STATUS,        // 会话状态（连接建立、断开等）
  AUTHORIZATION_STATUS,  // 授权状态（授权成功、失败等）
  SUBSCRIPTION_STATUS,   // 订阅状态
  REQUEST_STATUS,        // 请求状态
  FIX,                   // FIX协议消息
  FIX_STATUS,            // FIX协议状态
  HEARTBEAT,             // 心跳
  OTHER,                 // 其他事件
  UNKNOWN                // 未知事件
};

/**
 * 消息类型枚举
 */
enum class LTPMessageType {
  // 交易响应
  CREATE_ORDER,              // 创建订单响应
  CANCEL_ORDER,              // 取消订单响应
  GET_ORDER,                 // 查询订单响应
  GET_OPEN_ORDERS,           // 查询开放订单响应
  GET_ACCOUNT_BALANCES,      // 查询账户余额响应
  GET_ACCOUNT_POSITIONS,     // 查询持仓响应
  RESPONSE_ERROR,            // 响应错误

  // 状态消息
  SESSION_CONNECTION_UP,     // 连接建立
  SESSION_CONNECTION_DOWN,   // 连接断开
  AUTHORIZATION_SUCCESS,     // 授权成功
  AUTHORIZATION_FAILURE,     // 授权失败
  SUBSCRIPTION_STARTED,      // 订阅开始
  SUBSCRIPTION_FAILURE,      // 订阅失败
  SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE,  // 订阅失败（连接失败）

  // 订阅数据（实时推送）
  ORDER_UPDATE,              // 订单状态更新
  PRIVATE_TRADE,             // 私有成交
  BALANCE_UPDATE,            // 余额更新
  POSITION_UPDATE,           // 持仓更新

  OTHER,                     // 其他消息
  UNKNOWN                    // 未知消息
};

/**
 * 统一的响应结果
 */
struct LTPResponse {
  bool success;                           // 是否成功
  std::string errorCode;                  // 错误码
  std::string errorMessage;               // 错误信息

  // 事件信息
  LTPEventType eventType;             // 事件类型
  LTPMessageType messageType;         // 消息类型
  std::string eventTypeString;            // 事件类型字符串（原始）
  std::string messageTypeString;          // 消息类型字符串（原始）

  // 单个订单响应
  LTPOrderInfo orderInfo;

  // 多个订单响应（批量查询）
  std::vector<LTPOrderInfo> orders;

  // 账户余额响应
  std::vector<LTPBalanceInfo> balances;

  // 持仓响应
  std::vector<LTPPositionInfo> positions;

  // 原始响应（用于调试）
  std::string rawResponse;

  // 构造函数
  LTPResponse()
      : success(false),
        eventType(LTPEventType::UNKNOWN),
        messageType(LTPMessageType::UNKNOWN) {}
};

// ====================================================================
// 辅助函数 - 枚举与字符串转换
// ====================================================================

/**
 * 将交易所枚举转换为ccapi的交易所名称字符串
 */
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
    case LTPExchange::FTX:
      return CCAPI_EXCHANGE_NAME_FTX;
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

/**
 * 将字符串转换为交易所枚举
 */
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
  if (exchangeStr == CCAPI_EXCHANGE_NAME_FTX) return LTPExchange::FTX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_MEXC) return LTPExchange::MEXC;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITGET) return LTPExchange::BITGET;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_CRYPTOCOM) return LTPExchange::CRYPTOCOM;
  return LTPExchange::UNKNOWN;
}

/**
 * 将订单方向枚举转换为字符串
 */
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

/**
 * 将字符串转换为订单方向枚举
 */
inline LTPOrderSide stringToOrderSide(const std::string& sideStr) {
  if (sideStr == CCAPI_EM_ORDER_SIDE_BUY || sideStr == "BUY" || sideStr == "buy") {
    return LTPOrderSide::BUY;
  }
  if (sideStr == CCAPI_EM_ORDER_SIDE_SELL || sideStr == "SELL" || sideStr == "sell") {
    return LTPOrderSide::SELL;
  }
  return LTPOrderSide::UNKNOWN;
}

/**
 * 将订单类型枚举转换为字符串
 */
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

/**
 * 将时间有效性枚举转换为字符串
 */
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

/**
 * 将字符串转换为订单类型枚举
 */
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

/**
 * 将字符串转换为订单状态枚举
 *
 * 支持的状态值：
 * - 币安: NEW, PARTIALLY_FILLED, FILLED, CANCELED, PENDING_CANCEL, REJECTED, EXPIRED
 * - OKX: live, partially_filled, filled, canceled, mmp_canceled
 */
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
  // live: 订单已提交，等待成交
  if (statusStr == "live") return LTPOrderStatus::NEW;
  // partially_filled: 部分成交
  if (statusStr == "partially_filled") return LTPOrderStatus::PARTIALLY_FILLED;
  // filled: 完全成交
  if (statusStr == "filled") return LTPOrderStatus::FILLED;
  // canceled: 已撤销
  if (statusStr == "canceled") return LTPOrderStatus::CANCELED;
  // mmp_canceled: 做市商保护撤销
  if (statusStr == "mmp_canceled") return LTPOrderStatus::CANCELED;

  return LTPOrderStatus::UNKNOWN;
}

/**
 * 将字符串转换为时间有效性枚举
 */
inline LTPTimeInForce stringToTimeInForce(const std::string& tifStr) {
  if (tifStr == "GTC") return LTPTimeInForce::GTC;
  if (tifStr == "IOC") return LTPTimeInForce::IOC;
  if (tifStr == "FOK") return LTPTimeInForce::FOK;
  if (tifStr == "GTX") return LTPTimeInForce::GTX;
  return LTPTimeInForce::UNKNOWN;
}

// 注意：交易所能力配置已移至 LTPTradingService 类中管理
// 请使用 LTPTradingService::getExchangeCapabilities() 方法查询交易所能力

} /* namespace ltp */

#endif  // INCLUDE_LTP_TRADING_INTERFACE_H_