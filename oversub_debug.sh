#!/bin/bash

echo -ne "\\r\\e[0;0H\\e[33m*** Build pending... ***\\e[0m"
if ! make -k -j2 obj/ROM.rom bin/ars-emu-debug 2>/tmp/oversub_result.txt >/dev/null; then
    SIZE=$(stty size)
    ROWS=$(echo $SIZE | cut -f 1 -d " ")
    COLS=$(echo $SIZE | cut -f 2 -d " ")
    clear
    echo -e "\\e[31m*** Build failed ***\\e[0m"
    head -n $(( $ROWS - 2 )) /tmp/oversub_result.txt | \
        awk "{print substr(\$0,0,$(($COLS)))}"
    echo -ne \\e[31m
    play -q $HOME/System7Sounds/7.Indigo.flac vol 0.5 speed 0.28 &
else
    clear
    echo -e "\\e[32m*** Build succeeded ***"
    head -n $(( $ROWS - 2 )) /tmp/oversub_result.txt | \
        awk "{print substr(\$0,0,$(($COLS)))}"
    play -q $HOME/System7Sounds/6.Droplet.flac vol 0.5 speed 0.5 &
fi
echo -n "---" `date` "---"
echo -ne \\e[0m
