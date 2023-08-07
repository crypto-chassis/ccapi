class MainProgram {
  class MyEventHandler : ccapi.EventHandler {
    public override bool ProcessEvent(ccapi.Event event_, ccapi.Session session) {
      try {
        throw new System.Exception("oops");
      } catch (System.Exception e) {
        System.Console.WriteLine(e.ToString());
        System.Environment.Exit(1);
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
    subscriptionList.Add(new ccapi.Subscription("okx", "BTC-USDT", "MARKET_DEPTH"));
    session.Subscribe(subscriptionList);
    System.Threading.Thread.Sleep(10000);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
