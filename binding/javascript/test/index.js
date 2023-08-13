const ccapi = require('ccapi')
const Session = ccapi.Session
const SessionConfigs = ccapi.SessionConfigs
const SessionOptions = ccapi.SessionOptions
const Subscription = ccapi.Subscription
const Request = ccapi.Request
const MapStringString = ccapi.MapStringString
const Event = ccapi.Event
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const subscription = new Subscription('okx', 'BTC-USDT', 'MARKET_DEPTH')
session.subscribe(subscription)
const request = new Request(Request.Operation_GET_RECENT_TRADES, 'okx', 'BTC-USDT')
const param = new MapStringString()
param.set('LIMIT', '1')
request.appendParam(param)
session.sendRequest(request)
setTimeout(() => {
  session.stop()
  console.log('Bye')
}, 1)
