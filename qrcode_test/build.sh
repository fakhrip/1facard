if [ $# -eq 0 ] ; then
    echo "Usage: ./build.sh \$PORT"
    exit 1
fi

arduino-cli -b esp32:esp32:esp32cam compile -v -e && arduino-cli upload -p $1 --fqbn esp32:esp32:esp32cam .
