#ifndef INCLUDE_LTP_TRADING_INTERFACE_H_
#define INCLUDE_LTP_TRADING_INTERFACE_H_

#include <string>
#include <map>
#include <vector>

namespace ltp {

// ====================================================================
// Enumerations - Unified exchange, order type, order side, etc.
// ====================================================================

enum class LTPExchange {
  BINANCE,                    // Binance Spot/Margin (Classic Account)
  BINANCE_USDS_FUTURES,       // Binance USDT-M Futures (Classic Account)
  BINANCE_COIN_FUTURES,       // Binance COIN-M Futures (Classic Account)
  BINANCE_PORTFOLIO_MARGIN,   // Binance Portfolio Margin (Unified Account)
  OKX,                        // OKX
  BYBIT,                      // Bybit
  HUOBI,                      // Huobi Spot
  HUOBI_USDT_SWAP,            // Huobi USDT Perpetual
  HUOBI_COIN_SWAP,            // Huobi Coin Perpetual
  COINBASE,                   // Coinbase
  KRAKEN,                     // Kraken
  BITFINEX,                   // Bitfinex
  BITMEX,                     // BitMEX
  DERIBIT,                    // Deribit
  GATEIO,                     // Gate.io
  KUCOIN,                     // KuCoin
  MEXC,                       // MEXC
  BITGET,                     // Bitget
  CRYPTOCOM,                  // Crypto.com
  UNKNOWN                     // Unknown exchange
};

enum class LTPOrderSide {
  BUY,      // Buy
  SELL,     // Sell
  UNKNOWN
};

enum class LTPOrderType {
  LIMIT,              // Limit order
  MARKET,             // Market order
  STOP_LOSS,          // Stop loss order
  STOP_LOSS_LIMIT,    // Stop loss limit order
  TAKE_PROFIT,        // Take profit order
  TAKE_PROFIT_LIMIT,  // Take profit limit order
  LIMIT_MAKER,        // Post-only limit order
  UNKNOWN
};

enum class LTPOrderStatus {
  NEW,                // New order
  PARTIALLY_FILLED,   // Partially filled
  FILLED,             // Fully filled
  CANCELED,           // Canceled
  PENDING_CANCEL,     // Pending cancel
  REJECTED,           // Rejected
  EXPIRED,            // Expired
  UNKNOWN
};

enum class LTPTimeInForce {
  GTC,    // Good Till Cancel
  IOC,    // Immediate or Cancel
  FOK,    // Fill or Kill
  GTX,    // Good Till Crossing (Post Only)
  UNKNOWN
};

// ====================================================================
// Request parameter structures
// ====================================================================

struct LTPCreateOrderRequest {
  // Required fields
  LTPExchange exchange;           // Exchange
  std::string symbol;             // Trading pair, e.g. "BTCUSDT"
  LTPOrderSide side;              // Order side
  LTPOrderType type;              // Order type

  // Quantity related (at least one of quantity or quoteOrderQty required depending on order type)
  std::string quantity;               // Order quantity (base currency)
  std::string quoteOrderQty;          // Order amount (quote currency, market orders only)

  // Price related (required for limit orders)
  std::string price;                  // Order price
  std::string stopPrice;              // Trigger price (for stop loss/take profit orders)

  // Optional fields
  std::string clientOrderId;          // Client order ID (custom)
  LTPTimeInForce timeInForce;

  // Advanced options
  bool reduceOnly;                    // Reduce only (futures)
  bool postOnly;                      // Post only (maker only)
  std::string leverage;               // Leverage (futures)
  std::string marginMode;             // Margin mode: CROSS/ISOLATED (futures)
  std::string positionSide;           // Position side: LONG/SHORT/BOTH (futures)

  // Other parameters (exchange-specific parameters can be placed here)
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPCreateOrderRequest()
      : exchange(LTPExchange::UNKNOWN),
        side(LTPOrderSide::UNKNOWN),
        type(LTPOrderType::UNKNOWN),
        timeInForce(LTPTimeInForce::GTC),
        reduceOnly(false),
        postOnly(false) {}
};

struct LTPCancelOrderRequest {
  // Required fields
  LTPExchange exchange;           // Exchange
  std::string symbol;                 // Trading pair

  // Order identifier (at least one of orderId or clientOrderId required)
  std::string orderId;                // Exchange order ID
  std::string clientOrderId;          // Client order ID

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPCancelOrderRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

struct LTPCancelAllOrdersRequest {
  // Required fields
  LTPExchange exchange;           // Exchange

  // Optional fields (for filtering)
  std::string symbol;                 // Trading pair (empty to cancel all)

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPCancelAllOrdersRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

struct LTPGetOrderRequest {
  // Required fields
  LTPExchange exchange;           // Exchange
  std::string symbol;                 // Trading pair

  // Order identifier (at least one of orderId or clientOrderId required)
  std::string orderId;                // Exchange order ID
  std::string clientOrderId;          // Client order ID

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPGetOrderRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

struct LTPGetOpenOrdersRequest {
  // Required fields
  LTPExchange exchange;           // Exchange

  // Optional fields
  std::string symbol;                 // Trading pair (empty to query all)

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPGetOpenOrdersRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

struct LTPGetAccountBalancesRequest {
  // Required fields
  LTPExchange exchange;           // Exchange

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPGetAccountBalancesRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

struct LTPGetAccountPositionsRequest {
  // Required fields
  LTPExchange exchange;           // Exchange

