// usage: when generating the binding code, do cmake -DCCAPI_ENABLE_LOG_TRACE=ON ..., see https://github.com/crypto-chassis/ccapi#non-c
package main

import (
	"cryptochassis.com/ccapi"
	"fmt"
	"strings"
	"sync"
	"time"
)

type MyLogger struct {
	ccapi.Logger
	m sync.Mutex
}

func (ml *MyLogger) LogMessage(severity string, threadId string, timeISO string, fileName string, lineNumber string, message string) {
	ml.m.Lock()
	defer ml.m.Unlock()
	fmt.Printf("%s: [%s] {%s:%s} %s%s%s\n", threadId, timeISO, fileName, lineNumber, severity, strings.Repeat(" ", 8), message)
}

func main() {
	ml := &MyLogger{}
	ml.Logger = ccapi.NewDirectorLogger(ml)
	defer func() {
		ccapi.DeleteDirectorLogger(ml.Logger)
	}()
	ccapi.SetLoggerLogger(ml)
	option := ccapi.NewSessionOptions()
	defer ccapi.DeleteSessionOptions(option)
	config := ccapi.NewSessionConfigs()
	defer ccapi.DeleteSessionConfigs(config)
	session := ccapi.NewSession(option, config)
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
