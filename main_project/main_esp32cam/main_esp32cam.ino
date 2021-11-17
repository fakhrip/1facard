#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <ESPino32CAM.h>
#include <ESPino32CAM_QRCode.h>

/* ------ State Stuff ------ */
enum DisplayState
{
    QRCODE_PAGE,    // QRCODE Display Menu Page
    NFC_PAGE,       // NFC or RFID Display Menu Page
    INPUTMENU_PAGE, // Input Data Menu Page
    /****************/
    CHOICE_PAGE, // Data Choice Page --- This is kind of the "gate" state between the top and bottom state ---
    /****************/
    DATAINPUT_PAGE, // Data Input Page
    DISPLAY_PAGE    // Data Displayer or Transmission Page
};

enum AppState
{
    MAIN_MENU,   // Iterating between QRCODE_PAGE, NFC_PAGE, and INPUTMENU_PAGE
    CHOICE_MENU, // Inside CHOICE_PAGE, iterating between data inside of it
                 // iterating between QRCODE and NFCRFID ChoiceState if was from INPUTMENU_PAGE
    PROCESS_MENU // Processing stuff (either inputting data or (display/output)-ing it)
};

enum ChoiceState
{
    INPUT_QRCODE,  // INPUT QRCODE data
    INPUT_NFCRFID, // INPUT NFC or RFID data
    OUTPUT_QRCODE, // OUTPUT QRCODE data
    OUTPUT_NFCRFID // OUTPUT NFC or RFID data
};

DisplayState previous_display_state = QRCODE_PAGE;
DisplayState current_display_state = QRCODE_PAGE;
AppState previous_app_state = MAIN_MENU;
AppState current_app_state = MAIN_MENU;
ChoiceState previous_choice_state = INPUT_QRCODE;
ChoiceState current_choice_state = INPUT_QRCODE;

char chosen_data[100];
/* ------------------------- */

/* ------ TFT Display Stuff ------ */
TFT_eSPI tft = TFT_eSPI();
/* ------------------------------- */

/* ------ Communications Stuff ------ */
const uint8_t pico_address[] = {0xD8, 0xA0, 0x1D, 0x5B, 0x3B, 0x98};
typedef struct MessageStruct
{
    /*  EVENT TYPE
     *  ----------
     *  [1] Change page (button click)
     *  [2] Data transfer (nfc/qrcode/rfid data)
     */
    int event_type;

    /*  EVENT TYPE
     *  ----------
     *  [-1] Dont Care 
     *  [1] RED Button
     *  [2] GREEN Button
     */
    int button_click;

    char data[200];
} MessageStruct;

char payload_data[200];

MessageStruct transfer_payload;
MessageStruct receive_payload;
/* ---------------------------------- */

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // Only for debugging purpose, remove if unnecessary
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *received_data, int len)
{
    memcpy(&receive_payload, received_data, sizeof(receive_payload));
    Serial.print("Bytes received: ");
    Serial.println(len);

    switch (receive_payload.event_type)
    {
    case 1:
        switch (receive_payload.button_click)
        {
        case 1:
            current_display_state = getNextDisplayState(current_app_state, current_display_state);
            break;
        case 2:
            current_app_state = getNextAppState(current_app_state, current_choice_state);
            break;

        default:
            Serial.println("Something wrong happened");
            Serial.println("Possibly wrong event_type sent or uninitialized button_click");
            break;
        }
        break;

    case 2:
        strcpy(payload_data, receive_payload.data);
        break;

    default:
        break;
    }
}

AppState getNextAppState(AppState cur_app_state, ChoiceState cur_current_choice_state)
{
    switch (cur_app_state)
    {
    case MAIN_MENU:
        current_display_state = CHOICE_PAGE;
        return CHOICE_MENU;
        break;

    case CHOICE_MENU:
        switch (cur_current_choice_state)
        {
        case INPUT_QRCODE:
        case INPUT_NFCRFID:
            current_display_state = DATAINPUT_PAGE;
            break;

        case OUTPUT_QRCODE:
        case OUTPUT_NFCRFID:
            current_display_state = DISPLAY_PAGE;
            break;

        default:
            break;
        }
        return PROCESS_MENU;
        break;

    case PROCESS_MENU:
        current_display_state = QRCODE_PAGE;
        current_choice_state = OUTPUT_QRCODE;
        return MAIN_MENU;
        break;

    default:
        break;
    }
}

