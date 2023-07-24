import os
import sys
import time
from ccapi import Event, SessionOptions, SessionConfigs, Session, EventHandler, Request
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        print(f'Received an event:\n{event.toStringPretty(2, 2)}')
        return True  # This line is needed.
if __name__ == '__main__':
    if not os.environ.get('BINANCE_US_API_KEY'):
        print('Please set environment variable BINANCE_US_API_KEY', file=sys.stderr)
        sys.exit(1)
    if not os.environ.get('BINANCE_US_API_SECRET'):
        print('Please set environment variable BINANCE_US_API_SECRET', file=sys.stderr)
        sys.exit(1)
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    request = Request(Request.Operation_CREATE_ORDER, 'binance-us', 'BTCUSD')
    request.appendParam({
        'SIDE':'BUY',
        'QUANTITY':'0.0005',
        'LIMIT_PRICE':'20000',
    })
    session.sendRequest(request)
    time.sleep(10)
    session.stop()
    print('Bye')
