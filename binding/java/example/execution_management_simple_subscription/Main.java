import com.cryptochassis.ccapi.Event;
import com.cryptochassis.ccapi.EventHandler;
import com.cryptochassis.ccapi.MapStringString;
import com.cryptochassis.ccapi.Message;
import com.cryptochassis.ccapi.Request;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;
import com.cryptochassis.ccapi.Subscription;
import com.cryptochassis.ccapi.SubscriptionList;

public class Main {
  static class MyEventHandler extends EventHandler {
    @Override
    public boolean processEvent(Event event, Session session) {
      if (event.getType() == Event.Type.SUBSCRIPTION_STATUS) {
        System.out.println(String.format("Received an event of type SUBSCRIPTION_STATUS:\n%s", event.toStringPretty(2, 2)));
        var message = event.getMessageList().get(0);
        if (message.getType() == Message.Type.SUBSCRIPTION_STARTED) {
          var request = new Request(Request.Operation.CREATE_ORDER, "okx", "BTC-USDT");
          var param = new MapStringString();
          param.put("SIDE", "BUY");
          param.put("QUANTITY", "0.0005");
          param.put("LIMIT_PRICE", "20000");
          request.appendParam(param);
          session.sendRequest(request);
        }
      } else if (event.getType() == Event.Type.SUBSCRIPTION_DATA) {
        System.out.println(String.format("Received an event of type SUBSCRIPTION_DATA:\n%s", event.toStringPretty(2, 2)));
      }
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
    var subscriptionList = new SubscriptionList();
    subscriptionList.add(new Subscription("okx", "BTC-USDT", "ORDER_UPDATE"));
    session.subscribe(subscriptionList);
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
