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
	fmt.Printf("Received an event:\n%s\n", event.ToStringPretty(2, 2))
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
	request := ccapi.NewRequest(ccapi.RequestOperation_GET_RECENT_TRADES, "okx", "BTC-USDT")
	defer ccapi.DeleteRequest(request)
	param := ccapi.NewMapStringString()
	defer ccapi.DeleteMapStringString(param)
	param.Set("LIMIT", "1")
	request.AppendParam(param)
	session.SendRequest(request)
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
