#!/bin/sh

set -e

make

echo "Testing valid configs..."
for file in tests/configs/valid/*.conf; do
    echo "  $file"
    ./webserv --test-config "$file" >/dev/null
done

echo "Testing invalid configs..."
for file in tests/configs/invalid/*.conf; do
    echo "  $file"
    if ./webserv --test-config "$file" >/dev/null 2>&1; then
        echo "FAILED: invalid config passed: $file"
        exit 1
    fi
done

echo "All config tests passed"