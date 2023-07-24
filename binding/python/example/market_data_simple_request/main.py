import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Request, Event
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        print(f'Received an event:\n{event.toStringPretty(2, 2)}')
        return True  # This line is needed.
if __name__ == '__main__':
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    request = Request(Request.Operation_GET_RECENT_TRADES, 'coinbase', 'BTC-USD')
    request.appendParam({
        'LIMIT':'1',
    })
    session.sendRequest(request)
    time.sleep(10)
    session.stop()
    print('Bye')
