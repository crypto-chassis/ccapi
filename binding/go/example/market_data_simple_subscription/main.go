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
	if event.GetType() == ccapi.EventType_SUBSCRIPTION_STATUS {
		fmt.Printf("Received an event of type SUBSCRIPTION_STATUS:\n%s\n", event.ToStringPretty(2, 2))
	} else if event.GetType() == ccapi.EventType_SUBSCRIPTION_DATA {
		messageList := event.GetMessageList()
		for i := 0; i < int(messageList.Size()); i++ {
			message := messageList.Get(i)
			fmt.Printf("Best bid and ask at %s are:\n", message.GetTimeISO())
			elementList := message.GetElementList()
			for j := 0; j < int(elementList.Size()); j++ {
				element := elementList.Get(j)
				elementNameValueMap := element.GetNameValueMap()
				if elementNameValueMap.Has_key("BID_PRICE") {
					fmt.Printf("  BID_PRICE = %s\n", elementNameValueMap.Get("BID_PRICE"))
				}
				if elementNameValueMap.Has_key("BID_SIZE") {
					fmt.Printf("  BID_SIZE = %s\n", elementNameValueMap.Get("BID_SIZE"))
				}
				if elementNameValueMap.Has_key("ASK_PRICE") {
					fmt.Printf("  ASK_PRICE = %s\n", elementNameValueMap.Get("ASK_PRICE"))
				}
				if elementNameValueMap.Has_key("ASK_SIZE") {
					fmt.Printf("  ASK_SIZE = %s\n", elementNameValueMap.Get("ASK_SIZE"))
				}
			}
		}
	}
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
