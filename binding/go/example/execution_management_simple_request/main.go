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
	fmt.Printf("Received an event:\n%s\n", event.ToStringPretty(2, 2))
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
	request := ccapi.NewRequest(ccapi.RequestOperation_CREATE_ORDER, "okx", "BTC-USDT")
	defer ccapi.DeleteRequest(request)
	param := ccapi.NewMapStringString()
	defer ccapi.DeleteMapStringString(param)
	param.Set("SIDE", "BUY")
	param.Set("QUANTITY", "0.0005")
	param.Set("LIMIT_PRICE", "20000")
	request.AppendParam(param)
	session.SendRequest(request)
	time.Sleep(10 * time.Second)
	session.Stop()
	fmt.Println("Bye")
}
