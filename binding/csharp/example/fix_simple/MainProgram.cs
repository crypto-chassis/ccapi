class MainProgram {
  class MyEventHandler : ccapi.EventHandler {
    public override bool ProcessEvent(ccapi.Event event_, ccapi.Session session) {
      if (event_.GetType_() == ccapi.Event.Type.AUTHORIZATION_STATUS) {
        System.Console.WriteLine(string.Format("Received an event of type AUTHORIZATION_STATUS:\n{0}", event_.ToStringPretty(2, 2)));
        var message = event_.GetMessageList()[0];
        if (message.GetType_() == ccapi.Message.Type.AUTHORIZATION_SUCCESS) {
          var request = new ccapi.Request(ccapi.Request.Operation.FIX, "coinbase", "", "same correlation id for subscription and request");
          var param = new ccapi.VectorPairIntString();
          param.Add(new ccapi.PairIntString(35, "D"));
          param.Add(new ccapi.PairIntString(11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"));
          param.Add(new ccapi.PairIntString(55, "BTC-USD"));
          param.Add(new ccapi.PairIntString(54, "1"));
          param.Add(new ccapi.PairIntString(44, "20000"));
          param.Add(new ccapi.PairIntString(38, "0.001"));
          param.Add(new ccapi.PairIntString(40, "2"));
          param.Add(new ccapi.PairIntString(59, "1"));
          request.AppendParamFix(param);
          session.SendRequestByFix(request);
        }
      } else if (event_.GetType_() == ccapi.Event.Type.FIX) {
        System.Console.WriteLine(string.Format("Received an event of type FIX:\n{0}", event_.ToStringPretty(2, 2)));
      }
      return true;
    }
  }
  static void Main(string[] args) {
    if (System.Environment.GetEnvironmentVariable("COINBASE_API_KEY") is null) {
      System.Console.Error.WriteLine("Please set environment variable COINBASE_API_KEY");
      return;
    }
    if (System.Environment.GetEnvironmentVariable("COINBASE_API_SECRET") is null) {
      System.Console.Error.WriteLine("Please set environment variable COINBASE_API_SECRET");
      return;
    }
    if (System.Environment.GetEnvironmentVariable("COINBASE_API_PASSPHRASE") is null) {
      System.Console.Error.WriteLine("Please set environment variable COINBASE_API_PASSPHRASE");
      return;
    }
    var eventHandler = new MyEventHandler();
    var option = new ccapi.SessionOptions();
    var config = new ccapi.SessionConfigs();
    var session = new ccapi.Session(option, config, eventHandler);
    var subscription = new ccapi.Subscription("coinbase", "", "FIX", "", "same correlation id for subscription and request");
    session.SubscribeByFix(subscription);
    System.Threading.Thread.Sleep(10000);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
