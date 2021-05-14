#include <iostream>
#include <cstdio>
#include <map>

// We want Boost Date_Time support, so include these before hffix.hpp.
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <hffix.hpp>

const size_t chunksize = 4096; // Choose a preferred I/O chunk size.

char buffer[1 << 20]; // Must be larger than the largest FIX message size.

int main(int argc, char** argv)
{
    int return_code = 0;

    std::map<int, std::string> field_dictionary;
    hffix::dictionary_init_field(field_dictionary);

    size_t buffer_length(0); // The number of bytes read in buffer[].

    size_t fred; // Number of bytes read from fread().

    // Read chunks from stdin until 0 is read or the buffer fills up without
    // finding a complete message.
    while ((fred = std::fread(
                    buffer + buffer_length,
                    1,
                    std::min(sizeof(buffer) - buffer_length, chunksize),
                    stdin
                    )
          )) {

        buffer_length += fred;
        hffix::message_reader reader(buffer, buffer + buffer_length);

        // Try to read as many complete messages as there are in the buffer.
        for (; reader.is_complete(); reader = reader.next_message_reader()) {
            if (reader.is_valid()) {

                // Here is a complete message. Read fields out of the reader.
                try {
                    if (reader.message_type()->value() == "A") {
                        std::cout << "Logon message\n";

                        hffix::message_reader::const_iterator i = reader.begin();

                        if (reader.find_with_hint(hffix::tag::SenderCompID, i))
                            std::cout
                                << "SenderCompID = "
                                << i++->value() << '\n';

                        if (reader.find_with_hint(hffix::tag::MsgSeqNum, i))
                            std::cout
                                << "MsgSeqNum    = "
                                << i++->value().as_int<int>() << '\n';

                        if (reader.find_with_hint(hffix::tag::SendingTime, i))
                            std::cout
                                << "SendingTime  = "
                                << i++->value().as_timestamp() << '\n';

                        std::cout
                            << "The next field is "
                            << hffix::field_name(i->tag(), field_dictionary)
                            << " = " << i->value() << '\n';

                        std::cout << '\n';
                    }
                    else if (reader.message_type()->value() == "D") {
                        std::cout << "New Order Single message\n";

                        hffix::message_reader::const_iterator i = reader.begin();

                        if (reader.find_with_hint(hffix::tag::Side, i))
                            std::cout <<
                                (i++->value().as_char() == '1' ?"Buy ":"Sell ");

                        if (reader.find_with_hint(hffix::tag::Symbol, i))
                            std::cout << i++->value() << " ";

                        if (reader.find_with_hint(hffix::tag::OrderQty, i))
                            std::cout << i++->value().as_int<int>();

                        if (reader.find_with_hint(hffix::tag::Price, i)) {
                            int mantissa, exponent;
                            i->value().as_decimal(mantissa, exponent);
                            std::cout << " @ $" << mantissa << "E" << exponent;
                            ++i;
                        }

                        std::cout << "\n\n";
                    }

                } catch(std::exception& ex) {
                    std::cerr << "Error reading fields: " << ex.what() << '\n';
                }

            } else {
                // An invalid, corrupted FIX message. Do not try to read fields
                // out of this reader. The beginning of the invalid message is
                // at location reader.message_begin() in the buffer, but the
                // end of the invalid message is unknown (because it's invalid).
                //
                // Stay in this for loop, because the
                // messager_reader::next_message_reader() function will see
                // that this message is invalid and it will search the
                // remainder of the buffer for the text "8=FIX", to see if
                // there might be a complete or partial valid message anywhere
                // else in the remainder of the buffer.
                //
                // Set the return code non-zero to indicate that there was
                // an invalid message, and print the first 64 chars of the
                // invalid message.
                return_code = 1;
                std::cerr << "Error Invalid FIX message: ";
                std::cerr.write(
                    reader.message_begin(),
                    std::min(
                        ssize_t(64),
                        buffer + buffer_length - reader.message_begin()
                        )
                    );
                std::cerr << "...\n";
            }
        }
        buffer_length = reader.buffer_end() - reader.buffer_begin();

        if (buffer_length > 0)
            // Then there is an incomplete message at the end of the buffer.
            // Move the partial portion of the incomplete message to buffer[0].
            std::memmove(buffer, reader.buffer_begin(), buffer_length);
    }

    return return_code;
}
