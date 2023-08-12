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
  try {
    throw new Error('oops')
  } catch (error) {
    console.error(error.stack)
    process.exit(1)
  }
}, 1)
setTimeout(() => {
  clearInterval(intervalId)
session.stop()
  console.log('Bye')
}, 10000)
