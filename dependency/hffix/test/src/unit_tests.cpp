#define BOOST_TEST_MODULE hffix Unit Tests
#include <boost/test/included/unit_test.hpp>

#include <hffix.hpp>
#include <hffix_fields.hpp>

#if __cplusplus >= 201703L
using namespace std::literals::string_view_literals;
#endif
using namespace hffix;

#if __cplusplus >= 201103L
#include <chrono>
#include <iomanip>
#endif
#include <iterator>

BOOST_AUTO_TEST_CASE(basic)
{
    // Print the BOOST version, mostly so we can see it in Travis.
    std::cout << "BOOST version " << BOOST_LIB_VERSION << std::endl;

    char b[1024];
    message_writer w(b);
    w.push_back_header("FIX.4.2");
    w.push_back_string(tag::MsgType, "A");
    w.push_back_trailer();
    message_reader r(b);

    // A reader constructed from a writer should have the same size.
    BOOST_CHECK(w.message_size() == r.message_size());

    // Construct an invalid iterator.
    message_reader::const_iterator j;

    // Field value comparisons.
    message_reader::const_iterator i = r.begin();
    BOOST_CHECK(r.find_with_hint(tag::MsgType, i));
    BOOST_CHECK(i->value() == "A");
    BOOST_CHECK(i->value() == (const char*)"A");
    BOOST_CHECK(i->value() != "B");
    BOOST_CHECK(i->value() == std::string("A"));
    BOOST_CHECK(i->value() != std::string("B"));
#if __cplusplus >= 201703L
    BOOST_CHECK(i->value() == "A"sv); // string_view_literals
    BOOST_CHECK(i->value() != "B"sv); // string_view_literals
#endif

    // message_writer preconditions
    {
        message_writer w(b);
        BOOST_CHECK_THROW(w.push_back_trailer(false), std::logic_error);
    }
    {
        message_writer w(b);
        w.push_back_header("FIX.4.2");
        BOOST_CHECK_THROW(w.push_back_header("FIX.4.2"), std::logic_error);
    }
}

#if __cplusplus >= 201103L // message_writer_bounds test requires C++11

// find the minimum size buffer the message printed by f() will fit
// call the function with a buffer ranging from 100 bytes to 0 bytes
// record the smallest size where it does not throw an exception, make
// sure there's a single point where it starts throwing and that it keeps
// throwing for smaller buffer sizes
template <typename F>
int test_bound_checking(F f)
{
    char buffer[101];
    int minimum_size = 101;
    for (int i = 100; i >= 0; --i) {
        message_writer writer(buffer, buffer + i);
        try {
            buffer[i] = '\x55';
            f(writer);
            BOOST_CHECK(minimum_size == i + 1);
            minimum_size = i;
        }
        catch (std::out_of_range const& e) {}
        // make sure the next byte was not clobbered
        BOOST_CHECK(buffer[i] == '\x55');
    }
    return minimum_size;
}

BOOST_AUTO_TEST_CASE(message_writer_bounds)
{
    using W = message_writer;
    BOOST_CHECK(test_bound_checking([](W& w) { w.push_back_header("FIX.4.2"); } ) == 19);
    BOOST_CHECK(test_bound_checking([](W& w) {
                w.push_back_header("FIX.4.2");
                w.push_back_trailer(false);
            } ) == 26);
    BOOST_CHECK(test_bound_checking([](W& w) {
                w.push_back_header("FIX.4.2");
                w.push_back_trailer(true);
            } ) == 26);

    // 14 characters
    auto const test_string = "string literal";
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_string(58, test_string, test_string + 14); } ) == 4 + 14);
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_string(58, test_string); } ) == 4 + 14);
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_string(58, std::string(test_string)); } ) == 4 + 14);
#if HFFIX_HAS_STRING_VIEW
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_string(58, std::string_view(test_string)); } ) == 4 + 14);
#endif
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_char(58, 'a'); } ) == 5);
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_int(58, 55); } ) == 6);

    // 58=123.456|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_decimal(58, 123456, -3); } ) == 11);

    // 58=123456|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_decimal(58, 123456, 0); } ) == 10);

    // 58=19700101|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_date(58, 1970, 1, 1); } ) == 12);

    // 58=197001|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_monthyear(58, 1970, 1); } ) == 10);

    // 58=HH:MM:SS|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_timeonly(58, 23, 59, 59, 999); } ) == 16);

    // 58=YYYYMMDD-HH:MM:SS|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_timestamp(58, 1970, 1, 1, 23, 59, 59); } ) == 21);

    // 58=YYYYMMDD-HH:MM:SS.sss|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_timestamp(58, 1970, 1, 1, 23, 59, 59, 999); } ) == 25);

    // 58=14|59=..............|
    BOOST_CHECK(test_bound_checking([&](W& w) { w.push_back_data(58, 59, test_string, test_string + 14); } ) == 24);
}
#endif // message_writer_bounds test requires C++11

