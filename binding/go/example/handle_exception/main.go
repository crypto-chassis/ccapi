package main

import (
	"cryptochassis.com/ccapi"
	"fmt"
	"os"
	"runtime/debug"
	"time"
)

type MyEventHandler struct {
	ccapi.EventHandler
}

func (*MyEventHandler) ProcessEvent(event ccapi.Event, session ccapi.Session) bool {
	defer func() {
		if panicInfo := recover(); panicInfo != nil {
			fmt.Printf("%v, %s", panicInfo, string(debug.Stack()))
			os.Exit(1)
		}
	}()
	panic("oops")
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
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
