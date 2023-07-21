using ccapi;
class MainProgram
     {
      class MyEventHandler : EventHandler {
            public override bool ProcessEvent(Event event_, Session session) {
               System.Console.WriteLine(System.String.Format("Received an event:\n%s", event_.ToStringPretty(2, 2)));
               return true;
         }
      }
        static void Main(string[] args)
        {
         if(System.Environment.GetEnvironmentVariable("OKX_API_KEY") is null) {
            System.Console.Error.WriteLine("Please set environment variable OKX_API_KEY");
            return;
        }
        if(System.Environment.GetEnvironmentVariable("OKX_API_SECRET") is null) {
            System.Console.Error.WriteLine("Please set environment variable OKX_API_SECRET");
            return;
        }
        if(System.Environment.GetEnvironmentVariable("OKX_API_PASSPHRASE") is null) {
            System.Console.Error.WriteLine("Please set environment variable OKX_API_PASSPHRASE");
            return;
        }
            var eventHandler = new MyEventHandler();
            var option = new SessionOptions();
            var config = new SessionConfigs();
            var session = new Session(option, config, eventHandler);
            var request = new Request(Request.Operation.CREATE_ORDER, "okx", "BTC-USDT");
        var param = new MapStringString();
        param.Add("SIDE","BUY");
        param.Add("QUANTITY","0.0005");
        param.Add("LIMIT_PRICE","20000");
        request.AppendParam(param);
        session.SendRequest(request);
                  System.Threading.Thread.Sleep(10000);
            session.Stop();
            System.Console.WriteLine("Bye");
        }
     }
