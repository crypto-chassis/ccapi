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
	if event.GetType() == ccapi.EventType_SUBSCRIPTION_DATA {
		messageList := event.GetMessageList()
		for i := 0; i < int(messageList.Size()); i++ {
			message := messageList.Get(i)
			correlationId := message.GetCorrelationIdList().Get(0)
			fmt.Printf("%s: Best bid and ask at %s are:\n", correlationId, message.GetTimeISO())
			elementList := message.GetElementList()
			for j := 0; j < int(elementList.Size()); j++ {
				element := elementList.Get(j)
				nameValueMap := element.GetNameValueMap()
				if nameValueMap.Has_key("BID_PRICE") {
					fmt.Printf("  %s = %s\n", "BID_PRICE", nameValueMap.Get("BID_PRICE"))
				}
				if nameValueMap.Has_key("BID_SIZE") {
					fmt.Printf("  %s = %s\n", "BID_SIZE", nameValueMap.Get("BID_SIZE"))
				}
				if nameValueMap.Has_key("ASK_PRICE") {
					fmt.Printf("  %s = %s\n", "ASK_PRICE", nameValueMap.Get("ASK_PRICE"))
				}
				if nameValueMap.Has_key("ASK_SIZE") {
					fmt.Printf("  %s = %s\n", "ASK_SIZE", nameValueMap.Get("ASK_SIZE"))
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
	{
		subscription := ccapi.NewSubscription("okx", "BTC-USDT", "MARKET_DEPTH", "", "BTC")
		defer ccapi.DeleteSubscription(subscription)
		subscriptionList.Add(subscription)
	}
	{
		subscription := ccapi.NewSubscription("okx", "ETH-USDT", "MARKET_DEPTH", "", "ETH")
		defer ccapi.DeleteSubscription(subscription)
		subscriptionList.Add(subscription)
	}

	session.Subscribe(subscriptionList)
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
