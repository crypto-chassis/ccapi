const ccapi = require('ccapi')
const Session = ccapi.Session
const SessionConfigs = ccapi.SessionConfigs
const SessionOptions = ccapi.SessionOptions
const Request = ccapi.Request
const MapStringString = ccapi.MapStringString
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const request = new Request(Request.Operation_GET_RECENT_TRADES, 'okx', 'BTC-USDT')
const param = new MapStringString()
param.set('LIMIT', '1')
request.appendParam(param)
session.sendRequest(request)
const intervalId = setInterval(() => {
  const eventList = session.getEventQueue().purge()
  for (let i = 0; i < eventList.size(); i++) {
    const event = eventList.get(i)
    console.log(`Received an event:\n${event.toStringPretty(2, 2)}`)
  }
}, 1)
setTimeout(() => {
  clearInterval(intervalId)
session.stop()
  console.log('Bye')
}, 10000)
