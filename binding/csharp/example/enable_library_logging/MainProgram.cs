// usage: when generating the binding code, do cmake -DCCAPI_ENABLE_LOG_TRACE=ON ..., see https://github.com/crypto-chassis/ccapi#non-c
public class MainProgram {
  class MyLogger : ccapi.Logger {
    public override void LogMessage(string severity, string threadId, string timeISO, string fileName, string lineNumber, string message) {
      lock (this._myLock) {
        System.Console.WriteLine(
            string.Format("{0}: [{1}] {{{2}:{3}}} {4}{5}{6}", threadId, timeISO, fileName, lineNumber, severity, new string(' ', 8), message));
      }
    }
    private readonly object _myLock = new object();
  }
  public static void Main(string[] args) {
    var myLogger = new MyLogger();
    ccapi.Logger.logger = myLogger;
    var option = new ccapi.SessionOptions();
    var config = new ccapi.SessionConfigs();
    var session = new ccapi.Session(option, config);
    var subscription = new ccapi.Subscription("okx", "BTC-USDT", "MARKET_DEPTH");
    session.Subscribe(subscription);
    System.Threading.Thread.Sleep(10000);
    session.Stop();
    System.Console.WriteLine("Bye");
  }
}
