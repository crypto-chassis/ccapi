import com.cryptochassis.ccapi.Event;
import com.cryptochassis.ccapi.EventHandler;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;
import com.cryptochassis.ccapi.Subscription;
import com.cryptochassis.ccapi.SubscriptionList;
import com.cryptochassis.ccapi.Request;
import com.cryptochassis.ccapi.MapStringString;

public class Main {
  static class MyEventHandler extends EventHandler {
    @Override
    public boolean processEvent(Event event, Session session) {
      if (event.getType() == Event.Type.SUBSCRIPTION_DATA) {
        for (var message : event.getMessageList()) {
          System.out.println(String.format("Best bid and ask at %s are:", message.getTimeISO()));
          for (var element : message.getElementList()) {
            var elementNameValueMap = element.getNameValueMap();
            for (var entry : elementNameValueMap.entrySet()) {
              var name = entry.getKey();
              var value = entry.getValue();
              System.out.println(String.format("  %s = %s", name, value));
            }
          }
        }
      }
      return true;
    }
  }
  public static void main(String[] args) {
    System.loadLibrary("ccapi_binding_java");
    var eventHandler = new MyEventHandler();
    var option = new SessionOptions();
    var config = new SessionConfigs();
    var session = new Session(option, config, eventHandler);
    var subscriptionList = new SubscriptionList();
    subscriptionList.add(new Subscription("okx", "BTC-USDT", "MARKET_DEPTH"));
    session.subscribe(subscriptionList);
    var request = new Request(Request.Operation.GET_RECENT_TRADES, "okx", "BTC-USDT");
    var param = new MapStringString();
    param.put("LIMIT", "1");
    request.appendParam(param);
    session.sendRequest(request);
    session.stop();
    System.out.println("Bye");
  }
}
