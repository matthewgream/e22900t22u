#!/bin/bash

# Hex JSON decoder for mosquitto messages using packed json
# Usage: ./decode.sh '["6e6f745f76616c69645f6a736f6e"]'
# Or pipe: echo '["6e6f745f76616c69645f6a736f6e"]' | ./decode.sh

decode_hex() {
    local input="$1"
    local hex_string=$(echo "$input" | sed 's/^\["//' | sed 's/"\]$//')
    echo "$hex_string" | xxd -r -p
}

if [ $# -gt 0 ]; then
    decode_hex "$1"
else
    while IFS= read -r line; do
        echo "$line"
        json_part=$(echo "$line" | cut -d' ' -f2-)
        decode_hex "$json_part"
        echo
    done
fi
