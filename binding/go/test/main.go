package main

import (
	"cryptochassis.com/ccapi"
	"fmt"
	"time"
)

type MyEventHandler struct {
	ccapi.EventHandler
}

func (*MyEventHandler) ProcessEvent(event ccapi.Event, session ccapi.Session) bool {
	return true
}

func main() {
	option := ccapi.NewSessionOptions()
	defer ccapi.DeleteSessionOptions(option)
	config := ccapi.NewSessionConfigs()
	defer ccapi.DeleteSessionConfigs(config)
	meh := &MyEventHandler{}
	meh.EventHandler = ccapi.NewDirectorEventHandler(meh)
	defer func() {
		ccapi.DeleteDirectorEventHandler(meh.EventHandler)
	}()
	session := ccapi.NewSession(option, config, meh.EventHandler)
	defer ccapi.DeleteSession(session)
	subscriptionList := ccapi.NewSubscriptionList()
	defer ccapi.DeleteSubscriptionList(subscriptionList)
	subscription := ccapi.NewSubscription("okx", "BTC-USDT", "MARKET_DEPTH")
	defer ccapi.DeleteSubscription(subscription)
	subscriptionList.Add(subscription)
	session.Subscribe(subscriptionList)
	request := ccapi.NewRequest(ccapi.RequestOperation_GET_RECENT_TRADES, "okx", "BTC-USDT")
	defer ccapi.DeleteRequest(request)
	param := ccapi.NewMapStringString()
	defer ccapi.DeleteMapStringString(param)
	param.Set("LIMIT", "1")
	request.AppendParam(param)
	session.SendRequest(request)
	time.Sleep(time.Second)
	session.Stop()
	fmt.Println("Bye")
}
