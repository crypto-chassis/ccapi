#ifndef APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#define APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
#include "ccapi_cpp/ccapi_event.h"
namespace ccapi {
class HistoricalMarketDataEventProcessor {
 public:
   HistoricalMarketDataEventProcessor(std::function<bool(const Event& event)> eventHandler):eventHandler(eventHandler){

   }
   TimePoint startDateTp,endDateTp;
   std::string exchange,baseAsset,quoteAsset,historicalMarketDataDirectory;

   std::function<bool(const Event& event)> eventHandler;
   void processEvent(){
     int clockSeconds = 0;
     TimePoint currentDateTp{std::chrono::seconds{0}};
     std::ifstream fMarketDepth;
     std::ifstream fTrade;
     std::string lineMarketDepth;
     std::string lineTrade;
     Event previousEventMarketDepth;
     Event previousEventTrade;
     bool shouldContinueTrade{true};
     while(currentDateTp<this->endDateTp){
       std::string fileNameWithDirBase=this->historicalMarketDataDirectory+"/"+this->exchange+"__"+this->baseAsset+"-"+this->quoteAsset+"__"+UtilTime::getISOTimestamp(currentDateTp)+"__";

       fMarketDepth.open(fileNameWithDirBase+"market-depth.csv");
       if (fMarketDepth){
         fMarketDepth.ignore(INT_MAX, '\n');

         while (std::getline(fMarketDepth, lineMarketDepth)){
           shouldContinueTrade = true;
           auto splittedMarketDepth = UtilString::split(lineMarketDepth, ",");
           int currentSecondsMarketDepth = std::stoi(splittedMarketDepth.at(0));
           if (clockSeconds == 0) {
             clockSeconds = currentSecondsMarketDepth;
             fTrade.open(fileNameWithDirBase+"trade.csv");
             if (fTrade){
               fTrade.ignore(INT_MAX, '\n');
               this->advanceTradeIterator(shouldContinueTrade, fTrade,  lineTrade,  clockSeconds);
             }
             this->processMarketDataEventMarketDepth( splittedMarketDepth);
           } else {
             while (clockSeconds < currentSecondsMarketDepth){
               this->advanceTradeIterator( shouldContinueTrade, fTrade,  lineTrade,  clockSeconds);
               if (shouldContinueTrade){
                 fTrade.open(fileNameWithDirBase+"trade.csv");
                 if (fTrade){
                   fTrade.ignore(INT_MAX, '\n');
                   this->advanceTradeIterator( shouldContinueTrade, fTrade,  lineTrade,  clockSeconds);
                 }
               }
               this->processMarketDataEventMarketDepth( splittedMarketDepth);
               clockSeconds++;
             }
           }
         }
       }
       fMarketDepth.clear();
       fTrade.clear();
       currentDateTp+=std::chrono::hours(24);
     }
   }

 private:
   void advanceTradeIterator(bool& shouldContinueTrade,  std::ifstream& fTrade, std::string& lineTrade, int clockSeconds){
     while (shouldContinueTrade && std::getline(fTrade, lineTrade)){
       auto splittedTrade = UtilString::split(lineTrade, ",");
       int currentSecondsTrade = std::stoi(splittedTrade.at(0));
       if (currentSecondsTrade<clockSeconds){
         this->processMarketDataEventTrade(splittedTrade);
       } else {
         shouldContinueTrade = false;
       }
     }
   }
   void processMarketDataEventTrade(const std::vector<std::string>& splittedLine){
     Event event;
     event.setType(Event::Type::SUBSCRIPTION_DATA);
     Message message;
     message.setType(exchange.rfind("binance", 0) == 0? Message::Type::MARKET_DATA_EVENTS_AGG_TRADE:Message::Type::MARKET_DATA_EVENTS_TRADE);
     if (splittedLine.at(0).find('.') != std::string::npos){
       auto splittedSeconds = UtilString::split(splittedLine.at(0), ".");
       message.setTime(UtilTime::makeTimePoint(std::make_pair(std::stol(splittedSeconds.at(0)),std::stol(splittedSeconds.at(1)))));
     } else {
       message.setTime(UtilTime::makeTimePoint(std::make_pair(std::stol(splittedLine.at(0)),0)));
     }
     message.setCorrelationIdList({"TRADE"});
     std::vector<Element> elementList;
     Element element;

     if (!splittedLine.at(1).empty()){
       auto priceSize = UtilString::split(splittedLine.at(1), "_");
       element.insert("BID_PRICE",priceSize.at(0));
       element.insert("BID_SIZE",priceSize.at(1));
     }
     if (!splittedLine.at(2).empty()){
       auto priceSize = UtilString::split(splittedLine.at(2), "_");
       element.insert("ASK_PRICE",priceSize.at(0));
       element.insert("ASK_SIZE",priceSize.at(1));
     }
     elementList.push_back(std::move(element));
     message.setElementList(elementList);
     event.addMessage(message);
     this->eventHandler(event);
   }
   void processMarketDataEventMarketDepth(const std::vector<std::string>& splittedLine){
     Event event;
     event.setType(Event::Type::SUBSCRIPTION_DATA);
     Message message;
     message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
     message.setTime(UtilTime::makeTimePoint(std::make_pair(std::stol(splittedLine.at(0)),0)));
     message.setCorrelationIdList({"MARKET_DEPTH"});
     std::vector<Element> elementList;
     Element element;
     element.insert("LAST_PRICE",splittedLine.at(1));
     element.insert("LAST_SIZE",splittedLine.at(2));
     element.insert("IS_BUYER_MAKER",splittedLine.at(3));
     elementList.push_back(std::move(element));
     message.setElementList(elementList);
     event.addMessage(message);
     this->eventHandler(event);
   }
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_H_
