from ccapi import Session
assert hasattr(Session, 'subscribe') and callable(getattr(Session, 'subscribe'))
assert hasattr(Session, 'sendRequest') and callable(getattr(Session, 'sendRequest'))
