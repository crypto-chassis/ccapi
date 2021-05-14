%module(directors="1") ccapi
%{
#ifndef SWIG
#define SWIG
#endif
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util.h"
#include "ccapi_cpp/ccapi_element.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_event_dispatcher.h"
#include "ccapi_cpp/ccapi_subscription.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_session.h"
#include "ccapi_cpp/ccapi_logger.h"
%}
%feature("director") ccapi::EventHandler;
%feature("director") ccapi::Logger;
%include "exception.i"
%include "std_pair.i"
%include "std_string.i"
%include "std_map.i"
%include "std_vector.i"
%include "std_pair.i"
%exception {
    try {
        $action
    } catch (std::exception &e) {
        std::string s("error: "), s2(e.what());
        s = s + s2;
        SWIG_exception(SWIG_RuntimeError, s.c_str());
    }
}
%template(map_string_string) std::map<std::string, std::string>;
%template(vector_Element) std::vector<ccapi::Element>;
%template(vector_string) std::vector<std::string>;
%template(vector_Message) std::vector<ccapi::Message>;
%template(map_string_map_string_string) std::map<std::string, std::map<std::string, std::string> >;
%template(vector_Event) std::vector<ccapi::Event>;
%template(vector_Subscription) std::vector<ccapi::Subscription>;
%template(vector_Request) std::vector<ccapi::Request>;
%include "ccapi_cpp/ccapi_macro.h"
%include "ccapi_cpp/ccapi_util.h"
%include "ccapi_cpp/ccapi_element.h"
%include "ccapi_cpp/ccapi_message.h"
%include "ccapi_cpp/ccapi_event.h"
%include "ccapi_cpp/ccapi_event_handler.h"
%include "ccapi_cpp/ccapi_event_dispatcher.h"
%include "ccapi_cpp/ccapi_subscription.h"
%include "ccapi_cpp/ccapi_request.h"
%include "ccapi_cpp/ccapi_session_options.h"
%include "ccapi_cpp/ccapi_session_configs.h"
%include "ccapi_cpp/ccapi_queue.h"
%include "ccapi_cpp/ccapi_session.h"
%include "ccapi_cpp/ccapi_logger.h"
%template(Queue_Event) ccapi::Queue<ccapi::Event>;
