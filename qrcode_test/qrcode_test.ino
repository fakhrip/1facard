#include <SPI.h>
#include <TFT_eSPI.h>
#include <qrcode_espi.h>

TFT_eSPI display = TFT_eSPI();
QRcode_eSPI qrcode(&display);

void setup()
{
    Serial.begin(115200);
    Serial.println("");
    Serial.println("Starting...");

    display.begin();
    qrcode.init();
    display.fillScreen(TFT_WHITE);
    qrcode.create("Hello world.");
}

void loop() {}