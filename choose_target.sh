#!/bin/sh

if [ ! -t 0 ]; then
    echo "Please symlink make/cur_target.mk to an appropriate target definition. Target definitions can be found in the make/target directory."
    exit 1
fi

while true; do
    for f in make/target/*.mk; do
        base=`basename $f .mk`
        printf "%s: " "$base"
        head -n 1 "$f" | cut -b 3-
    done
    echo "Please type one of the above target names (e.g. Linux) and press enter."
    read choice
    if [ -f "make/target/$choice.mk" ]; then
        rm -f make/cur_target.mk
        ln -s "target/$choice.mk" make/cur_target.mk
        break
    else
        echo "Not a valid target. Try again."
    fi
done
