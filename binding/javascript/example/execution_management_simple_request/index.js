const ccapi = require('ccapi')
if (!process.env.OKX_API_KEY) {
  console.error('Please set environment variable OKX_API_KEY')
  process.exit(1)
}
if (!process.env.OKX_API_SECRET) {
  console.error('Please set environment variable OKX_API_SECRET')
  process.exit(1)
}
if (!process.env.OKX_API_PASSPHRASE) {
  console.error('Please set environment variable OKX_API_PASSPHRASE')
  process.exit(1)
}
const Session = ccapi.Session
const SessionConfigs = ccapi.SessionConfigs
const SessionOptions = ccapi.SessionOptions
const Request = ccapi.Request
const MapStringString = ccapi.MapStringString
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const request = new Request(Request.Operation_CREATE_ORDER, 'okx', 'BTC-USDT')
const param = new MapStringString()
param.set('SIDE', 'BUY')
param.set('QUANTITY', '0.0005')
param.set('LIMIT_PRICE', '20000')
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
