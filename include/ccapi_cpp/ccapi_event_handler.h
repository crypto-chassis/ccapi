#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_HANDLER_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_HANDLER_H_
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class Session;
/**
 * Defines the callback supplied by the application to process Events. Applications which use event driven programming supply instances of their own subclass of
 * this class to the Session to receive callbacks when an Event is available.
 */
class EventHandler {
 public:
  virtual ~EventHandler() {}
  virtual bool processEvent(const Event& event, Session* session) { return true; }
  // An implementation of processEvent should process the specified
  // 'event' which originates from the specified 'session' and
  // return true to indicate events should continue to be delivered
  // or false to terminate dispatching of events.
  //
  // If the application wishes to process the event further
  // after returning from the processEvents() call it must make
  // a copy of the Event to ensure the underlying data is not
  // freed.
  // Note: no exceptions are thrown
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_HANDLER_H_
