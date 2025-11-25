#ifndef INCLUDE_CCAPI_CPP_CCAPI_UNIFIED_TRADING_INTERFACE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UNIFIED_TRADING_INTERFACE_H_

#include <string>
#include <map>
#include <vector>
#include "ccapi_cpp/ccapi_macro.h"

namespace ccapi {

/**
 * 统一交易接口 - 为客户提供跨交易所的统一下单和撤单接口
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
enum class UnifiedExchange {
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
enum class UnifiedOrderSide {
  BUY,      // 买入
  SELL,     // 卖出
  UNKNOWN
};

/**
 * 订单类型
 */
enum class UnifiedOrderType {
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
enum class UnifiedOrderStatus {
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
enum class UnifiedTimeInForce {
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
struct UnifiedCreateOrderRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所
  std::string symbol;                 // 交易对，如 "BTCUSDT"
  UnifiedOrderSide side;              // 订单方向
  UnifiedOrderType type;              // 订单类型

  // 数量相关（根据订单类型，quantity或quoteOrderQty至少填一个）
  std::string quantity;               // 下单数量（基础货币）
  std::string quoteOrderQty;          // 下单金额（报价货币，仅市价单）

  // 价格相关（限价单必填）
  std::string price;                  // 委托价格
  std::string stopPrice;              // 触发价格（止损/止盈单）

  // 可选字段
  std::string clientOrderId;          // 客户端订单ID（自定义）
  UnifiedTimeInForce timeInForce;     // 时间有效性

  // 高级选项
  bool reduceOnly;                    // 只减仓（合约）
  bool postOnly;                      // 只做Maker
  std::string leverage;               // 杠杆倍数（合约）
  std::string marginMode;             // 保证金模式：CROSS/ISOLATED（合约）
  std::string positionSide;           // 持仓方向：LONG/SHORT/BOTH（合约）

  // 其他参数（交易所特定参数可以放在这里）
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedCreateOrderRequest()
      : exchange(UnifiedExchange::UNKNOWN),
        side(UnifiedOrderSide::UNKNOWN),
        type(UnifiedOrderType::UNKNOWN),
        timeInForce(UnifiedTimeInForce::GTC),
        reduceOnly(false),
        postOnly(false) {}
};

/**
 * 统一的撤单请求参数
 */
struct UnifiedCancelOrderRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所
  std::string symbol;                 // 交易对

  // 订单标识（orderId和clientOrderId至少填一个）
  std::string orderId;                // 交易所订单ID
  std::string clientOrderId;          // 客户端订单ID

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedCancelOrderRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

/**
 * 统一的批量撤单请求参数
 */
struct UnifiedCancelAllOrdersRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所

  // 可选字段（用于过滤）
  std::string symbol;                 // 交易对（为空则撤销所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedCancelAllOrdersRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

/**
 * 统一的查询订单请求参数
 */
struct UnifiedGetOrderRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所
  std::string symbol;                 // 交易对

  // 订单标识（orderId和clientOrderId至少填一个）
  std::string orderId;                // 交易所订单ID
  std::string clientOrderId;          // 客户端订单ID

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedGetOrderRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

/**
 * 统一的查询开放订单请求参数
 */
struct UnifiedGetOpenOrdersRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所

  // 可选字段
  std::string symbol;                 // 交易对（为空则查询所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedGetOpenOrdersRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

/**
 * 统一的查询账户余额请求参数
 */
struct UnifiedGetAccountBalancesRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedGetAccountBalancesRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

/**
 * 统一的查询持仓请求参数
 */
struct UnifiedGetAccountPositionsRequest {
  // 必填字段
  UnifiedExchange exchange;           // 交易所

  // 可选字段
  std::string symbol;                 // 交易对（为空则查询所有）

  // 其他参数
  std::map<std::string, std::string> extraParams;

  // 构造函数
  UnifiedGetAccountPositionsRequest()
      : exchange(UnifiedExchange::UNKNOWN) {}
};

// ====================================================================
// 统一的响应结构体
// ====================================================================

/**
 * 统一的账户余额信息
 */
struct UnifiedBalanceInfo {
  std::string asset;                      // 资产名称（如BTC、USDT）
  std::string availableBalance;           // 可用余额
  std::string totalBalance;               // 总余额
  std::string frozenBalance;              // 冻结余额

  // 其他信息
  std::map<std::string, std::string> extraInfo;

