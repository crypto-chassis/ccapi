import os
import sys
import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Request, Event, Message


class MyEventHandler(EventHandler):
    def __init__(self, fixSubscriptionCorrelationId):
        super().__init__()
        self.fixSubscriptionCorrelationId = fixSubscriptionCorrelationId
        self.sentRequest = False
        self.firstProcessEventTime = None

    def processEvent(self, event: Event, session: Session) -> None:
        if event.getType() != Event.Type_HEARTBEAT:
            print(f"Received an event:\n{event.toPrettyString(2, 2)}")
        if self.firstProcessEventTime is None:
            self.firstProcessEventTime = time.time()
            elapsedSeconds = 0
        else:
            elapsedSeconds = time.time() - self.firstProcessEventTime

        if elapsedSeconds >= 1 and not self.sentRequest:
            self.sentRequest = True
            request = Request(Request.Operation_FIX, "binance")
            request.appendFixParam(
                [
                    (35, "D"),
                    (11, request.generateNextClientOrderId()),
                    (55, "BTCUSDT"),
                    (54, "1"),
                    (44, "100000"),
                    (38, "0.0001"),
                    (40, "2"),
                    (59, "1"),
                ]
            )
            session.sendRequestByFix(self.fixSubscriptionCorrelationId, request)


if __name__ == "__main__":
    if not os.environ.get("BINANCE_FIX_API_KEY"):
        print("Please set environment variable BINANCE_FIX_API_KEY", file=sys.stderr)
        sys.exit(1)
    if not os.environ.get("BINANCE_FIX_API_PRIVATE_KEY_PATH"):
        print("Please set environment variable BINANCE_FIX_API_PRIVATE_KEY_PATH", file=sys.stderr)
        sys.exit(1)
    fixSubscriptionCorrelationId = "any"
    eventHandler = MyEventHandler(fixSubscriptionCorrelationId)
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    subscription = Subscription("binance", "", "FIX", "", fixSubscriptionCorrelationId)
    session.subscribe(subscription)
    subscription = Subscription("", "", "HEARTBEAT", "HEARTBEAT_INTERVAL_MILLISECONDS=1000")
    session.subscribe(subscription)
    time.sleep(100)
    session.stop()
    print("Bye")
