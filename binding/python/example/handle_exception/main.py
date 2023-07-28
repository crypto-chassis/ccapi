import sys
import time
import traceback
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Event
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        try:
            raise Exception('oops')
        except Exception:
            print(traceback.format_exc())
            sys.exit(1)
        return True  # This line is needed.
if __name__ == '__main__':
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    subscription = Subscription('coinbase', 'BTC-USD', 'MARKET_DEPTH')
    session.subscribe(subscription)
    time.sleep(10)
    session.stop()
    print('Bye')