  // 构造函数
  UnifiedBalanceInfo() {}
};

/**
 * 统一的持仓信息
 */
struct UnifiedPositionInfo {
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
  UnifiedPositionInfo() {}
};

/**
 * 统一的订单信息
 */
struct UnifiedOrderInfo {
  std::string orderId;                    // 交易所订单ID
  std::string clientOrderId;              // 客户端订单ID
  std::string symbol;                     // 交易对
  UnifiedOrderSide side;                  // 订单方向
  UnifiedOrderType type;                  // 订单类型
  UnifiedOrderStatus status;              // 订单状态

  std::string price;                      // 委托价格
  std::string quantity;                   // 委托数量
  std::string executedQty;                // 已成交数量
  std::string cumulativeQuoteQty;         // 已成交金额
  std::string avgPrice;                   // 平均成交价格

  std::string stopPrice;                  // 触发价格
  UnifiedTimeInForce timeInForce;         // 时间有效性

  std::string createTime;                 // 创建时间（毫秒时间戳）
  std::string updateTime;                 // 更新时间（毫秒时间戳）

  // 费用信息
  std::string commission;                 // 手续费
  std::string commissionAsset;            // 手续费币种

  // 其他信息
  std::map<std::string, std::string> extraInfo;

  // 构造函数
  UnifiedOrderInfo()
      : side(UnifiedOrderSide::UNKNOWN),
        type(UnifiedOrderType::UNKNOWN),
        status(UnifiedOrderStatus::UNKNOWN),
        timeInForce(UnifiedTimeInForce::UNKNOWN) {}
};

/**
 * 事件类型枚举
 */
enum class UnifiedEventType {
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
enum class UnifiedMessageType {
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
struct UnifiedResponse {
  bool success;                           // 是否成功
  std::string errorCode;                  // 错误码
  std::string errorMessage;               // 错误信息

  // 事件信息
  UnifiedEventType eventType;             // 事件类型
  UnifiedMessageType messageType;         // 消息类型
  std::string eventTypeString;            // 事件类型字符串（原始）
  std::string messageTypeString;          // 消息类型字符串（原始）

  // 单个订单响应
  UnifiedOrderInfo orderInfo;

  // 多个订单响应（批量查询）
  std::vector<UnifiedOrderInfo> orders;

  // 账户余额响应
  std::vector<UnifiedBalanceInfo> balances;

  // 持仓响应
  std::vector<UnifiedPositionInfo> positions;

  // 原始响应（用于调试）
  std::string rawResponse;

