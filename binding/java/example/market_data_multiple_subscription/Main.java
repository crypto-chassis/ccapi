import com.cryptochassis.ccapi.Event;
import com.cryptochassis.ccapi.EventHandler;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;
import com.cryptochassis.ccapi.Subscription;
import com.cryptochassis.ccapi.SubscriptionList;

public class Main {
  static class MyEventHandler extends EventHandler {
    @Override
    public boolean processEvent(Event event, Session session) {
      if (event.getType() == Event.Type.SUBSCRIPTION_DATA) {
        for (var message : event.getMessageList()) {
          var correlationId = message.getCorrelationIdList().get(0);
          System.out.println(String.format("%s: Best bid and ask at %s are:", correlationId, message.getTimeISO()));
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
    subscriptionList.add(new Subscription("okx", "BTC-USDT", "MARKET_DEPTH", "", "BTC"));
    subscriptionList.add(new Subscription("okx", "ETH-USDT", "MARKET_DEPTH", "", "ETH"));
    session.subscribe(subscriptionList);
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
