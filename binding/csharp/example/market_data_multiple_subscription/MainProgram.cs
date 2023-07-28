class MainProgram {
  class MyEventHandler : ccapi.EventHandler {
    public override bool ProcessEvent(ccapi.Event event_, ccapi.Session session) {
      if (event_.GetType_() == ccapi.Event.Type.SUBSCRIPTION_DATA) {
        foreach (var message in event_.GetMessageList()) {
          var correlationId = message.GetCorrelationIdList()[0];
          System.Console.WriteLine(string.Format("{0}: Best bid and ask at {1} are:", correlationId, message.GetTimeISO()));
          foreach (var element in message.GetElementList()) {
            var elementNameValueMap = element.GetNameValueMap();
            foreach (var entry in elementNameValueMap) {
              var name = entry.Key;
              var value = entry.Value;
              System.Console.WriteLine(string.Format("  {0} = {1}", name, value));
            }
          }
        }
      }
      return true;
    }
  }
  static void Main(string[] args) {
    var eventHandler = new MyEventHandler();
    var option = new ccapi.SessionOptions();
    var config = new ccapi.SessionConfigs();
    var session = new ccapi.Session(option, config, eventHandler);
    var subscriptionList = new ccapi.SubscriptionList();
    subscriptionList.Add(new ccapi.Subscription("okx", "BTC-USDT", "MARKET_DEPTH", "", "BTC"));
    subscriptionList.Add(new ccapi.Subscription("okx", "ETH-USDT", "MARKET_DEPTH", "", "ETH"));
    session.Subscribe(subscriptionList);
    System.Threading.Thread.Sleep(10000);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
