#include "ccapi_cpp/ccapi_session.h"

#include "cppflow/ops.h"
#include "cppflow/model.h"
#include "cppflow/cppflow.h"

namespace ccapi
{
    Logger *Logger::logger = nullptr;
    class MyEventHandler : public EventHandler
    {
    public:
        bool processEvent(const Event &event, Session *session) override
        {
            if (event.getType() == Event::Type::SUBSCRIPTION_DATA)
            {
                for (const auto &message : event.getMessageList())
                {
                    if (message.getRecapType() == Message::RecapType::NONE)
                    {
                        for (const auto &element : message.getElementList())
                        {
                            std::lock_guard<std::mutex> lock(m);
                            if (element.has("BID_PRICE"))
                            {
                                bestBidPrice = element.getValue("BID_PRICE");
                            }
                            if (element.has("ASK_PRICE"))
                            {
                                bestAskPrice = element.getValue("ASK_PRICE");
                            }
                            if (element.has("BID_SIZE"))
                            {
                                bestBidSize = element.getValue("BID_SIZE");
                            }
                            if (element.has("ASK_SIZE"))
                            {
                                bestAskSize = element.getValue("ASK_SIZE");
                            }
                        }
                    }
                }
            }
            return true;
        }

        float getOrderPressure()
        {
            std::lock_guard<std::mutex> lock(m);
            float orderPressure =(std::stof(bestBidSize) - std::stof(bestAskSize)) / (std::stof(bestBidSize) + std::stof(bestAskSize));;
            return orderPressure;
        }

        float getSpread() // normalized by mid-price
        {
            std::lock_guard<std::mutex> lock(m);
            float spread = (std::stof(bestAskPrice) - std::stof(bestBidPrice)) / ((std::stof(bestAskPrice) + std::stof(bestBidPrice))/2);
            return spread;
        }

        bool marketReady()
        {
            return (!bestBidPrice.empty() && !bestAskPrice.empty() && !bestBidSize.empty() && !bestAskSize.empty());
        }

    private:
        mutable std::mutex m;
        std::string bestBidPrice;
        std::string bestAskPrice;
        std::string bestBidSize;
        std::string bestAskSize;
    };
} /* namespace ccapi */

int main(int argc, char **argv)
{
    using namespace ccapi; // NOLINT(build/namespaces)
    std::cout << cppflow::version() << std::endl;
    auto loaded_model = new cppflow::model("../../../example/src/tf_model_simple/model");

    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
    session.subscribe(subscription);

    for (auto start = std::chrono::steady_clock::now(), now = start; now < start + std::chrono::seconds{10}; now = std::chrono::steady_clock::now()) 
    {
        if (eventHandler.marketReady())
        {
            auto input_data = std::vector<float>(
                {
                    eventHandler.getOrderPressure(),
                    eventHandler.getSpread()
                });
            auto input_shape = std::vector<int64_t>({1, 2});
            auto input = cppflow::tensor(input_data, input_shape);
            auto output = loaded_model->operator()(input);
            auto values = output.get_data<float>();
            for (auto &v : values) 
            {
                std::cout << "Model output: " + std::to_string(v) << std::endl;
            }
        }
    }

    session.stop();
    std::cout << "Bye" << std::endl;
    return EXIT_SUCCESS;
}