DisplayState getNextDisplayState(AppState cur_app_state, DisplayState cur_disp_state)
{
    switch (cur_app_state)
    {
    case MAIN_MENU:
        switch (cur_disp_state)
        {
        case QRCODE_PAGE:
            current_choice_state = OUTPUT_NFCRFID;
            return NFC_PAGE;
            break;
        case NFC_PAGE:
            current_choice_state = INPUT_QRCODE;
            return INPUTMENU_PAGE;
            break;
        case INPUTMENU_PAGE:
            current_choice_state = OUTPUT_QRCODE;
            return QRCODE_PAGE;
            break;

        default:
            break;
        }
        break;

    case CHOICE_MENU:
        switch (current_choice_state)
        {
        case INPUT_QRCODE:
            current_choice_state = INPUT_NFCRFID;
            break;

        case INPUT_NFCRFID:
            current_choice_state = INPUT_QRCODE;
            break;

        case OUTPUT_QRCODE:
        case OUTPUT_NFCRFID:
            // TODO:
            // Iterate through existing data
            break;

        default:
            break;
        }
        return CHOICE_PAGE;
        break;

    case PROCESS_MENU:
        current_choice_state = OUTPUT_QRCODE;
        current_app_state = MAIN_MENU;
        return QRCODE_PAGE;
        break;

    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println("Starting ESP32CAM .....");
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register onsend callback
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, pico_address, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    // Register onreceive callback
    esp_now_register_recv_cb(OnDataRecv);
    initializeDisplay();
}

void loop()
{
    // Update FSM and redraw display if there are any changes
    if (previous_display_state != current_display_state ||
        previous_app_state != current_app_state ||
        previous_choice_state != current_choice_state)
    {
        Serial.printf("%d -- %d -- %d\n\n", current_app_state, current_display_state, current_choice_state);
        tft.fillScreen(TFT_WHITE);
        drawBorder();
        drawPage(current_display_state, current_choice_state);
        previous_display_state = current_display_state;
        previous_app_state = current_app_state;
        previous_choice_state = current_choice_state;
    }
}

void initializeDisplay()
{
    tft.init();
    tft.fillScreen(TFT_WHITE);
    drawBorder();
    drawPage(current_display_state, current_choice_state);
}

void drawPage(DisplayState page_num, ChoiceState chosen_data_type)
{
    switch (page_num)
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
        drawChoicePage(chosen_data_type);
        break;

    case DATAINPUT_PAGE:
        drawDataInputPage(chosen_data_type);
        break;

    case DISPLAY_PAGE:
        drawDisplayPage(chosen_data_type);
        break;

    default:
        // Should be unreachable
        Serial.println("Something wrong happened");
        Serial.println("Mistakenly set the display_state ?");
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

void drawChoicePage(ChoiceState chosen_data_type)
{
    int w = tft.width(), h = tft.height();
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);

    switch (chosen_data_type)
    {
    case OUTPUT_QRCODE:
    case OUTPUT_NFCRFID:
        // TODO:
        // SEND data to esp32pico (to kind of saying im ready),
        // depends on kind of data that want to grab (check using prev_page_num)
        // and then READ data from espnow (if data have not yet exist)
        // if data have existed, then iterate through the data, by checking current chosendata
        // from global var
        char data[10];
        if (chosen_data_type == INPUT_QRCODE)
        {
            strcpy(data, "qrcode");
        }
        else
        {
            strcpy(data, "nfc_rfid");
        }
        break;

    case INPUT_QRCODE:
        tft.setTextColor(TFT_DARKGREEN);
        tft.setTextSize(3);

        tft.setCursor(30, ((int)h / 2) - 65);
        tft.println("----------");
        tft.setCursor(30, ((int)h / 2) - 30);
        tft.println("* QRCODE");
        tft.setCursor(30, ((int)h / 2) + 5);
        tft.println("  NFC/RFID");
        tft.setCursor(30, ((int)h / 2) + 35);
        tft.println("----------");
        break;

    case INPUT_NFCRFID:
        tft.setTextColor(TFT_DARKGREEN);
        tft.setTextSize(3);

        tft.setCursor(30, ((int)h / 2) - 65);
        tft.println("----------");
        tft.setCursor(30, ((int)h / 2) - 30);
        tft.println("  QRCODE");
        tft.setCursor(30, ((int)h / 2) + 5);
        tft.println("* NFC/RFID");
        tft.setCursor(30, ((int)h / 2) + 35);
        tft.println("----------");
        break;

    default:
        break;
    }
}

void drawDataInputPage(ChoiceState chosen_data_type)
{
    switch (chosen_data_type)
    {
    case INPUT_QRCODE:
        tft.fillScreen(TFT_WHITE);
        // TODO:
        // SCAN for QR Code using ESPIno32CAM Library,
        // and if the user click the red button, it will stop scanning
        // (even when there are no qrcode scanned yet)
        break;

    case INPUT_NFCRFID:
        drawLoadingText();
        // TODO:
        // SEND data to esp32pico (to kind of saying im ready),
        // set the NFC Card to scan mode in esp32pico
        // and then READ data from espnow (write success / errors depending on the result)
        break;

    default:
        break;
    }
}

void drawDisplayPage(ChoiceState chosen_data_type)
{
    switch (chosen_data_type)
    {
    case OUTPUT_QRCODE:
        tft.fillScreen(TFT_WHITE);
        // TODO:
        // GENERATE QR Code based on chosenData
        // and then DISPLAY it to the tft screen using https://github.com/yoprogramo/QRcode_eSPI
        break;

    case OUTPUT_NFCRFID:
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
    tft.setTextSize(1);

    tft.setCursor(30, 30);
    tft.println("Loading ...");
}