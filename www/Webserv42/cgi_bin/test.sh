#!/bin/bash

printf "Content-Type: text/plain\r\n"
printf "\r\n"

printf "CGI TYPE: BASH\n"
printf "STATUS: OK\n"
printf "\n"

printf "REQUEST_METHOD=%s\n" "${REQUEST_METHOD}"
printf "QUERY_STRING=%s\n" "${QUERY_STRING}"
printf "SCRIPT_FILENAME=%s\n" "${SCRIPT_FILENAME}"
printf "SCRIPT_NAME=%s\n" "${SCRIPT_NAME}"
printf "CONTENT_TYPE=%s\n" "${CONTENT_TYPE}"
printf "CONTENT_LENGTH=%s\n" "${CONTENT_LENGTH}"

printf "\nBODY:\n"

if [ "${REQUEST_METHOD}" = "POST" ]; then
    if [ -n "${CONTENT_LENGTH}" ]; then
        head -c "${CONTENT_LENGTH}"
    else
        cat
    fi
fi

printf "\n"