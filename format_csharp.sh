#!/usr/bin/env bash
find . -type f -path "*/binding/*" -not -path "*/build/*" -name "*.cs" -exec clang-format -i -style=file {} \+
