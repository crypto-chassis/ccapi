// We want Boost Date_Time support, so include these before hffix.hpp.
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>

#include <hffix.hpp>
#include <iostream>

using namespace boost::posix_time;
using namespace boost::gregorian;

int main(int argc, char** argv)
{
    int seq_send(1); // Sending sequence number.

    char buffer[1 << 13];

    ptime tsend(date(2017,8,9), time_duration(12,34,56));

    // We'll put a FIX Logon message in the buffer.
    hffix::message_writer logon(buffer, buffer + sizeof(buffer));

    logon.push_back_header("FIX.4.2"); // Write BeginString and BodyLength.

    // Logon MsgType.
    logon.push_back_string    (hffix::tag::MsgType, "A");
    logon.push_back_string    (hffix::tag::SenderCompID, "AAAA");
    logon.push_back_string    (hffix::tag::TargetCompID, "BBBB");
    logon.push_back_int       (hffix::tag::MsgSeqNum, seq_send++);
    logon.push_back_timestamp (hffix::tag::SendingTime, tsend);
    // No encryption.
    logon.push_back_int       (hffix::tag::EncryptMethod, 0);
    // 10 second heartbeat interval.
    logon.push_back_int       (hffix::tag::HeartBtInt, 10);

    logon.push_back_trailer(); // write CheckSum.

    // Now the Logon message is written to the buffer.

    // Add a FIX New Order - Single message to the buffer, after the Logon
    // message.
    hffix::message_writer new_order(logon.message_end(), buffer + sizeof(buffer));

    new_order.push_back_header("FIX.4.2");

    // New Order - Single
    new_order.push_back_string    (hffix::tag::MsgType, "D");
    // Required Standard Header field.
    new_order.push_back_string    (hffix::tag::SenderCompID, "AAAA");
    new_order.push_back_string    (hffix::tag::TargetCompID, "BBBB");
    new_order.push_back_int       (hffix::tag::MsgSeqNum, seq_send++);
    new_order.push_back_timestamp (hffix::tag::SendingTime, tsend);
    new_order.push_back_string    (hffix::tag::ClOrdID, "A1");
    // Automated execution.
    new_order.push_back_char      (hffix::tag::HandlInst, '1');
    // Ticker symbol OIH.
    new_order.push_back_string    (hffix::tag::Symbol, "OIH");
    // Buy side.
    new_order.push_back_char      (hffix::tag::Side, '1');
    new_order.push_back_timestamp (hffix::tag::TransactTime, tsend);
    // 100 shares.
    new_order.push_back_int       (hffix::tag::OrderQty, 100);
    // Limit order.
    new_order.push_back_char      (hffix::tag::OrdType, '2');
    // Limit price $500.01 = 50001*(10^-2). The push_back_decimal() method
    // takes a decimal floating point number of the form mantissa*(10^exponent).
    new_order.push_back_decimal   (hffix::tag::Price, 50001, -2);
    // Good Till Cancel.
    new_order.push_back_char      (hffix::tag::TimeInForce, '1');

    new_order.push_back_trailer(); // write CheckSum.

    //Now the New Order message is in the buffer after the Logon message.

    // Write both messages to stdout.
    std::cout.write(buffer, new_order.message_end() - buffer);

    return 0;
}