  // Optional fields
  std::string symbol;                 // Trading pair (empty to query all)

  // Other parameters
  std::map<std::string, std::string> extraParams;

  // Constructor
  LTPGetAccountPositionsRequest()
      : exchange(LTPExchange::UNKNOWN) {}
};

// ====================================================================
// Response structures
// ====================================================================

struct LTPBalanceInfo {
  std::string asset;                      // Asset name (e.g. BTC, USDT)
  std::string availableBalance;           // Available balance
  std::string totalBalance;               // Total balance
  std::string frozenBalance;              // Frozen balance

  // Additional information
  std::map<std::string, std::string> extraInfo;

  // Constructor
  LTPBalanceInfo() {}
};

struct LTPPositionInfo {
  std::string symbol;                     // Trading pair
  std::string positionSide;               // Position side (LONG/SHORT/BOTH)
  std::string positionAmount;             // Position amount
  std::string entryPrice;                 // Entry price
  std::string unrealizedProfit;           // Unrealized profit/loss
  std::string leverage;                   // Leverage
  std::string marginType;                 // Margin type (cross/isolated)

  // Additional information
  std::map<std::string, std::string> extraInfo;

  // Constructor
  LTPPositionInfo() {}
};

struct LTPOrderInfo {
  std::string orderId;                    // Exchange order ID
  std::string clientOrderId;              // Client order ID
  std::string symbol;                     // Trading pair
  LTPOrderSide side;                  // Order side
  LTPOrderType type;                  // Order type
  LTPOrderStatus status;              // Order status

  std::string price;                      // Order price
  std::string quantity;                   // Order quantity
  std::string executedQty;                // Executed quantity
  std::string cumulativeQuoteQty;         // Cumulative quote quantity
  std::string avgPrice;                   // Average execution price

  std::string stopPrice;                  // Stop price
  LTPTimeInForce timeInForce;         // Time in force

  std::string createTime;                 // Creation time (millisecond timestamp)
  std::string updateTime;                 // Update time (millisecond timestamp)

  // Fee information
  std::string commission;                 // Commission fee
  std::string commissionAsset;            // Commission asset

  // Additional information
  std::map<std::string, std::string> extraInfo;

  // Constructor
  LTPOrderInfo()
      : side(LTPOrderSide::UNKNOWN),
        type(LTPOrderType::UNKNOWN),
        status(LTPOrderStatus::UNKNOWN),
        timeInForce(LTPTimeInForce::UNKNOWN) {}
};

enum class LTPEventType {
  RESPONSE,              // Trading response (order creation, cancellation, etc.)
  SUBSCRIPTION_DATA,     // Subscription data (real-time order updates, trades, etc.)
  SESSION_STATUS,        // Session status (connection established, disconnected, etc.)
  AUTHORIZATION_STATUS,  // Authorization status (authorization success, failure, etc.)
  SUBSCRIPTION_STATUS,   // Subscription status
  REQUEST_STATUS,        // Request status
  FIX,                   // FIX protocol message
  FIX_STATUS,            // FIX protocol status
  HEARTBEAT,             // Heartbeat
  OTHER,                 // Other events
  UNKNOWN                // Unknown event
};

enum class LTPMessageType {
  // Trading responses
  CREATE_ORDER,              // Create order response
  CANCEL_ORDER,              // Cancel order response
  GET_ORDER,                 // Get order response
  GET_OPEN_ORDERS,           // Get open orders response
  GET_ACCOUNT_BALANCES,      // Get account balances response
  GET_ACCOUNT_POSITIONS,     // Get account positions response
  RESPONSE_ERROR,            // Response error

  // Status messages
  SESSION_CONNECTION_UP,     // Connection established
  SESSION_CONNECTION_DOWN,   // Connection disconnected
  AUTHORIZATION_SUCCESS,     // Authorization successful
  AUTHORIZATION_FAILURE,     // Authorization failed
  SUBSCRIPTION_STARTED,      // Subscription started
  SUBSCRIPTION_FAILURE,      // Subscription failed
  SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE,  // Subscription failed (connection failure)

  // Subscription data (real-time push)
  ORDER_UPDATE,              // Order status update
  PRIVATE_TRADE,             // Private trade
  BALANCE_UPDATE,            // Balance update
  POSITION_UPDATE,           // Position update

  OTHER,                     // Other message
  UNKNOWN                    // Unknown message
};

struct LTPResponse {
  bool success;                           // Success flag
  std::string errorCode;                  // Error code
  std::string errorMessage;               // Error message

  // Event information
  LTPEventType eventType;             // Event type
  LTPMessageType messageType;         // Message type
  std::string eventTypeString;            // Event type string (raw)
  std::string messageTypeString;          // Message type string (raw)

  // Single order response
  LTPOrderInfo orderInfo;

  // Multiple orders response (batch query)
  std::vector<LTPOrderInfo> orders;

  // Account balance response
  std::vector<LTPBalanceInfo> balances;

  // Position response
  std::vector<LTPPositionInfo> positions;

  // Raw response (for debugging)
  std::string rawResponse;

  // Constructor
  LTPResponse()
      : success(false),
        eventType(LTPEventType::UNKNOWN),
        messageType(LTPMessageType::UNKNOWN) {}
};

} /* namespace ltp */

#endif  // INCLUDE_LTP_TRADING_INTERFACE_H_