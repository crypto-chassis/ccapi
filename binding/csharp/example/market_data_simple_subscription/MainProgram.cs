using ccapi;
class MainProgram
     {
      class MyEventHandler : EventHandler {
            public override bool ProcessEvent(Event event_, Session session) {
               if (event_.GetType_() == Event.Type.SUBSCRIPTION_DATA) {
                  foreach (var message in event_.GetMessageList()) {
                     System.Console.WriteLine(System.String.Format("Best bid and ask at {0} are:", message.GetTimeISO()));
                     foreach (var element in message.GetElementList()){
                        var elementNameValueMap = element.GetNameValueMap();
                        foreach(var entry in elementNameValueMap)
                        {
                        var name = entry.Key;
                        var value = entry.Value;
                        System.Console.WriteLine(System.String.Format("  {0} = {1}", name, value));
                        }
                     }
                  }
               }
               return true;
         }
      }
        static void Main(string[] args)
        {
            var eventHandler = new MyEventHandler();
            var option = new SessionOptions();
            var config = new SessionConfigs();
            var session = new Session(option, config, eventHandler);
            var subscriptionList = new SubscriptionList();
            subscriptionList.Add(new Subscription("okx", "BTC-USDT", "MARKET_DEPTH"));
            session.Subscribe(subscriptionList);
                  System.Threading.Thread.Sleep(10000);
            session.Stop();
            System.Console.WriteLine("Bye");
        }
     }
