import os
import sys
import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Request, Event, Message
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        if event.getType() == Event.Type_SUBSCRIPTION_STATUS:
          print(f'Received an event of type SUBSCRIPTION_STATUS:\n{event.toStringPretty(2, 2)}')
          message = event.getMessageList()[0]
          if message.getType() == Message.Type_SUBSCRIPTION_STARTED:
              request = Request(Request.Operation_CREATE_ORDER, 'coinbase', 'BTC-USD')
              request.appendParam({
                  'SIDE':'BUY',
                  'LIMIT_PRICE':'20000',
                  'QUANTITY':'0.001',
              })
              session.sendRequest(request)
        elif event.getType() == Event.Type_SUBSCRIPTION_DATA:
          print(f'Received an event of type SUBSCRIPTION_DATA:\n{event.toStringPretty(2, 2)}')
        return True  # This line is needed.
if __name__ == '__main__':
    if not os.environ.get('COINBASE_API_KEY'):
        print('Please set environment variable COINBASE_API_KEY', file=sys.stderr)
        sys.exit(1)
    if not os.environ.get('COINBASE_API_SECRET'):
        print('Please set environment variable COINBASE_API_SECRET', file=sys.stderr)
        sys.exit(1)
    if not os.environ.get('COINBASE_API_PASSPHRASE'):
        print('Please set environment variable COINBASE_API_PASSPHRASE', file=sys.stderr)
        sys.exit(1)
    eventHandler = MyEventHandler()
    option = SessionOptions()
    config = SessionConfigs()
    session = Session(option, config, eventHandler)
    subscription = Subscription('coinbase', 'BTC-USD', 'ORDER_UPDATE')
    session.subscribe(subscription)
    time.sleep(10)
    session.stop()
    print('Bye')
