import com.cryptochassis.ccapi.Event;
import com.cryptochassis.ccapi.EventHandler;
import com.cryptochassis.ccapi.MapStringString;
import com.cryptochassis.ccapi.Request;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;

public class Main {
  static class MyEventHandler extends EventHandler {
    @Override
    public boolean processEvent(Event event, Session session) {
      System.out.println(String.format("Received an event:\n%s", event.toStringPretty(2, 2)));
      return true;
    }
  }
  public static void main(String[] args) {
    if (System.getenv("OKX_API_KEY") == null) {
      System.err.println("Please set environment variable OKX_API_KEY");
      return;
    }
    if (System.getenv("OKX_API_SECRET") == null) {
      System.err.println("Please set environment variable OKX_API_SECRET");
      return;
    }
    if (System.getenv("OKX_API_PASSPHRASE") == null) {
      System.err.println("Please set environment variable OKX_API_PASSPHRASE");
      return;
    }
    System.loadLibrary("ccapi_binding_java");
    var eventHandler = new MyEventHandler();
    var option = new SessionOptions();
    var config = new SessionConfigs();
    var session = new Session(option, config, eventHandler);
    var request = new Request(Request.Operation.CREATE_ORDER, "okx", "BTC-USDT");
    var param = new MapStringString();
    param.put("SIDE", "BUY");
    param.put("QUANTITY", "0.0005");
    param.put("LIMIT_PRICE", "20000");
    request.appendParam(param);
    session.sendRequest(request);
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
