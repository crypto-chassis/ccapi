#!/usr/bin/env bash
find . -type f -path "*/binding/*" -not -path "*/build/*" -name "*.go" -exec gofmt -w {} \+
