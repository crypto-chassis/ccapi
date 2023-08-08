package main

import (
	"cryptochassis.com/ccapi"
	"fmt"
	"os"
	"time"
)

type MyEventHandler struct {
	ccapi.EventHandler
}

func (*MyEventHandler) ProcessEvent(event ccapi.Event, session ccapi.Session) bool {
	if event.GetType() == ccapi.EventType_SUBSCRIPTION_STATUS {
		fmt.Printf("Received an event of type SUBSCRIPTION_STATUS:\n%s\n", event.ToStringPretty(2, 2))
		message := event.GetMessageList().Get(0)
		if message.GetType() == ccapi.MessageType_SUBSCRIPTION_STARTED {
			request := ccapi.NewRequest(ccapi.RequestOperation_CREATE_ORDER, "okx", "BTC-USDT")
			defer ccapi.DeleteRequest(request)
			param := ccapi.NewMapStringString()
			defer ccapi.DeleteMapStringString(param)
			param.Set("SIDE", "BUY")
			param.Set("QUANTITY", "0.0005")
			param.Set("LIMIT_PRICE", "20000")
			request.AppendParam(param)
			session.SendRequest(request)
		}
	} else if event.GetType() == ccapi.EventType_SUBSCRIPTION_DATA {
		fmt.Printf("Received an event of type SUBSCRIPTION_DATA:\n%s\n", event.ToStringPretty(2, 2))
	}
	return true
}

func main() {
	if len(os.Getenv("OKX_API_KEY")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable OKX_API_KEY\n")
		os.Exit(1)
	}
	if len(os.Getenv("OKX_API_SECRET")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable OKX_API_SECRET\n")
		os.Exit(1)
	}
	if len(os.Getenv("OKX_API_PASSPHRASE")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable OKX_API_PASSPHRASE\n")
		os.Exit(1)
	}
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
	subscription := ccapi.NewSubscription("okx", "BTC-USDT", "ORDER_UPDATE")
	defer ccapi.DeleteSubscription(subscription)
	subscriptionList.Add(subscription)
	session.Subscribe(subscriptionList)
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
