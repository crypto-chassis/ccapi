#!/usr/bin/env bash
find . -type f -path "*/binding/*" -not -path "*/build/*" -name "*.js" -exec clang-format -i -style=file {} \+
