#include <WiFi.h>
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
                sendData(1, 1, "");
                break;

            case GREEN_BUTTON:
                sendData(1, 2, "");
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
    strcpy(payload_data, receive_payload.data);
}

void sendData(int payoad_event_type, int payload_button_click, char *payload_data_send)
{
    transfer_payload.event_type = payoad_event_type;
    transfer_payload.button_click = payload_button_click;
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

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.printf("Starting ESP32PICO .....");
    initializeCommunications();
}

void loop()
{
    red_button.checkPressed();
    green_button.checkPressed();
}

void initializeCommunications()
{
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