import com.cryptochassis.ccapi.Event;
import com.cryptochassis.ccapi.EventHandler;
import com.cryptochassis.ccapi.Message;
import com.cryptochassis.ccapi.PairIntString;
import com.cryptochassis.ccapi.Request;
import com.cryptochassis.ccapi.Session;
import com.cryptochassis.ccapi.SessionConfigs;
import com.cryptochassis.ccapi.SessionOptions;
import com.cryptochassis.ccapi.Subscription;
import com.cryptochassis.ccapi.SubscriptionList;
import com.cryptochassis.ccapi.VectorPairIntString;

public class Main {
  static class MyEventHandler extends EventHandler {
    @Override
    public boolean processEvent(Event event, Session session) {
      if (event.getType() == Event.Type.AUTHORIZATION_STATUS) {
        System.out.println(String.format("Received an event of type AUTHORIZATION_STATUS:\n%s", event.toStringPretty(2, 2)));
        var message = event.getMessageList().get(0);
        if (message.getType() == Message.Type.AUTHORIZATION_SUCCESS) {
          var request = new Request(Request.Operation.FIX, "coinbase", "", "same correlation id for subscription and request");
          var param = new VectorPairIntString();
          param.add(new PairIntString(35, "D"));
          param.add(new PairIntString(11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"));
          param.add(new PairIntString(55, "BTC-USD"));
          param.add(new PairIntString(54, "1"));
          param.add(new PairIntString(44, "20000"));
          param.add(new PairIntString(38, "0.001"));
          param.add(new PairIntString(40, "2"));
          param.add(new PairIntString(59, "1"));
          request.appendParamFix(param);
          session.sendRequestByFix(request);
        }
      } else if (event.getType() == Event.Type.FIX) {
        System.out.println(String.format("Received an event of type FIX:\n%s", event.toStringPretty(2, 2)));
      }
      return true;
    }
  }
  public static void main(String[] args) {
    if (System.getenv("COINBASE_API_KEY") == null) {
      System.err.println("Please set environment variable COINBASE_API_KEY");
      return;
    }
    if (System.getenv("COINBASE_API_SECRET") == null) {
      System.err.println("Please set environment variable COINBASE_API_SECRET");
      return;
    }
    if (System.getenv("COINBASE_API_PASSPHRASE") == null) {
      System.err.println("Please set environment variable COINBASE_API_PASSPHRASE");
      return;
    }
    System.loadLibrary("ccapi_binding_java");
    var eventHandler = new MyEventHandler();
    var option = new SessionOptions();
    var config = new SessionConfigs();
    var session = new Session(option, config, eventHandler);
    var subscription = new Subscription("coinbase", "", "FIX", "", "same correlation id for subscription and request");
    session.subscribeByFix(subscription);
    try {
      Thread.sleep(10000);
    } catch (InterruptedException e) {
    }
    session.stop();
    System.out.println("Bye");
  }
}