void test_checksum(message_writer& mw, char const (&expected)[4])
{
    mw.push_back_trailer();
    char const* end = mw.message_end();

    BOOST_CHECK(end[-4] == expected[0]);
    BOOST_CHECK(end[-3] == expected[1]);
    BOOST_CHECK(end[-2] == expected[2]);
}

BOOST_AUTO_TEST_CASE(checksum_empty)
{
    char buffer[50] = {};
    message_writer writer(buffer);

    // 8=FIX.4.2\x01 = (56+61+70+73+88+46+52+46+50+1) % 256 = 31
    // 9=000000\x01 = (57+61+48+48+48+48+48+48+1) % 256 = 151
    // (31 + 151) % 256 = 182
    writer.push_back_header("FIX.4.2");
    test_checksum(writer, "182");
}

BOOST_AUTO_TEST_CASE(checksum)
{
    char buffer[50] = {};
    message_writer writer(buffer);
    writer.push_back_header("FIX.4.2");
    writer.push_back_decimal(58, 123, 0);

    // 8=FIX.4.2\x01 = (56+61+70+73+88+46+52+46+50+1) % 256 = 31
    // 9=000007\x01 = (57+61+48+48+48+48+48+48+1) % 256 = 158
    // 58=123\x01 = (53+56+61+49+50+51+1) % 256 = 65
    // (31 + 158 + 65) % 256 = 254
    test_checksum(writer, "254");
}

BOOST_AUTO_TEST_CASE(checksum_negative)
{
    char buffer[50] = {};
    message_writer writer(buffer);
    writer.push_back_header("FIX.4.2");
    writer.push_back_char(58, '\x80');

    // 8=FIX.4.2\x01 = (56+61+70+73+88+46+52+46+50+1) % 256 = 31
    // 9=000005\x01 = (57+61+48+48+48+48+48+48+1) % 256 = 156
    // 58=\x80\x01 = (53+56+61+128+1) % 256 = 43
    // (31 + 156 + 43) % 256 = 230
    test_checksum(writer, "230");
}

BOOST_AUTO_TEST_CASE(checksum_calc)
{
    char buffer[50] = {};
    message_writer writer(buffer);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(tag::MsgType, "A");
    writer.push_back_trailer();

    message_reader mr(writer);
    BOOST_CHECK_EQUAL(mr.calculate_check_sum(), mr.check_sum()->value().as_int<unsigned char>());
}

// test that null fields can be iterated properly by message_reader
BOOST_AUTO_TEST_CASE(null_field_value)
{
    char buffer[50] = {};
    message_writer writer(buffer);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "A");
    writer.push_back_string(37, "");
    writer.push_back_string(38, "whatever");
    writer.push_back_trailer();

    message_reader reader(writer);
    message_reader::const_iterator i = reader.begin();
    BOOST_CHECK(reader.find_with_hint(38, i));
}

