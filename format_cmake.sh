#!/usr/bin/env bash
find . -type f -not -path "*/build/*" \( -name "CMakeLists.txt" -or -name "*.cmake" \) -exec cmake-format -i {} \+
