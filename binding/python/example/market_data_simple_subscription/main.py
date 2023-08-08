import time
from ccapi import EventHandler, SessionOptions, SessionConfigs, Session, Subscription, Event
class MyEventHandler(EventHandler):
    def __init__(self):
        super().__init__()
    def processEvent(self, event: Event, session: Session) -> bool:
        if event.getType() == Event.SUBSCRIPTION_STATUS:
            print(f'Received an event of type SUBSCRIPTION_STATUS:\n{event.toStringPretty(2, 2)}')
        elif event.getType() == Event.Type_SUBSCRIPTION_DATA:
            for message in event.getMessageList():
                print(f'Best bid and ask at {message.getTimeISO()} are:')
                for element in message.getElementList():
                    elementNameValueMap = element.getNameValueMap()
                    for name, value in elementNameValueMap.items():
                        print(f'  {name} = {value}')
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
    time.sleep(10)
    session.stop()
    print('Bye')
