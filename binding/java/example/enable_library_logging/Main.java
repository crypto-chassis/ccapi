// usage: when generating the binding code, do cmake -DCCAPI_ENABLE_LOG_TRACE=ON ..., see https://github.com/crypto-chassis/ccapi#non-c
import com.cryptochassis.ccapi.Logger;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;
import com.cryptochassis.ccapi.Subscription;
import java.util.concurrent.locks.ReentrantLock;

public class Main {
  static class MyLogger extends Logger {
    @Override
    public void logMessage(String severity, String threadId, String timeISO, String fileName, String lineNumber, String message) {
      this.lock.lock();
      try {
        System.out.println(String.format("%s: [%s] {%s:%s} %s%s%s", threadId, timeISO, fileName, lineNumber, severity, " ".repeat(8), message));
      } finally {
        this.lock.unlock();
      }
    }
    ReentrantLock lock = new ReentrantLock();
  }
  public static void main(String[] args) {
    System.loadLibrary("ccapi_binding_java");
    var myLogger = new MyLogger();
    Logger.setLogger(myLogger);
    var option = new SessionOptions();
    var config = new SessionConfigs();
    var session = new Session(option, config);
    var subscription = new Subscription("okx", "BTC-USDT", "MARKET_DEPTH");
    session.subscribe(subscription);
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
