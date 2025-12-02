/**
 * @file ltp_ucli_ffi.h
 * It provides functionality for publishing and receiving market and order data
 * through a topic-based messaging system.
 *
 * @copyright Copyright (c) 2025 LiquidityTech. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, or modification of this file is strictly prohibited.
 *
 * For more information, visit: https://www.liquiditytech.com/
 *
 * LiquidityTech - The Leading Digital Asset Prime Brokerage for Institutions
 * Trusted execution, custody, clearing, and financing solutions for digital assets.
 */

#ifndef LTP_UCLI_FFI_H
#define LTP_UCLI_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct UltraMarketPublisher
 * @brief Structure representing a publisher for market data
 *
 * This structure maintains the internal state needed to publish market data
 * to a specific topic.
 *
 * @var UltraMarketPublisher::topic
 * The topic name to publish market data to
 *
 * @var UltraMarketPublisher::inner
 * Internal implementation details for the publisher
 */
struct UltraMarketPublisher {
    const void *topic;
    const void *inner;
};

/**
 * @struct UltraOrderPublisher
 * @brief Structure representing a publisher for order data
 *
 * This structure maintains the internal state needed to publish order data
 * to a specific topic.
 *
 * @var UltraOrderPublisher::topic
 * The topic name to publish order data to
 *
 * @var UltraOrderPublisher::inner
 * Internal implementation details for the publisher
 */
struct UltraOrderPublisher {
    void *topic;
    void *inner;
};

/**
 * @struct UltraMarketReceiver
 * @brief Structure representing a receiver for market data
 *
 * This structure maintains the internal state needed to receive market data
 * from a specific topic.
 *
 * @var UltraMarketReceiver::topic
 * The topic name to receive market data from
 *
 * @var UltraMarketReceiver::inner
 * Internal implementation details for the receiver
 */
struct UltraMarketReceiver {
    void *topic;
    void *inner;
};

/**
 * @struct UltraOrderReceiver
 * @brief Structure representing a receiver for order data
 *
 * This structure maintains the internal state needed to receive order data
 * from a specific topic.
 *
 * @var UltraOrderReceiver::topic
 * The topic name to receive order data from
 *
 * @var UltraOrderReceiver::inner
 * Internal implementation details for the receiver
 */
struct UltraOrderReceiver {
    void *topic;
    void *inner;
};

/**
 * @brief Initialize a new market data publisher
 *
 * Creates and initializes a new publisher that can send market data to the specified topic.
 *
 * @param topic The name of the topic to publish market data to
 * @param len The length of the topic name string
 * @return A pointer to a newly allocated UltraMarketPublisher structure,
 *         or NULL if initialization fails
 */
extern struct UltraMarketPublisher* init_market_publisher(const char *topic, unsigned int len);

/**
 * @brief Initialize a new order data publisher
 *
 * Creates and initializes a new publisher that can send order data to the specified topic.
 *
 * @param topic The name of the topic to publish order data to
 * @param len The length of the topic name string
 * @return A pointer to a newly allocated UltraOrderPublisher structure,
 *         or NULL if initialization fails
 */
extern struct UltraOrderPublisher* init_order_publisher(const char *topic, unsigned int len);

/**
 * @brief Initialize a new market data receiver
 *
 * Creates and initializes a new receiver that can receive market data from the specified topic.
 *
 * @param topic The name of the topic to receive market data from
 * @param len The length of the topic name string
 * @return A pointer to a newly allocated UltraMarketReceiver structure,
 *         or NULL if initialization fails
 */
extern struct UltraMarketReceiver* init_market_receiver(const char *topic, unsigned int len);

/**
 * @brief Initialize a new order data receiver
 *
 * Creates and initializes a new receiver that can receive order data from the specified topic.
 *
 * @param topic The name of the topic to receive order data from
 * @param len The length of the topic name string
 * @return A pointer to a newly allocated UltraOrderReceiver structure,
 *         or NULL if initialization fails
 */
extern struct UltraOrderReceiver* init_order_receiver(const char *topic, unsigned int len);

/**
 * @brief Publish market data to a topic (non-blocking)
 *
 * Sends the provided message as market data to the topic associated with the given publisher.
 * This function is non-blocking and returns immediately without waiting for the message
 * to be delivered or acknowledged.
 *
 * @param publisher A pointer to an initialized UltraMarketPublisher
 * @param msg The market data message to publish
 * @param len The length of the message in bytes
 */
extern void publish_market(struct UltraMarketPublisher *publisher, const char *msg, unsigned int len);

/**
 * @brief Publish order data to a topic (non-blocking)
 *
 * Sends the provided message as order data to the topic associated with the given publisher.
 * This function is non-blocking and returns immediately without waiting for the message
 * to be delivered or acknowledged.
 *
 * @param publisher A pointer to an initialized UltraOrderPublisher
 * @param msg The order data message to publish
 * @param len The length of the message in bytes
 */
extern void publish_order(struct UltraOrderPublisher *publisher, const char *msg, unsigned int len);

/**
 * @brief Receive market data from a topic (non-blocking)
 *
 * Attempts to receive a market data message from the topic associated with the given receiver.
 * The message is stored in the provided buffer if available.
 * This function is non-blocking and returns immediately, even if no message is available.
 *
 * @param receiver A pointer to an initialized UltraMarketReceiver
 * @param buf A buffer to store the received message
 * @return The number of bytes received, or 0 if no message was available
 */
extern unsigned int receive_market(struct UltraMarketReceiver *receiver, char *buf);

/**
 * @brief Receive order data from a topic (non-blocking)
 *
 * Attempts to receive an order data message from the topic associated with the given receiver.
 * The message is stored in the provided buffer if available.
 * This function is non-blocking and returns immediately, even if no message is available.
 *
 * @param receiver A pointer to an initialized UltraOrderReceiver
 * @param buf A buffer to store the received message
 * @return The number of bytes received, or 0 if no message was available
 */
extern unsigned int receive_order(struct UltraOrderReceiver *receiver, char *buf);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //LTP_UCLI_FFI_H
