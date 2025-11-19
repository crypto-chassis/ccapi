import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Event


class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()

    def processEvent(self, event: Event, session: Session) -> None:
        if event.getType() == Event.Type_HEARTBEAT:
            print(f"Received an event of type HEARTBEAT:\n{event.toPrettyString(2, 2)}")


if __name__ == "__main__":
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    subscription = Subscription("", "", "HEARTBEAT", "HEARTBEAT_INTERVAL_MILLISECONDS=1000")
    session.subscribe(subscription)
    time.sleep(10)
    session.stop()
    print("Bye")
