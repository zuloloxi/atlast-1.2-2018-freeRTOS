#!/bin/bash

# set -x

printf "uint8_t nvramrc[] = \""

while read -r LINE; do 
    if [ ${#LINE} -ge 0 ]; then
#        printf "$LINE \\n"| tr '\n' ' ' | sed 's/  */ /g'
        printf "$LINE \\" | sed '/^\s*$/d'
        printf "n"
    fi
done 
printf "\";\n"

