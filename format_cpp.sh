#!/usr/bin/env bash
find . -type f -not -path "*/build/*" \( -name "*.h" -or -name "*.cpp" -or -name "*.hpp" \) -exec clang-format -i -style=file {} \+
