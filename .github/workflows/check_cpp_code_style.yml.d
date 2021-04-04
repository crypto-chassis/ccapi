name: Cpplint
on:
  pull_request:
    branches:
      - master
      - development
  push:
    branches:
      - master
      - development
jobs:
  cpplint:
    runs-on: ubuntu-latest
    steps:
      env:
          CPP_CODE_STYLE_BASE: Google
          LINE_LENGTH: 125
    - uses: actions/checkout@v1
    - uses: actions/setup-python@v1
    - run: sudo apt install clang-format
    - run: find . -type f -not -path "*/dependency/*" -not -path "*/build/*" \( -name "*.h" -or -name "*.cpp" \)  | xargs clang-format -i -style="{BasedOnStyle: ${{ env.CPP_CODE_STYLE_BASE }}, ColumnLimit: ${{ env.LINE_LENGTH }}}"
    - run: pip install cpplint
    - run: cpplint --recursive --exclude=include/ccapi_cpp/ccapi_hmac.h --exclude=include/ccapi_cpp/ccapi_date.h --exclude=include/ccapi_cpp/websocketpp_decompress_workaround.h --exclude=*/build --filter -legal/copyright,-whitespace/line_length,-build/c++11,-runtime/references,-build/include_what_you_use,-runtime/int include/**/* example/**/* test/**/*
