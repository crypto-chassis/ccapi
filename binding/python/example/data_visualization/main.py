import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Event
import matplotlib.pyplot as plt
import seaborn as sns
import time

if __name__ == "__main__":
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config)
    exchange = "okx"
    instrument = "BTC-USDT"
    subscription = Subscription(exchange, instrument, "MARKET_DEPTH", "MARKET_DEPTH_MAX=400&CONFLATE_INTERVAL_MILLISECONDS=100")
    session.subscribe(subscription)
    fig, ax = plt.subplots()
    startTime = time.time()
    while True:
        bids = {"price": [], "size": []}
        asks = {"price": [], "size": []}
        eventList = session.getEventQueue().purge()
        if eventList:
            event = eventList[-1]
            if event.getType() == Event.Type_SUBSCRIPTION_DATA:
                for message in event.getMessageList():
                    for element in message.getElementList():
                        elementNameValueMap = element.getNameValueMap()
                        for name, value in elementNameValueMap.items():
                            if name == "BID_PRICE":
                                bids["price"].append(float(value))
                            if name == "BID_SIZE":
                                bids["size"].append(float(value))
                            if name == "ASK_PRICE":
                                asks["price"].append(float(value))
                            if name == "ASK_SIZE":
                                asks["size"].append(float(value))
                    ax.clear()
                    ax.set_title(f"{instrument} Order Book On {exchange.title()} at {message.getTimeISO()}")
                    ax.set_xlabel("Price")
                    ax.set_ylabel("Amount")
                    sns.ecdfplot(
                        x="price",
                        weights="size",
                        legend=False,
                        stat="count",
                        complementary=True,
                        data={"price": bids["price"], "size": bids["size"]},
                        ax=ax,
                        color="g",
                    )
                    sns.ecdfplot(x="price", weights="size", legend=False, stat="count", data={"price": asks["price"], "size": asks["size"]}, ax=ax, color="r")
        plt.pause(0.1)
        if time.time() - startTime > 10:
            break
    session.stop()
    print("Bye")
