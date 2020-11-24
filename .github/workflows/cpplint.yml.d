name: Cpplint
on:
  pull_request:
    branches:
      - master
  push:
    branches:
      - master
jobs:
  cpplint:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v1
    - uses: actions/setup-python@v1
    - run: pip install cpplint
    - run: cpplint --recursive --exclude=include/ccapi_cpp/websocketpp_decompress_workaround.h --filter -legal/copyright,-whitespace/line_length,-build/c++11,-runtime/references,-build/include_what_you_use,-runtime/int include/**/* example/src/*
