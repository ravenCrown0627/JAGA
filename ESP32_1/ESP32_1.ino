/*
    ESP32 Node A
    Feature connected:
    1. Fan
    2. Light
    3. Button
*/
#include <WiFi.h>
#include <PubSubClient.h>

#define MSG_BUFFER_SIZE	(50)

// Update these with values suitable for your network.
const char* ssid        = "Connected WiFi Name";
const char* password    = "Connected WiFi Password";
const char* mqtt_server = "Ip address of the MQTT Broker";

// Pin assignment
const int FAN_RELAY_PIN       = 23;       // Fan's relay ctrl pin
const int LIGHT_RELAY_PIN     = 22;       // Light's relay ctrl pin
const int EMERGENCY_BTN_PIN   = 27;       // LAY37 btn ctrl pin

// Topic publish
const char* emergency_topic = "outTopic/emergency";   // Topic send emergency signal

// Topic subscribe
const char* fan_topic       = "inTopic/fan";          // Topic ctrl fan
const char* light_topic     = "inTopic/light";        // Topic ctrl light

// Global variable
int previousBtnState        = LOW;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

char out_msg[MSG_BUFFER_SIZE];

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
        clientId += "Node A";
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(emergency_topic, "Successfully reconnect Node A");
            // ... and resubscribe
            client.subscribe(fan_topic);
            client.subscribe(light_topic);

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
    // For topic: inTopic/fan
    if (String(topic) == fan_topic) {
        if (in_msg == "ON") {
            Serial.println("Turn on fan");
            digitalWrite(FAN_RELAY_PIN, HIGH);
        }
        else if (in_msg == "OFF") {
            Serial.println("Turn off fan");
            digitalWrite(FAN_RELAY_PIN, LOW);
        }
    }
    // For topic: inTopic/light
    if (String(topic) == light_topic) {
        if (in_msg == "ON") {
            Serial.println("Turn on light");
            digitalWrite(LIGHT_RELAY_PIN, HIGH);
        }
        else if (in_msg == "OFF") {
            Serial.println("Turn off light");
            digitalWrite(LIGHT_RELAY_PIN, LOW);
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize pin behaviour
    pinMode(FAN_RELAY_PIN, OUTPUT);
    pinMode(LIGHT_RELAY_PIN, OUTPUT);
    pinMode(EMERGENCY_BTN_PIN, INPUT_PULLUP);
    // Setup comm. system
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    long now = millis();
    if (now - lastMsg > 2000) {
        // Here is the start point of writing what to publish out.
        int buttonState = digitalRead(EMERGENCY_BTN_PIN);

        if (buttonState == HIGH && previousBtnState == LOW) {
            // Tunr on buzzer
            // digitalWrite(EMERGENCY_BTN_PIN, HIGH);
            delay(200);
            // Save the message to send
            snprintf (out_msg, MSG_BUFFER_SIZE, "ON");
            Serial.print("Publish message [");
            Serial.print(emergency_topic);
            Serial.print("]: ");
            Serial.println(out_msg);
            client.publish(emergency_topic, out_msg);
        } else if (buttonState == LOW && previousBtnState == HIGH) {
            // Tunr on buzzer
            // digitalWrite(EMERGENCY_BTN_PIN, LOW);
            delay(200);
            // Save the message to send
            snprintf (out_msg, MSG_BUFFER_SIZE, "OFF");
            Serial.print("Publish message [");
            Serial.print(emergency_topic);
            Serial.print("]: ");
            Serial.println(out_msg);
            client.publish(emergency_topic, out_msg);
        }

        previousBtnState = buttonState;
    }
}