#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include <FunctionalInterrupt.h>

#define RED_BUTTON 25
#define GREEN_BUTTON 26

// REPLACE WITH THE MAC Address of your receiver
uint8_t broadcastAddress[] = {0x10, 0x52, 0x1C, 0x75, 0x87, 0x20};

// Variable to store if sending data was successful
char incomingDataString[100];

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message
{
    char data[100];
} struct_message;

// Create a struct_message called BME280Readings to hold sensor readings
struct_message reading;

// Create a struct_message to hold incoming sensor readings
struct_message incoming;

class Button
{
public:
    Button(uint8_t reqPin) : PIN(reqPin)
    {
        pinMode(PIN, INPUT_PULLUP);
        attachInterrupt(PIN, std::bind(&Button::isr, this), FALLING);
    };

    ~Button()
    {
        detachInterrupt(PIN);
    }

    void IRAM_ATTR isr()
    {
        // Prevent bad ripple / noise
        if (millis() - lastMillis > 200)
        {
            lastMillis = millis();
            numberKeyPresses += 1;
            pressed = true;
        }
    }

    void checkPressed()
    {
        if (pressed)
        {
            // Send message via ESP-NOW
            sprintf(reading.data, "Button on pin %u has been pressed %u times\n", PIN, numberKeyPresses);
            esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&reading, sizeof(reading));

            if (result == ESP_OK)
            {
                Serial.println("Sent with success");
            }
            else
            {
                Serial.println("Error sending the data");
            }
            Serial.printf(reading.data);
            pressed = false;
        }
    }

private:
    const uint8_t PIN;
    volatile uint32_t numberKeyPresses;
    volatile bool pressed;
    volatile uint32_t lastMillis;
};

Button red_button(RED_BUTTON);
Button green_button(GREEN_BUTTON);

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&incoming, incomingData, sizeof(incoming));
    Serial.print("Bytes received: ");
    Serial.println(len);
    strcpy(incomingDataString, incoming.data);
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.printf("starting......");

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
    red_button.checkPressed();
    green_button.checkPressed();
}