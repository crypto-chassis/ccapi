#!/usr/bin/env bash
find . -type f -path "*/binding/*" -not -path "*/build/*" -not -path "*/.venv/*" -name "*.py" -exec black --line-length 160 {} \+
