const ccapi = require('ccapi')
const Session = ccapi.Session
const SessionConfigs = ccapi.SessionConfigs
const SessionOptions = ccapi.SessionOptions
const Subscription = ccapi.Subscription
const Event = ccapi.Event
const sessionConfigs = new SessionConfigs()
const sessionOptions = new SessionOptions()
const session = new Session(sessionOptions, sessionConfigs)
const subscription = new Subscription('okx', 'BTC-USDT', 'MARKET_DEPTH')
session.subscribe(subscription)
const intervalId = setInterval(() => {
  const eventList = session.getEventQueue().purge()
  for (let i = 0; i < eventList.size(); i++) {
    const event = eventList.get(i)
    if (event.getType() == Event.Type_SUBSCRIPTION_STATUS) {
      console.log(`Received an event of type SUBSCRIPTION_STATUS:\n${event.toStringPretty(2, 2)}`)
    }
    else if (event.getType() == Event.Type_SUBSCRIPTION_DATA) {
      const messageList = event.getMessageList()
      for (let j = 0; j < messageList.size(); j++) {
        const message = messageList.get(j)
        console.log(`Best bid and ask at ${message.getTimeISO()} are:`)
        const elementList = message.getElementList()
        for (let k = 0; k < elementList.size(); k++) {
          const element = elementList.get(k)
          elementNameValueMap = element.getNameValueMap()
          if (elementNameValueMap.has_key('BID_PRICE')) {
            console.log(`  BID_PRICE = ${elementNameValueMap.get('BID_PRICE')}`)
          }
          if (elementNameValueMap.has_key('BID_SIZE')) {
            console.log(`  BID_SIZE = ${elementNameValueMap.get('BID_SIZE')}`)
          }
          if (elementNameValueMap.has_key('ASK_PRICE')) {
            console.log(`  ASK_PRICE = ${elementNameValueMap.get('ASK_PRICE')}`)
          }
          if (elementNameValueMap.has_key('ASK_SIZE')) {
            console.log(`  ASK_SIZE = ${elementNameValueMap.get('ASK_SIZE')}`)
          }
        }
      }
    }
  }
}, 1)
setTimeout(() => {
  clearInterval(intervalId)
session.stop()
  console.log('Bye')
}, 10000)
