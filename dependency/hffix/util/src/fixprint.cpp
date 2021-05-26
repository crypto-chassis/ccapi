#include <hffix.hpp>

#include <iostream>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <map>

const char color_field[]   = "\x1b" "[33m"; // Yellow
const char color_value[]   = "\x1b" "[37m"; // White
const char color_msgtype[] = "\x1b" "[32m"; // Green
const char color_default[] = "\x1b" "[39m"; // Default Foreground

enum { chunksize = 4096 }; // Choose a preferred I/O chunk size.

char buffer[1 << 20]; // Must be larger than the largest FIX message size.

int main(int argc, char** argv)
{
    if (argc > 1 && ((0 == std::strcmp("-h", argv[1])) || (0 == std::strcmp("--help", argv[1])))) {
        std::cout << 
            "fixprint [Options]\n\n"
            "Reads raw FIX encoded data from stdin and writes annotated human-readable FIX to stdout.\n\n"
            "Options:\n"
            "  -c --color     Color output.\n";
        exit(0);
    }

    bool color = argc > 1 && (0 == std::strcmp("-c", argv[1]) || 0 == std::strcmp("--color", argv[1]));

    std::map<int, std::string> field_dictionary;
    hffix::dictionary_init_field(field_dictionary);
    std::map<std::string, std::string> message_dictionary;
    hffix::dictionary_init_message(message_dictionary);

    size_t buffer_length(0); // The number of bytes read in buffer[].

    size_t fred; // Number of bytes read from fread().

    // Read chunks from stdin until 0 is read or the buffer fills up without finding a complete message.
    while((fred = std::fread(buffer + buffer_length, 1, std::min(sizeof(buffer) - buffer_length, size_t(chunksize)), stdin))) {

        buffer_length += fred;
        hffix::message_reader reader(buffer, buffer + buffer_length);

        // Try to read as many complete messages as there are in the buffer.
        for (; reader.is_complete(); reader = reader.next_message_reader()) {
            if (reader.is_valid()) {

                // Here is a complete message. Read fields out of the reader.
                if (color) std::cout << color_value;
                std::cout.write(reader.prefix_begin(), reader.prefix_size());
                std::cout << ' ';

                try {
                    for(hffix::message_reader::const_iterator i = reader.begin(); i != reader.end(); ++i) {

                        if (color) std::cout << color_field;

                        std::map<int, std::string>::iterator fname = field_dictionary.find(i->tag());
                        if (fname != field_dictionary.end())
                            std::cout << fname->second << '_'; // Print the name of the field, if it's known.
                        std::cout << i->tag();

                        std::cout << '=';

                        if (color) std::cout << (i->tag() == hffix::tag::MsgType ? color_msgtype : color_value);

                        std::cout.write(i->value().begin(), i->value().size());

                        if (i->tag() == hffix::tag::MsgType) { // If this is a MsgType field.
                            std::map<std::string, std::string>::iterator mname = message_dictionary.find(std::string(i->value().begin(), i->value().end()));
                            if (mname != message_dictionary.end())
                                std::cout << '_' << mname->second; // Print the name of the message, if it's known.
                        }

                        std::cout << ' ';
                    }

                    std::cout << '\n';
                }
                catch(std::exception& ex) {
                    std::cerr << "Error reading the fields: " << ex.what() << '\n';
                }


            } else {
                // An invalid, corrupted FIX message. Do not try to read fields out of this reader.
                // The beginning of the invalid message is at location reader.message_begin() in the buffer,
                // but the end of the invalid message is unknown (because it's invalid).
                // Stay in this for loop, because the messager_reader::next_message_reader() function
                // will see that this message is invalid and it will search the remainder of the buffer
                // for the text "8=FIX", to see if there might be a complete or partial valid message
                // anywhere else in the remainder of the buffer.
                std::cerr << "Error Corrupt FIX message: ";
                std::cerr.write(reader.message_begin(), std::min(ssize_t(64), buffer + buffer_length - reader.message_begin()));
                std::cerr << "...\n";
            }
        }
        buffer_length = reader.buffer_end() - reader.buffer_begin();

        if (buffer_length > 0) // Then there is an incomplete message at the end of the buffer.
            std::memmove(buffer, reader.buffer_begin(), buffer_length); // Move the partial portion of the incomplete message to buffer[0].
    }

    if (color) std::cout << color_default;
    return 0;
}
