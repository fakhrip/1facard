#include <FS.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <Arduino.h>
#include <esp_now.h>
#include <FunctionalInterrupt.h>

#define RED_BUTTON 25
#define GREEN_BUTTON 26

/* ------ Communications Stuff ------ */
const uint8_t cam_address[] = {0x10, 0x52, 0x1C, 0x75, 0x87, 0x20};
typedef struct MessageStruct
{
    /*  EVENT TYPE
     *  ----------
     *  [1] Change page (button click)
     *  [2] Data transfer (nfc/qrcode/rfid data)
     */
    int event_type;

    /*  BUTTON TYPE
     *  -----------
     *  [-1] Dont Care 
     *  [1] RED Button
     *  [2] GREEN Button
     */
    int button_click;

    /*  DATA TYPE
     *  ---------
     *  [-1] Dont Care
     *  [1] QRCODE data
     *  [2] NFC/RFID data
     */
    int data_type;

    char data[200];
} MessageStruct;

char payload_data[200];

MessageStruct transfer_payload;
MessageStruct receive_payload;
/* ---------------------------------- */

void sendData(int payoad_event_type, int payload_button_click, int payload_data_type, char *payload_data_send)
{
    transfer_payload.event_type = payoad_event_type;
    transfer_payload.button_click = payload_button_click;
    transfer_payload.data_type = payload_data_type;
    strcpy(transfer_payload.data, payload_data_send);
    esp_err_t result = esp_now_send(cam_address, (uint8_t *)&transfer_payload, sizeof(transfer_payload));

    if (result == ESP_OK)
    {
        Serial.println("Sent with success");
    }
    else
    {
        Serial.println("Error sending the data");
    }
}

class Button
{
public:
    Button(uint8_t reqPin) : button_pin(reqPin)
    {
        pinMode(button_pin, INPUT_PULLUP);
        attachInterrupt(button_pin, std::bind(&Button::isr, this), FALLING);
    };

    ~Button()
    {
        detachInterrupt(button_pin);
    }

    void IRAM_ATTR isr()
    {
        // Prevent bad ripple / noise
        if (millis() - last_millis > 200)
        {
            last_millis = millis();
            pressed = true;
        }
    }

    void checkPressed()
    {
        if (pressed)
        {
            // Send message via ESP-NOW
            switch (button_pin)
            {
            case RED_BUTTON:
                sendData(1, 1, -1, "");
                break;

            case GREEN_BUTTON:
                sendData(1, 2, -1, "");
                break;

            default:
                break;
            }

            Serial.printf("%s button on pin has been pressed\n", (button_pin == RED_BUTTON ? "RED" : "GREEN"));
            pressed = false;
        }
    }

private:
    const uint8_t button_pin;
    volatile bool pressed;
    volatile uint32_t last_millis;
};

Button red_button(RED_BUTTON);
Button green_button(GREEN_BUTTON);

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
        // Should be unreachable
        Serial.println("Something wrong happened");
        Serial.println("Must've sent a wrong event_type");
        break;

    case 2:
        strcpy(payload_data, receive_payload.data);
        Serial.printf("Got data: %s\n\n", payload_data);
        switch (receive_payload.data_type)
        {
        case 1:
            writeFile(SPIFFS, "/qrcode_data.txt", payload_data);
            break;

        case 2:
            /* code */
            break;

        default:
            break;
        }
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

    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    Serial.printf("Starting ESP32PICO .....");
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
    memcpy(peerInfo.peer_addr, cam_address, 6);
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
}

void loop()
{
    red_button.checkPressed();
    green_button.checkPressed();
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("− failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(" − not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("− failed to open file for reading");
        return;
    }

    Serial.println("− read from file:");
    while (file.available())
    {
        Serial.write(file.read());
    }
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("− failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("− file written");
    }
    else
    {
        Serial.println("− frite failed");
    }
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("− failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("− message appended");
    }
    else
    {
        Serial.println("− append failed");
    }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2))
    {
        Serial.println("− file renamed");
    }
    else
    {
        Serial.println("− rename failed");
    }
}

void deleteFile(fs::FS &fs, const char *path)
{
    Serial.printf("Deleting file: %s\r\n", path);
    if (fs.remove(path))
    {
        Serial.println("− file deleted");
    }
    else
    {
        Serial.println("− delete failed");
    }
}