import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Request, Event


class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()

    def processEvent(self, event: Event, session: Session) -> None:
        print(f"Received an event:\n{event.toPrettyString(2, 2)}")


if __name__ == "__main__":
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    request = Request(Request.Operation_GET_RECENT_TRADES, "coinbase", "BTC-USD")
    request.appendParam(
        {
            "LIMIT": "1",
        }
    )
    session.sendRequest(request)
    time.sleep(10)
    session.stop()
    print("Bye")
