import faulthandler; faulthandler.enable()
import time
import logging
import threading
from ccapi import Element, Message, Event, SessionOptions, SessionConfigs, Session, EventHandler, Subscription, Request, Logger
class MyLogger(Logger):
    def __init__(self):
        super().__init__()
    def logMessage(self, ccapi_utilseverity, ccapi_utilthreadId, ccapi_utiltimeISO, capi_utilfileName, lineNumber, ccapi_utilmessage):
        pass
        # Logger.logMessage(self, ccapi_utilseverity, ccapi_utilthreadId, ccapi_utiltimeISO, capi_utilfileName, lineNumber, ccapi_utilmessage)
        # logging.warning(f'{threading.get_ident()}, {ccapi_utilseverity}, {ccapi_utilthreadId}, {ccapi_utiltimeISO}, {capi_utilfileName}, {lineNumber}, {ccapi_utilmessage}')
myLogger = MyLogger()
Logger.logger = myLogger
# element = Element()
# element.insert('a', 'b')
# print(element.toString())
# message = Message()
# message.setElementList([element])
# print(message.toString())
# for e in message.getElementList():
#     print(e.toString())
# message.setCorrelationIdList(['c'])
# for c in message.getCorrelationIdList():
#     print(c)
# print(message.getTimeISO())
# print(message.getTimeReceivedISO())
# print(message.getRecapType() == message.RecapType_UNKNOWN)
# print(message.getType() == message.Type_UNKNOWN)
option = SessionOptions()
# print(option.warnLateEventMaxMilliSeconds)
# option.warnLateEventMaxMilliSeconds = 1
# print(option.warnLateEventMaxMilliSeconds)
# print(option.enableCheckSequence)
# print(option.httpMaxNumRetry)
# exchangePairSymbolMap = {
#     'a': {
#         'b':'c'
#     }
# }
config = SessionConfigs()
# for (k, v) in config.getExchangeInstrumentSymbolMap().items():
#     print(f'k = {k}')
#     print(f'v={dict(v)}')
class MyEventHandler(EventHandler):
    def __init__(self):
        # EventHandler.__init__(self)
        super().__init__()
    # def foo(self):
    #     print('python foo')
    def processEvent(self, event, session):
        logging.warning(f'{threading.get_ident()} python process event start')
        logging.warning(f'python event = {event.toString()}')
        # print(session.sessionOptions.toString())
        return True
eventHandler = MyEventHandler()

session = Session(option, config, eventHandler)
# event = Event()
# print(event.toString())
# q = session.getEventQueue()
# q.pushBack(event)
# print(q.size())
# for e in q.purge():
#     print(e.toString())
# print(q.size())
subscription = Subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "", "BTC")
# print(f'subscription = {subscription.toString()}')
subscription2 = Subscription("coinbase", "ETH-USD", "MARKET_DEPTH", "", "ETH")
session.subscribe([subscription,subscription2])

# while True:
#     a = session.getEventQueue().purge()
#     for item in a:
#         print(f'item = {item.toString()}')
#     time.sleep(1)
time.sleep(2)

print('before stop')
session.stop()
Logger.logger = None
print('after stop')
    # time.sleep(10)
