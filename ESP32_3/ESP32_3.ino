/*
    ESP32 Node C
    Feature connected:
    1. Humidifier
    2. OLED display
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Macro for OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define HUMID_PIN 25

#define MSG_BUFFER_SIZE (50)

// Update these with values suitable for your network.
const char* ssid        = "Connected WiFi Name";
const char* password    = "Connected WiFi Password";
const char* mqtt_server = "Ip address of the MQTT Broker";

// Topic subscribe
const char* mq_135_CO2_topic    = "outTopic/MQ_135/CO2";    // Topic receive data read from MQ-135
const char* mq_135_CO_topic     = "outTopic/MQ_135/CO";     // Topic receive data read from MQ-135
const char* emergency_topic     = "outTopic/emergency";     // Receive emergency signal
const char* humid_topic         = "inTopic/Humidifier";     // Topic receive signal of humidifier

// Global variable
float CO2_val = 0;
float CO_val = 0;
bool is_flammable_gas = false;
unsigned long lastMsg = 0;
char out_msg[MSG_BUFFER_SIZE];

// Bitmap
static const unsigned char PROGMEM image_Warning_30x23_bits[] = {
    0x00,0x03,0x00,0x00,0x00,0x07,0x80,0x00,0x00,0x0f,0xc0,0x00,0x00,0x0f,0xc0,0x00,0x00,0x1f,
    0xe0,0x00,0x00,0x3c,0xf0,0x00,0x00,0x3c,0xf0,0x00,0x00,0x7c,0xf8,0x00,0x00,0xfc,0xfc,0x00,
    0x00,0xfc,0xfc,0x00,0x01,0xfc,0xfe,0x00,0x03,0xfc,0xff,0x00,0x03,0xfc,0xff,0x00,0x07,0xfc,
    0xff,0x80,0x0f,0xfc,0xff,0xc0,0x0f,0xfc,0xff,0xc0,0x1f,0xfc,0xff,0xe0,0x3f,0xff,0xff,0xf0,
    0x3f,0xff,0xff,0xf0,0x7f,0xfc,0xff,0xf8,0xff,0xfc,0xff,0xfc,0xff,0xff,0xff,0xfc,0x7f,0xff,
    0xff,0xf8
};

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP32Client-";
        clientId += "Node C";
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // // Once connected, publish an announcement...
            // client.publish(connection, "Successfully reconnect Node C");
            // ... and resubscribe
            client.subscribe(mq_135_CO2_topic);
            client.subscribe(mq_135_CO_topic);
            client.subscribe(emergency_topic);
            client.subscribe(humid_topic);

        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String in_msg;

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
        in_msg += (char)payload[i];
    }
    Serial.println();

    // Here is the starting point of what to do with the data received from the MQTT topic
    // For topic: inTopic/Humidifier
    if (String(topic) == humid_topic) {
        if (in_msg == "ON") {
            Serial.println("Turn on humidifier");
            digitalWrite(HUMID_PIN, LOW);
        } else if (in_msg == "OFF") {
            Serial.println("Turn off humidifier");
            digitalWrite(HUMID_PIN, HIGH);
        }
    }

    if (String(topic) == mq_135_CO2_topic) {
        CO2_val = atof((char*) payload);
    }

    if (String(topic) == mq_135_CO_topic) {
        CO_val = atof((char*) payload);
    }

    if (String(topic) == emergency_topic) {
        if (in_msg == "FIRE") {
            is_flammable_gas = true;
            Serial.print("FIRE DETECTED");
        }
        else if (in_msg == "SAFE") {
            is_flammable_gas = false;
            Serial.println("SAFE");
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize pin behaviour
    pinMode(HUMID_PIN, OUTPUT);
    // Setup comm. system
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    // Turn off the humidifier at initial (active low)
    digitalWrite(HUMID_PIN, HIGH);
    setup_OLED_display();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    if (!is_flammable_gas) {
        // Need to clear & update the display
        display.clearDisplay();
        draw_CO2(CO2_val);
        // Divider
        display.drawLine(66, 1, 66, 62, 1);
        draw_CO(CO_val);
        display.display();  //Flush characters to screen
        // Need to delay 
        delay(500);
    }
    else {
        draw_danger();
        display.clearDisplay();
        display.display();
        delay(200);
    }
}

void setup_OLED_display() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1)
        ;
    }

    display.display();       // Clear the display buffer
    display.clearDisplay();  // Clear the display
}

void draw_CO2(float val) {
    // CO2
    // Title
    display.drawCircle(31, 36, 21, 1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(23, 3);
    display.setTextWrap(false);
    display.print("CO2");
    // Percentage of CO2 value
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    if (val < 10) {
        display.setCursor(18, 33);  //For 3sf
        display.setTextWrap(false);
        display.print(val, 2);
    } else if (val > 10 && val < 100) {
        display.setCursor(14, 33);  //For 4sf
        display.setTextWrap(false);
        display.print(val, 2);
    } else if (val >= 100) {
        display.setCursor(21, 33);  //For 100%
        display.setTextWrap(false);
        display.print(100);
    }
    display.print("%");
}

void draw_CO(float val) {
    // CO
    display.drawCircle(98, 36, 21,  1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(93, 3);
    display.setTextWrap(false);
    display.print("CO");
    // Percentage of CO value
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    if (val < 10) {
        display.setCursor(85, 33);  // For 3sf
        display.setTextWrap(false);
        display.print(val, 2);
    } else if (val > 10 && val < 100) {
        display.setCursor(81, 33);  // For 4sf
        display.setTextWrap(false);
        display.print(val, 2);
    } else if (val >= 100) {
        display.setCursor(88, 33);  //For 100%
        display.setTextWrap(false);
        display.print(100);
    }
    display.print("%");
}

void draw_danger() {
    display.clearDisplay();
    display.drawBitmap(50, 30, image_Warning_30x23_bits, 30, 23, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setCursor(20, 9);
    display.setTextWrap(false);
    display.print("DANGER!!");
    display.display();
    delay(200);
}