  // 构造函数
  UnifiedResponse()
      : success(false),
        eventType(UnifiedEventType::UNKNOWN),
        messageType(UnifiedMessageType::UNKNOWN) {}
};

// ====================================================================
// 辅助函数 - 枚举与字符串转换
// ====================================================================

/**
 * 将UnifiedExchange转换为ccapi的交易所名称
 */
inline std::string unifiedExchangeToString(UnifiedExchange exchange) {
  switch (exchange) {
    case UnifiedExchange::BINANCE:
      return CCAPI_EXCHANGE_NAME_BINANCE;
    case UnifiedExchange::BINANCE_USDS_FUTURES:
      return CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES;
    case UnifiedExchange::BINANCE_COIN_FUTURES:
      return CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES;
    case UnifiedExchange::BINANCE_PORTFOLIO_MARGIN:
      return CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN;
    case UnifiedExchange::OKX:
      return CCAPI_EXCHANGE_NAME_OKX;
    case UnifiedExchange::BYBIT:
      return CCAPI_EXCHANGE_NAME_BYBIT;
    case UnifiedExchange::HUOBI:
      return CCAPI_EXCHANGE_NAME_HUOBI;
    case UnifiedExchange::HUOBI_USDT_SWAP:
      return CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP;
    case UnifiedExchange::HUOBI_COIN_SWAP:
      return CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP;
    case UnifiedExchange::COINBASE:
      return CCAPI_EXCHANGE_NAME_COINBASE;
    case UnifiedExchange::KRAKEN:
      return CCAPI_EXCHANGE_NAME_KRAKEN;
    case UnifiedExchange::BITFINEX:
      return CCAPI_EXCHANGE_NAME_BITFINEX;
    case UnifiedExchange::BITMEX:
      return CCAPI_EXCHANGE_NAME_BITMEX;
    case UnifiedExchange::DERIBIT:
      return CCAPI_EXCHANGE_NAME_DERIBIT;
    case UnifiedExchange::GATEIO:
      return CCAPI_EXCHANGE_NAME_GATEIO;
    case UnifiedExchange::KUCOIN:
      return CCAPI_EXCHANGE_NAME_KUCOIN;
    case UnifiedExchange::FTX:
      return CCAPI_EXCHANGE_NAME_FTX;
    case UnifiedExchange::MEXC:
      return CCAPI_EXCHANGE_NAME_MEXC;
    case UnifiedExchange::BITGET:
      return CCAPI_EXCHANGE_NAME_BITGET;
    case UnifiedExchange::CRYPTOCOM:
      return CCAPI_EXCHANGE_NAME_CRYPTOCOM;
    default:
      return CCAPI_UNKNOWN;
  }
}

/**
 * 将字符串转换为UnifiedExchange
 */
inline UnifiedExchange stringToUnifiedExchange(const std::string& exchangeStr) {
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE) return UnifiedExchange::BINANCE;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES) return UnifiedExchange::BINANCE_USDS_FUTURES;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES) return UnifiedExchange::BINANCE_COIN_FUTURES;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN) return UnifiedExchange::BINANCE_PORTFOLIO_MARGIN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_OKX) return UnifiedExchange::OKX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BYBIT) return UnifiedExchange::BYBIT;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI) return UnifiedExchange::HUOBI;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP) return UnifiedExchange::HUOBI_USDT_SWAP;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP) return UnifiedExchange::HUOBI_COIN_SWAP;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_COINBASE) return UnifiedExchange::COINBASE;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_KRAKEN) return UnifiedExchange::KRAKEN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITFINEX) return UnifiedExchange::BITFINEX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITMEX) return UnifiedExchange::BITMEX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_DERIBIT) return UnifiedExchange::DERIBIT;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_GATEIO) return UnifiedExchange::GATEIO;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_KUCOIN) return UnifiedExchange::KUCOIN;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_FTX) return UnifiedExchange::FTX;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_MEXC) return UnifiedExchange::MEXC;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_BITGET) return UnifiedExchange::BITGET;
  if (exchangeStr == CCAPI_EXCHANGE_NAME_CRYPTOCOM) return UnifiedExchange::CRYPTOCOM;
  return UnifiedExchange::UNKNOWN;
}

/**
 * 将UnifiedOrderSide转换为字符串
 */
inline std::string unifiedOrderSideToString(UnifiedOrderSide side) {
  switch (side) {
    case UnifiedOrderSide::BUY:
      return CCAPI_EM_ORDER_SIDE_BUY;
    case UnifiedOrderSide::SELL:
      return CCAPI_EM_ORDER_SIDE_SELL;
    default:
      return CCAPI_UNKNOWN;
  }
}

/**
 * 将字符串转换为UnifiedOrderSide
 */
inline UnifiedOrderSide stringToUnifiedOrderSide(const std::string& sideStr) {
  if (sideStr == CCAPI_EM_ORDER_SIDE_BUY || sideStr == "BUY" || sideStr == "buy") {
    return UnifiedOrderSide::BUY;
  }
  if (sideStr == CCAPI_EM_ORDER_SIDE_SELL || sideStr == "SELL" || sideStr == "sell") {
    return UnifiedOrderSide::SELL;
  }
  return UnifiedOrderSide::UNKNOWN;
}

/**
 * 将UnifiedOrderType转换为字符串
 */
inline std::string unifiedOrderTypeToString(UnifiedOrderType type) {
  switch (type) {
    case UnifiedOrderType::LIMIT:
      return "LIMIT";
    case UnifiedOrderType::MARKET:
      return "MARKET";
    case UnifiedOrderType::STOP_LOSS:
      return "STOP_LOSS";
    case UnifiedOrderType::STOP_LOSS_LIMIT:
      return "STOP_LOSS_LIMIT";
    case UnifiedOrderType::TAKE_PROFIT:
      return "TAKE_PROFIT";
    case UnifiedOrderType::TAKE_PROFIT_LIMIT:
      return "TAKE_PROFIT_LIMIT";
    case UnifiedOrderType::LIMIT_MAKER:
      return "LIMIT_MAKER";
    default:
      return CCAPI_UNKNOWN;
  }
}

