#include "ccapi_cpp/ccapi_event.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::ElementsAre;
namespace ccapi {
TEST(EventTest, addMessagesZeroAddZero) {
  Event e;
  std::vector<Message> ml;
  e.addMessages(ml);
  EXPECT_TRUE(e.getMessageList().empty());
}
TEST(EventTest, addMessagesZeroAddSome) {
  Event e;
  std::vector<Message> ml;
  ml.push_back(Message());
  e.addMessages(ml);
  EXPECT_EQ(e.getMessageList().size(), 1);
}
TEST(EventTest, addMessagesSomeAddZero) {
  Event e;
  e.setMessageList({Message()});
  std::vector<Message> ml;
  e.addMessages(ml);
  EXPECT_EQ(e.getMessageList().size(), 1);
}
TEST(EventTest, addMessagesSomeAddSome) {
  Message m1;
  m1.setType(Message::Type::SESSION_CONNECTION_UP);
  Message m2;
  m2.setType(Message::Type::MARKET_DATA_EVENTS_TRADE);
  Event e;
  e.setMessageList({m1});
  std::vector<Message> ml{m2};
  e.addMessages(ml);
  EXPECT_EQ(e.getMessageList().size(), 2);
  EXPECT_EQ(e.getMessageList().at(0).getType(), Message::Type::SESSION_CONNECTION_UP);
  EXPECT_EQ(e.getMessageList().at(1).getType(), Message::Type::MARKET_DATA_EVENTS_TRADE);
}

} /* namespace ccapi */
