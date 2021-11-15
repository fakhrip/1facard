if [ $# -eq 0 ] ; then
    echo "Usage: ./build.sh \$PORT \$SSID \$PASS"
    exit 1
fi

sed "s/{{SSID}}/$2/g" camera_test.ino > temp.ino
sed "s/{{PASS}}/$3/g" temp.ino > temp.ino

cp camera_test.ino temp2.ino
mv temp.ino camera_test.ino

arduino-cli -b esp32:esp32:esp32cam compile -v -e
arduino-cli upload -p $1 --fqbn esp32:esp32:esp32cam .

rm temp.ino
mv temp2.ino camera_test.ino