/**
 * 将UnifiedTimeInForce转换为字符串
 */
inline std::string unifiedTimeInForceToString(UnifiedTimeInForce tif) {
  switch (tif) {
    case UnifiedTimeInForce::GTC:
      return "GTC";
    case UnifiedTimeInForce::IOC:
      return "IOC";
    case UnifiedTimeInForce::FOK:
      return "FOK";
    case UnifiedTimeInForce::GTX:
      return "GTX";
    default:
      return "GTC";  // 默认GTC
  }
}

/**
 * 将字符串转换为UnifiedOrderType
 */
inline UnifiedOrderType stringToUnifiedOrderType(const std::string& typeStr) {
  if (typeStr == "LIMIT") return UnifiedOrderType::LIMIT;
  if (typeStr == "MARKET") return UnifiedOrderType::MARKET;
  if (typeStr == "STOP_LOSS" || typeStr == "STOP") return UnifiedOrderType::STOP_LOSS;
  if (typeStr == "STOP_LOSS_LIMIT") return UnifiedOrderType::STOP_LOSS_LIMIT;
  if (typeStr == "TAKE_PROFIT") return UnifiedOrderType::TAKE_PROFIT;
  if (typeStr == "TAKE_PROFIT_LIMIT") return UnifiedOrderType::TAKE_PROFIT_LIMIT;
  if (typeStr == "LIMIT_MAKER") return UnifiedOrderType::LIMIT_MAKER;
  return UnifiedOrderType::UNKNOWN;
}

/**
 * 将字符串转换为UnifiedOrderStatus
 *
 * 支持的状态值：
 * - 币安: NEW, PARTIALLY_FILLED, FILLED, CANCELED, PENDING_CANCEL, REJECTED, EXPIRED
 * - OKX: live, partially_filled, filled, canceled, mmp_canceled
 */
inline UnifiedOrderStatus stringToUnifiedOrderStatus(const std::string& statusStr) {
  // 币安状态
  if (statusStr == "NEW" || statusStr == "PENDING") return UnifiedOrderStatus::NEW;
  if (statusStr == "PARTIALLY_FILLED" || statusStr == "PARTIAL_FILL") return UnifiedOrderStatus::PARTIALLY_FILLED;
  if (statusStr == "FILLED" || statusStr == "FILL") return UnifiedOrderStatus::FILLED;
  if (statusStr == "CANCELED" || statusStr == "CANCELLED") return UnifiedOrderStatus::CANCELED;
  if (statusStr == "PENDING_CANCEL") return UnifiedOrderStatus::PENDING_CANCEL;
  if (statusStr == "REJECTED" || statusStr == "REJECT") return UnifiedOrderStatus::REJECTED;
  if (statusStr == "EXPIRED" || statusStr == "EXPIRE") return UnifiedOrderStatus::EXPIRED;

  // OKX状态
  // live: 订单已提交，等待成交
  if (statusStr == "live") return UnifiedOrderStatus::NEW;
  // partially_filled: 部分成交
  if (statusStr == "partially_filled") return UnifiedOrderStatus::PARTIALLY_FILLED;
  // filled: 完全成交
  if (statusStr == "filled") return UnifiedOrderStatus::FILLED;
  // canceled: 已撤销
  if (statusStr == "canceled") return UnifiedOrderStatus::CANCELED;
  // mmp_canceled: 做市商保护撤销
  if (statusStr == "mmp_canceled") return UnifiedOrderStatus::CANCELED;

  return UnifiedOrderStatus::UNKNOWN;
}

/**
 * 将字符串转换为UnifiedTimeInForce
 */
inline UnifiedTimeInForce stringToUnifiedTimeInForce(const std::string& tifStr) {
  if (tifStr == "GTC") return UnifiedTimeInForce::GTC;
  if (tifStr == "IOC") return UnifiedTimeInForce::IOC;
  if (tifStr == "FOK") return UnifiedTimeInForce::FOK;
  if (tifStr == "GTX") return UnifiedTimeInForce::GTX;
  return UnifiedTimeInForce::UNKNOWN;
}

// 注意：交易所能力配置已移至 UnifiedTradingService 类中管理
// 请使用 UnifiedTradingService::getExchangeCapabilities() 方法查询交易所能力

} /* namespace ccapi */

#endif  // INCLUDE_CCAPI_CPP_CCAPI_UNIFIED_TRADING_INTERFACE_H_

