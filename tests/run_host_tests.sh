#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT
mkdir -p "$test_dir/freertos"
for header in esp_err.h esp_twai.h esp_twai_onchip.h esp_log.h esp_timer.h \
              freertos/FreeRTOS.h freertos/task.h freertos/queue.h freertos/semphr.h; do
    printf '#include "mock.h"\n' > "$test_dir/$header"
done
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Wno-unused-but-set-variable \
    -I"$test_dir" -I"$repo_dir/tests/host" -I"$repo_dir/main" \
    "$repo_dir/tests/host/test_can.c" -o "$test_dir/test_can"
"$test_dir/test_can"
