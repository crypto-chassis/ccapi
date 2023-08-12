const ccapi = require('ccapi')
if (!process.env.COINBASE_API_KEY) {
  console.error('Please set environment variable COINBASE_API_KEY')
  process.exit(1)
}
if (!process.env.COINBASE_API_SECRET) {
  console.error('Please set environment variable COINBASE_API_SECRET')
  process.exit(1)
}
if (!process.env.COINBASE_API_PASSPHRASE) {
  console.error('Please set environment variable COINBASE_API_PASSPHRASE')
  process.exit(1)
}
const Session = ccapi.Session
const SessionConfigs = ccapi.SessionConfigs
const SessionOptions = ccapi.SessionOptions
const Subscription = ccapi.Subscription
const Request = ccapi.Request
const VectorPairIntString = ccapi.VectorPairIntString
const PairIntString = ccapi.PairIntString
const Event = ccapi.Event
const Message = ccapi.Message
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const subscription = new Subscription('coinbase', '', 'FIX', '', 'same correlation id for subscription and request')
session.subscribeByFix(subscription)
const intervalId = setInterval(() => {
  const eventList = session.getEventQueue().purge()
  for (let i = 0; i < eventList.size(); i++) {
    const event = eventList.get(i)
    console.log(`Received an event of type any:\n${event.toStringPretty(2, 2)}`)
    if (event.getType() == Event.Type_AUTHORIZATION_STATUS) {
      const message = event.getMessageList().get(0)
      if (message.getType() == Message.Type_AUTHORIZATION_SUCCESS) {
        const request = new Request(ccapi.Request.Operation_FIX, 'coinbase', '', 'same correlation id for subscription and request')
        const param = new VectorPairIntString()
        param.add(new PairIntString(35, 'D'))
        param.add(new PairIntString(11, '6d4eb0fb-2229-469f-873e-557dd78ac11e'))
        param.add(new PairIntString(55, 'BTC-USD'))
        param.add(new PairIntString(54, '1'))
        param.add(new PairIntString(44, '20000'))
        param.add(new PairIntString(38, '0.001'))
        param.add(new PairIntString(40, '2'))
        param.add(new PairIntString(59, '1'))
        request.appendParamFix(param)
        session.sendRequestByFix(request)
      }
    }
    else if (event.getType() == ccapi.Event.Type_FIX) {
      console.log(`Received an event of type FIX:\n${event.toStringPretty(2, 2)}`)
    }
  }
}, 1)
setTimeout(() => {
  clearInterval(intervalId)
session.stop()
  console.log('Bye')
}, 10000)
