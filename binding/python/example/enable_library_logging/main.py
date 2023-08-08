# usage: when generating the binding code, do cmake -DCCAPI_ENABLE_LOG_TRACE=ON ..., see https://github.com/crypto-chassis/ccapi#non-c
from threading import Lock
import time
from ccapi import Logger, Session, Subscription
class MyLogger(Logger):
    def __init__(self):
        super().__init__()
        self._lock = Lock()
    def logMessage(self, severity: str, threadId: str, timeISO: str, fileName: str, lineNumber: str, message: str) -> None:
        self._lock.acquire()
        print(f'{threadId}: [{timeISO}] {{{fileName}:{lineNumber}}} {severity}{" " * 8}{message}')
        self._lock.release()
myLogger = MyLogger()
Logger.logger = myLogger
if __name__ == '__main__':
    session = Session()
    subscription = Subscription('coinbase', 'BTC-USD', 'MARKET_DEPTH')
    session.subscribe(subscription)
    time.sleep(10)
    session.stop()
    print('Bye')
    Logger.logger = None
