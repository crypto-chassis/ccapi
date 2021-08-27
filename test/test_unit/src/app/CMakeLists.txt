set(NAME app)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} common_test.cpp historical_market_data_event_processor_test.cpp)
gtest_discover_tests(${NAME} PROPERTIES ENVIRONMENT "HISTORICAL_MARKET_DATA_EVENT_PROCESSOR_TEST=gemini,btc,usd,2021-07-01,2021-07-03,${CCAPI_PROJECT_DIR}/test/test_unit/data/historical_market_data,false")
