#include "SPI.h"
#include "TFT_eSPI.h"
#include "ESPino32CAM.h"
#include "ESPino32CAM_QRCode.h"

enum AppState
{
    QRCODE_PAGE,    // QRCODE Display Menu Page
    NFC_PAGE,       // NFC or RFID Display Menu Page
    INPUTMENU_PAGE, // Input Data Menu Page
    CHOICE_PAGE,    // Data Choice Page --- This is kind of the "gate" state between the top and bottom state ---
    DATAINPUT_PAGE, // Data Input Page
    DISPLAY_PAGE    // Data Displayer or Transmission Page
};

enum ChoiceState
{
    QRCODE, // QRCODE data
    NFCRFID // NFC or RFID data
};

AppState app_state = QRCODE_PAGE;
ChoiceState choice_state = QRCODE;

char chosenData[30];

TFT_eSPI tft = TFT_eSPI();

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println("TFT_eSPI library test!");
    initializeDisplay();
}

void loop(void)
{
}

void initializeDisplay()
{
    tft.init();
    tft.fillScreen(TFT_WHITE);
    drawBorder();
    drawPage(app_state, app_state, choice_state);
}

void drawPage(AppState pageNum, AppState prevPageNum, ChoiceState chosenDataType)
{
    switch (pageNum)
    {
    case QRCODE_PAGE:
        drawQRCodePage();
        break;

    case NFC_PAGE:
        drawNFCPage();
        break;

    case INPUTMENU_PAGE:
        drawInputMenuPage();
        break;

    case CHOICE_PAGE:
        drawChoicePage(prevPageNum);
        break;

    case DATAINPUT_PAGE:
        drawDataInputPage(chosenDataType);
        break;

    case DISPLAY_PAGE:
        drawDisplayPage(chosenDataType);
        break;

    default:
        // Should be unreachable
        break;
    }
}

void drawBorder()
{
    int x, y, w = tft.width(), h = tft.height();

    // Draw top and left borders
    for (y = 0; y < 20; y++)
        tft.drawFastHLine(0, y, w, TFT_BROWN);
    for (x = 0; x < 20; x++)
        tft.drawFastVLine(x, 0, h, TFT_BROWN);

    // Draw bottom and right borders
    for (y = h; y > h - 20; y--)
        tft.drawFastHLine(0, y, w, TFT_BROWN);
    for (x = w; x > w - 20; x--)
        tft.drawFastVLine(x, 0, h, TFT_BROWN);
}

void drawQRCodePage()
{
    int w = tft.width(), h = tft.height();
    tft.setTextColor(TFT_DARKGREEN);
    tft.setTextSize(3);

    tft.setCursor(60, ((int)h / 2) - 50);
    tft.println("-------");
    tft.setCursor(60, ((int)h / 2) - 15);
    tft.println("QR CODE");
    tft.setCursor(60, ((int)h / 2) + 20);
    tft.println("-------");
}

void drawNFCPage()
{
    int w = tft.width(), h = tft.height();
    tft.setTextColor(TFT_DARKGREEN);
    tft.setTextSize(3);

    tft.setCursor(50, ((int)h / 2) - 50);
    tft.println("--------");
    tft.setCursor(50, ((int)h / 2) - 15);
    tft.println("NFC/RFID");
    tft.setCursor(50, ((int)h / 2) + 20);
    tft.println("--------");
}

void drawInputMenuPage()
{
    int w = tft.width(), h = tft.height();
    tft.setTextColor(TFT_DARKGREEN);
    tft.setTextSize(3);

    tft.setCursor(35, ((int)h / 2) - 50);
    tft.println("----------");
    tft.setCursor(35, ((int)h / 2) - 15);
    tft.println("INPUT DATA");
    tft.setCursor(35, ((int)h / 2) + 20);
    tft.println("----------");
}

void drawChoicePage(AppState prevPageNum)
{
    int w = tft.width();
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);

    switch (prevPageNum)
    {
    case QRCODE_PAGE:
    case NFC_PAGE:
        // TODO:
        // SEND data to esp32pico (to kind of saying im ready),
        // depends on kind of data that want to grab (check using prevPageNum)
        // and then READ data from serial
        char data[10];
        if (prevPageNum == QRCODE_PAGE)
        {
            strcpy(data, "qrcode");
        }
        else
        {
            strcpy(data, "nfc_rfid");
        }
        break;

    case INPUTMENU_PAGE:
        break;

    default:
        break;
    }
}

void drawDataInputPage(ChoiceState chosenDataType)
{
    switch (chosenDataType)
    {
    case QRCODE:
        tft.fillScreen(TFT_WHITE);
        // TODO:
        // SCAN for QR Code using ESPIno32CAM Library,
        // and if the user click the red button, it will stop scanning
        // (even when there are no qrcode scanned yet)
        break;

    case NFCRFID:
        drawLoadingText();
        // TODO:
        // SEND data to esp32pico (to kind of saying im ready),
        // set the NFC Card to scan mode in esp32pico
        // and then READ data from serial (write success / errors depending on the result)
        break;

    default:
        break;
    }
}

void drawDisplayPage(ChoiceState chosenDataType)
{
    switch (chosenDataType)
    {
    case QRCODE:
        tft.fillScreen(TFT_WHITE);
        // TODO:
        // GENERATE QR Code based on chosenData
        // and then DISPLAY it to the tft screen using https://github.com/yoprogramo/QRcode_eSPI
        break;

    case NFCRFID:
        drawLoadingText();
        // TODO:
        // SEND data to esp32pico about the chosenData,
        // to tell it what data to be written to the NFC Card
        // and then READ data from serial (write success / errors depending on the result)
        break;

    default:
        break;
    }
}

void drawLoadingText()
{
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);

    tft.setCursor(30, 30);
    tft.println("Loading ...");
}