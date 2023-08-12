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
const Subscription = ccapi.Subscription
const Request = ccapi.Request
const MapStringString = ccapi.MapStringString
const Event = ccapi.Event
const Message = ccapi.Message
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const subscription = new Subscription('okx', 'BTC-USDT', 'ORDER_UPDATE')
session.subscribe(subscription)
const intervalId = setInterval(() => {
  const eventList = session.getEventQueue().purge()
  for (let i = 0; i < eventList.size(); i++) {
    const event = eventList.get(i)
    if (event.getType() == Event.Type_SUBSCRIPTION_STATUS) {
      console.log(`Received an event of type SUBSCRIPTION_STATUS:\n${event.toStringPretty(2, 2)}`)
      const message = event.getMessageList().get(0)
      if (message.getType() == Message.Type_SUBSCRIPTION_STARTED) {
        const request = new Request(Request.Operation_CREATE_ORDER, 'okx', 'BTC-USDT')
        const param = new MapStringString()
        param.set('SIDE', 'BUY')
        param.set('QUANTITY', '0.0005')
        param.set('LIMIT_PRICE', '20000')
        request.appendParam(param)
        session.sendRequest(request)
      }
    }
    else if (event.getType() == ccapi.Event.Type_SUBSCRIPTION_DATA) {
      console.log(`Received an event of type SUBSCRIPTION_DATA:\n${event.toStringPretty(2, 2)}`)
    }
  }
}, 1)
setTimeout(() => {
  clearInterval(intervalId)
session.stop()
  console.log('Bye')
}, 10000)
