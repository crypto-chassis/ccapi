import os
import sys
import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Request, Event, Message
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        if event.getType() == Event.Type_AUTHORIZATION_STATUS:
            print(f'Received an event of type AUTHORIZATION_STATUS:\n{event.toStringPretty(2, 2)}')
            message = event.getMessageList()[0]
            if message.getType() == Message.Type_AUTHORIZATION_SUCCESS:
                request = Request(Request.Operation_FIX, 'coinbase', '', 'same correlation id for subscription and request')
                request.appendParamFix([
                    (35,'D'),
                    (11,'6d4eb0fb-2229-469f-873e-557dd78ac11e'),
                    (55,'BTC-USD'),
                    (54,'1'),
                    (44,'20000'),
                    (38,'0.001'),
                    (40,'2'),
                    (59,'1'),
                ])
                session.sendRequestByFix(request)
        elif event.getType() == Event.Type_FIX:
            print(f'Received an event of type FIX:\n{event.toStringPretty(2, 2)}')
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
    subscription = Subscription('coinbase', '', 'FIX', '', 'same correlation id for subscription and request')
    session.subscribeByFix(subscription)
    time.sleep(10)
    session.stop()
    print('Bye')
