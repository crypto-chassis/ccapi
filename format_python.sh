#!/usr/bin/env bash
find . -type f -path "*/binding/*" -not -path "*/build/*" -not -path "*/.venv/*" -name "*.py" -exec autopep8 --in-place --max-line-length 160 {} \+
