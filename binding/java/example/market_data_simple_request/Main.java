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
    System.loadLibrary("ccapi_binding_java");
    var eventHandler = new MyEventHandler();
    var option = new SessionOptions();
    var config = new SessionConfigs();
    var session = new Session(option, config, eventHandler);
    var request = new Request(Request.Operation.GET_RECENT_TRADES, "okx", "BTC-USDT");
    var param = new MapStringString();
    param.put("LIMIT", "1");
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
