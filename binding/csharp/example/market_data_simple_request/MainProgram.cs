class MainProgram {
  class MyEventHandler : ccapi.EventHandler {
    public override bool ProcessEvent(ccapi.Event event_, ccapi.Session session) {
      System.Console.WriteLine(string.Format("Received an event:\n{0}", event_.ToStringPretty(2, 2)));
      return true;
    }
  }
  static void Main(string[] args) {
    var eventHandler = new MyEventHandler();
    var option = new ccapi.SessionOptions();
    var config = new ccapi.SessionConfigs();
    var session = new ccapi.Session(option, config, eventHandler);
    var request = new ccapi.Request(ccapi.Request.Operation.GET_RECENT_TRADES, "okx", "BTC-USDT");
    var param = new ccapi.MapStringString();
    param.Add("LIMIT", "1");
    request.AppendParam(param);
    session.SendRequest(request);
    System.Threading.Thread.Sleep(10000);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