BOOST_AUTO_TEST_CASE(data_length)
{
    char buffer[100] = {};
    message_writer writer(buffer);
    std::string datum("datum");
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(tag::MsgType, "A");
    writer.push_back_data(
        tag::RawDataLength,
        tag::RawData,
        &*datum.begin(),
        &*datum.end()
        );
    writer.push_back_data(
        tag::EncodedUnderlyingProvisionTextLen,
        tag::EncodedUnderlyingProvisionText,
        &*datum.begin(),
        &*datum.end()
        );
    writer.push_back_trailer();

    message_reader reader(writer);
    message_reader::const_iterator i;

    bool found;

    i = reader.begin();
    found = reader.find_with_hint(tag::RawDataLength, i);
    BOOST_REQUIRE(!found);

    i = reader.begin();
    found = reader.find_with_hint(tag::EncodedUnderlyingProvisionTextLen, i);
    BOOST_REQUIRE(!found);

    i = reader.begin();
    found = reader.find_with_hint(tag::RawData, i);
    BOOST_REQUIRE(found);
    BOOST_REQUIRE(i != reader.end());
    BOOST_CHECK_EQUAL(i->value().as_string(), datum);

    found = reader.find_with_hint(tag::EncodedUnderlyingProvisionText, i);
    BOOST_REQUIRE(found);
    BOOST_REQUIRE(i != reader.end());
    BOOST_CHECK_EQUAL(i->value().as_string(), datum);
}

BOOST_AUTO_TEST_CASE(iterating)
{
    char b[1024];
    char* ptr = b;
    unsigned num = 0;
    for(size_t i = 0; i < 10; i++ ) {
      message_writer w(ptr, 1024 - (ptr - b));
      w.push_back_header("FIX.4.2");
      w.push_back_string(tag::MsgType, "A");
      w.push_back_trailer();
      ptr += w.message_size();
    }

    for (message_reader reader(b);
        reader.is_complete();
        reader = reader.next_message_reader()) {
        if(reader.is_complete() && reader.is_valid()) {
          BOOST_CHECK_EQUAL(std::distance(reader.begin(), reader.end()), 1);
          num++;
        }
    }
    BOOST_CHECK_EQUAL(num, 10u);
}

#if __cplusplus >= 201103L
// test that std::chrono values can be written and read correctly
BOOST_AUTO_TEST_CASE(chrono)
{
    using namespace std::chrono;
    using TimePoint = time_point<system_clock, milliseconds>;

    TimePoint tsend(milliseconds(1502282096123ULL)); // 2017-08-09 12:34:56.123

    milliseconds timeofday =
            hours(12) + minutes(34) + seconds(0) + milliseconds(789);

    char buffer[100] = {};
    message_writer writer(buffer);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "A");
    writer.push_back_timestamp(hffix::tag::SendingTime, tsend);
    writer.push_back_timeonly(hffix::tag::MDEntryTime, timeofday);
    writer.push_back_trailer();

    message_reader reader(writer);
    message_reader::const_iterator i = reader.begin();
    reader.find_with_hint(hffix::tag::SendingTime, i);
    BOOST_CHECK_EQUAL(i->value().as_string(), std::string("20170809-12:34:56.123"));

    TimePoint trecv;
    bool tparsed = i->value().as_timestamp(trecv);

    BOOST_CHECK(tparsed);
    BOOST_CHECK(tsend == trecv);

    message_reader::const_iterator j = reader.begin();
    reader.find_with_hint(hffix::tag::MDEntryTime, j);
    BOOST_CHECK_EQUAL(j->value().as_string(), std::string("12:34:00.789"));

    milliseconds todrecv;
    bool dparsed = j->value().as_timeonly(todrecv);
    BOOST_CHECK(dparsed);
    BOOST_CHECK(timeofday == todrecv);

    // TODO check this when we have a way to parse milliseconds with put_time.
    // std::time_t trecv_t = std::chrono::system_clock::to_time_t(trecv);
    // std::ostringstream oss;
    // oss << std::put_time(std::localtime(&trecv_t), "%Y%m%d-%T");
    // std::string tstr = oss.str();
    // BOOST_CHECK_EQUAL(tstr, i->value().as_string());

}
#endif

