/*
    ESP32 Node B
    Feature connected:
    1. PIR motion sensor
    2. MQ-135
    3. MQ-2
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <MQUnifiedsensor.h>

//Definitions
#define Board                   "ESP-32"
#define Voltage_Resolution      5
#define ADC_Bit_Resolution      12      // For ESP-32
#define Type_MQ_135             "MQ-135"
#define RatioMQ135CleanAir      3.6     // RS / R0 = 3.6 ppm 
#define Type_MQ_2               "MQ-2"
#define RatioMQ2CleanAir        9.83    //RS / R0 = 9.83 ppm 

//#define calibration_button 13 //Pin to calibrate your sensor
#define MSG_BUFFER_SIZE	(50)

// Update these with values suitable for your network.
const char* ssid        = "Connected WiFi Name";
const char* password    = "Connected WiFi Password";
const char* mqtt_server = "Ip address of the MQTT Broker";

// Pin assignment
const int PIR_PIN       = 35;       // PIR motion pin
const int MQ_135_PIN    = 34;       // MQ-135 pin
const int MQ_2_PIN      = 32;       // MQ-135 pin
const int BUZ_PIN       = 5;       // Buzzer pin

// Topic publish
const char* pir_motion_topic  = "outTopic/PIR";          // Topic send data read from PIR
const char* mq_135_CO2_topic  = "outTopic/MQ_135/CO2";   // Topic send data read from MQ-135
const char* mq_135_CO_topic   = "outTopic/MQ_135/CO";    // Topic send data read from MQ-135
const char* mq_2_topic        = "outTopic/MQ_2";         // Topic send alert signal from MQ-2
const char* fan_topic         = "outTopic/fan";          // Topic ctrl fan
const char* light_topic       = "outTopic/light";        // Topic ctrl light
const char* humid_topic       = "outTopic/Humidifier";    // Topic receive signal of humidifier

// Topic subscribe
const char* emergency_topic   = "outTopic/emergency";  // Receive emergency signal
const char* buzzer_topic      = "inTopic/buzzer";      // Topic receive buzzer signalw

// Global variable
int previousMotionDetected    = LOW;      // Previous motion state
int previousMQ2StateDetected  = LOW;      // Previous state of gas sensor
int previousHumidifierState   = LOW;
float CO2_val                 = 0;
float CO_val                  = 0;

// Instantiate object
//Declare Sensor
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_135_PIN, Type_MQ_135);
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_2_PIN, Type_MQ_2);

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
        clientId += "Node B";
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(pir_motion_topic, "Successfully reconnect Node B");
            // ... and resubscribe
            client.subscribe(buzzer_topic);
            client.subscribe(emergency_topic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup_mq_135() {
    // Set up for MQ-135
    // Math model to calculate PPM
    MQ135.setRegressionMethod(1);
    // Init sensor
    MQ135.init();
    /*****************************  MQ Calibration ********************************************/ 
    Serial.print("Calibrating MQ-135 please wait.");
    float calcR0 = 0;
    for(int i = 1; i<=10; i ++)
    {
        MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
        calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
        Serial.print(".");
    }
    MQ135.setR0(calcR0/10);
    Serial.println("  done!.");

    if(isinf(calcR0)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
    if(calcR0 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
    /*****************************  MQ CAlibration Debug ********************************************/ 
    // MQ135.serialDebug(true);
    // Serial.println("** Values from MQ-135 ****");
    // Serial.println("|    CO   |  Alcohol |   CO2  |  Toluen  |  NH4  |  Aceton  |");
}

void setup_mq_2() {
    // Set up for MQ-2
    // Math model to calculate PPM
    MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
    // Init sensor
    MQ2.init(); 
    /*****************************  MQ Calibration ********************************************/ 
    Serial.print("Calibrating MQ-2 please wait.");
    float calcR0 = 0;
    for(int i = 1; i<=10; i ++)
    {
        MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
        calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
        Serial.print(".");
    }
    MQ2.setR0(calcR0/10);
    Serial.println("  done!.");
    if(isinf(calcR0)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
    if(calcR0 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}

    // MQ2.serialDebug(true);
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
    // For topic: NodeB/outTopic/buzzer
    if (String(topic) == buzzer_topic || String(topic) == emergency_topic) {
        if (in_msg == "ON") {
            Serial.println("Turn on buzzer");
            // tone(BUZ_PIN,200,100);
            // delay(200);
            // tone(BUZ_PIN,500,300);
            digitalWrite(BUZ_PIN, HIGH);
        }
        else if (in_msg == "OFF") {
            Serial.println("Turn off buzzer");
            // noTone(BUZ_PIN);
            digitalWrite(BUZ_PIN, LOW);
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize pin behaviour
    pinMode(PIR_PIN, INPUT);
    pinMode(MQ_135_PIN, INPUT);
    pinMode(BUZ_PIN, OUTPUT);

    // WiFi need to be setup first before sensors calibration
    // Setup comm. system
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    // // Setup for gas sensor
    setup_mq_135();
    setup_mq_2();    
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    long now = millis();
    if (now - lastMsg > 2000) {
        // Here is the start point of writing what to publish out.
        // Read the analog value of MQ-135 and publish
        MQ135.update();
        MQ2.update();
        updateLPGReading();
        updateCO2Reading();
        updateCOReading();

        if ((CO_val > 20 || CO2_val > 450) && !previousHumidifierState) {
            Serial.println("On humidifier");
            snprintf(out_msg, MSG_BUFFER_SIZE, "ON");
            client.publish(humid_topic, out_msg);

            // Flag to trigger send turn off humidifier once
            previousHumidifierState = HIGH;
        }
        else if (CO_val < 20 && CO2_val > 50 && previousHumidifierState){
            Serial.println("Off humidifier");
            snprintf(out_msg, MSG_BUFFER_SIZE, "OFF");
            client.publish(humid_topic, out_msg);

            // Flag to trigger send turn on humidifier once
            previousHumidifierState = LOW;
        }
        checkPIRMotion();

        lastMsg = now;
    }
}

void updateCO2Reading() {
    MQ135.setA(110.47); MQ135.setB(-2.862);
    CO2_val = 400 + MQ135.readSensor();
    if (CO2_val >= 1000) 
        CO2_val = 1000;
    sprintf(out_msg, "%.2f", (CO2_val / 10));
    Serial.printf("CO2 reading: %.2f, Done publish\n", CO2_val);
    client.publish(mq_135_CO2_topic, out_msg);
}

void updateCOReading() {
    MQ135.setA(605.18); MQ135.setB(-3.937);
    CO_val = MQ135.readSensor();
    if (CO_val >= 100)
        CO_val = 100;
    sprintf(out_msg, "%.2f", CO_val);
    Serial.printf("CO reading: %.2f, Done publish\n", CO_val);
    client.publish(mq_135_CO_topic, out_msg);
}

void updateLPGReading() {
    MQ2.setA(574.25); MQ2.setB(-2.222); 

    float LPG_val = MQ2.readSensor();

    if (LPG_val > 200 && !previousMQ2StateDetected) {
        // Send notification to user
        snprintf(out_msg, MSG_BUFFER_SIZE , "Flammable gas detected!");
        Serial.printf("LPG value measured: %.2f\n", LPG_val);
        client.publish(mq_2_topic, out_msg);
        // Turn on buzzer
        digitalWrite(BUZ_PIN, HIGH);
        // Send alert notification
        snprintf(out_msg, MSG_BUFFER_SIZE , "FIRE");
        client.publish(emergency_topic, out_msg);

        // Flag to trigger send turn off buzzer once
        previousMQ2StateDetected = HIGH;
    }
    else if (LPG_val < 200 && previousMQ2StateDetected) {
        // Turn off the buzze when it is no more detected
        digitalWrite(BUZ_PIN, LOW);
        // Shut off alert notification
        snprintf(out_msg, MSG_BUFFER_SIZE , "SAFE");
        client.publish(emergency_topic, out_msg);

        // Flag to trigger send turn on buzzer once
        previousMQ2StateDetected = LOW;
    }
}

// Turn on all appliances
void turnOnAllAppliances() {
    snprintf (out_msg, MSG_BUFFER_SIZE, "ON");
    client.publish(fan_topic, out_msg);
    client.publish(light_topic, out_msg);
}

// Turn off all appliances
void turnOffAllAppliances() {
    snprintf (out_msg, MSG_BUFFER_SIZE, "OFF");
    client.publish(fan_topic, out_msg);
    client.publish(light_topic, out_msg);
}

// Check PIR motion sensor
void checkPIRMotion() {
    // Check the status of PIR motion and publish
    bool motionDetected = digitalRead(PIR_PIN);
    if (motionDetected != previousMotionDetected) {
        if (motionDetected == HIGH) {
            turnOnAllAppliances();
        }
        else {
            turnOffAllAppliances();
        }
    
        previousMotionDetected = motionDetected;
    }
}