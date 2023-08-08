class MainProgram {
  class MyEventHandler : ccapi.EventHandler {
    public override bool ProcessEvent(ccapi.Event event_, ccapi.Session session) {
      return true;
    }
  }
  static void Main(string[] args) {
    var eventHandler = new MyEventHandler();
    var option = new ccapi.SessionOptions();
    var config = new ccapi.SessionConfigs();
    var session = new ccapi.Session(option, config, eventHandler);
    var subscriptionList = new ccapi.SubscriptionList();
    subscriptionList.Add(new ccapi.Subscription("okx", "BTC-USDT", "MARKET_DEPTH"));
    session.Subscribe(subscriptionList);
    var request = new ccapi.Request(ccapi.Request.Operation.GET_RECENT_TRADES, "okx", "BTC-USDT");
    var param = new ccapi.MapStringString();
    param.Add("LIMIT", "1");
    request.AppendParam(param);
    session.SendRequest(request);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
