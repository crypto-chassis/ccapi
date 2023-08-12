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
	if event.GetType() == ccapi.EventType_AUTHORIZATION_STATUS {
		fmt.Printf("Received an event of type AUTHORIZATION_STATUS:\n%s\n", event.ToStringPretty(2, 2))
		message := event.GetMessageList().Get(0)
		if message.GetType() == ccapi.MessageType_AUTHORIZATION_SUCCESS {
			request := ccapi.NewRequest(ccapi.RequestOperation_FIX, "coinbase", "", "same correlation id for subscription and request")
			defer ccapi.DeleteRequest(request)
			param := ccapi.NewVectorPairIntString()
			defer ccapi.DeleteVectorPairIntString(param)
			fixParams := []struct {
				tag   int
				value string
			}{
				{35, "D"},
				{11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
				{55, "BTC-USD"},
				{54, "1"},
				{44, "20000"},
				{38, "0.001"},
				{40, "2"},
				{59, "1"},
			}
			for _, fixParam := range fixParams {
				aParam := ccapi.NewPairIntString()
				aParam.SetFirst(fixParam.tag)
				aParam.SetSecond(fixParam.value)
				defer ccapi.DeletePairIntString(aParam)
				param.Add(aParam)
			}
			request.AppendParamFix(param)
			session.SendRequestByFix(request)
		}
	} else if event.GetType() == ccapi.EventType_FIX {
		fmt.Printf("Received an event of type FIX:\n%s\n", event.ToStringPretty(2, 2))
	}
	return true
}

func main() {
	if len(os.Getenv("COINBASE_API_KEY")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable COINBASE_API_KEY\n")
		os.Exit(1)
	}
	if len(os.Getenv("COINBASE_API_SECRET")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable COINBASE_API_SECRET\n")
		os.Exit(1)
	}
	if len(os.Getenv("COINBASE_API_PASSPHRASE")) == 0 {
		fmt.Fprintf(os.Stderr, "Please set environment variable COINBASE_API_PASSPHRASE\n")
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
	subscription := ccapi.NewSubscription("coinbase", "", "FIX", "", "same correlation id for subscription and request")
	defer ccapi.DeleteSubscription(subscription)
	session.SubscribeByFix(subscription)
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
