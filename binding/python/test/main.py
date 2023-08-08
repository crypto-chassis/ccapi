import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Event, Request
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        return True  # This line is needed.
if __name__ == '__main__':
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    subscription = Subscription('okx', 'BTC-USDT', 'MARKET_DEPTH')
    session.subscribe(subscription)
    request = Request(Request.Operation_GET_RECENT_TRADES, 'okx', 'BTC-USDT')
    request.appendParam({
        'LIMIT':'1',
    })
    session.sendRequest(request)
    session.stop()
    print('Bye')
