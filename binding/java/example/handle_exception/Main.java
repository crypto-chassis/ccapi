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
      try {
        throw new Exception("oops");
      } catch (Exception e) {
        e.printStackTrace();
        System.exit(1);
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
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
