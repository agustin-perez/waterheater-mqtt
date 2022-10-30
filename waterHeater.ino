#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>
#include <DHT.h>
#include <ESP8266WiFi.h>

//DEFINITIONS
#define boardLed 2 //D4
#define relay 5 //D1
#define dht11 4 //D2
#define DHTTYPE DHT11
#ifndef STASSID
#define STAMqttServerAddress ""
#define STAMqttUserName ""
#define STAMqttPwd ""
#define STAMqttClientID  "WaterHeater"
#endif

const char* mqttServerAddress = STAMqttServerAddress;
const char* mqttUserName = STAMqttUserName;
const char* mqttPwd = STAMqttPwd;
const char* mqttClientID = STAMqttClientID;
const int stepsPerRevolution = 2048;
const int ldr = A0;
bool prevThermostat;
bool thermostat = false;
float prevTemp;
String strTopic;
String strPayload;
unsigned long previousMillis = 0;   
const long interval = 1000;
static const char HELLO_PAGE[] PROGMEM = R"(
{ "title": "Water Heater", "uri": "/", "menu": true, "element": [
    { "name": "caption", "type": "ACText", "value": "<h2>Water Heater - 40lt</h2>",  "style": "text-align:center;color:#2f4f4f;padding:10px;" },
    { "name": "content", "type": "ACText", "value": "ESP8266 management page" } ]
}
)";    

//INITIALIZATIONS
WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server;                              
AutoConnect portal(server);   
AutoConnectConfig config;           
AutoConnectAux hello;      
DHT dht(dht11, DHTTYPE, 15);

//MQTT CALLBACK FUNCTION
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char*)topic);
  int payloadInt = (int)payload;

  //RELAY SET
  if (strTopic == "cmnd/waterHeater/power") {
    switch(atoi((char*)payload)){
      case 1:
        Serial.println("Relay ON");
        digitalWrite(relay, HIGH);
        client.publish("stat/waterHeater/power", "on"); 
      break;
      case 2:
        Serial.println("Relay OFF");
        digitalWrite(relay, LOW);
        client.publish("stat/waterHeater/power", "off"); 
      break;
    }
  }
}

//MQTT RECONNECT
void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttClientID, mqttUserName, mqttPwd)) {
      Serial.println("Connected to Home Assistant MQTT Broker");
      //MQTT SUBSCRIPTIONS
      client.subscribe("avail/waterHeater");
      client.subscribe("cmnd/waterHeater/temp");
      client.subscribe("cmnd/waterHeater/thermostat");
      client.subscribe("cmnd/waterHeater/power");
      client.subscribe("stat/waterHeater/sensor");
      client.subscribe("stat/waterHeater/power");
      client.publish("avail/waterHeater", "Online");
    } else {
      Serial.print("Failed: ");
      Serial.print(client.state());
      Serial.println("Trying again in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  pinMode(ldr, INPUT);
  dht.begin();
  
  client.setServer(mqttServerAddress, 1883);
  client.setCallback(mqttCallback);
  config.ota = AC_OTA_BUILTIN;      
  portal.config(config);           
  hello.load(HELLO_PAGE);         
  portal.join({ hello });           
  portal.begin();   

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
}

void loop() {
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();
  portal.handleClient(); 
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    float temp = dht.readTemperature();
    int ldrStatus = analogRead(ldr);
    if (temp != prevTemp){
      client.publish("stat/waterHeater/sensor", String(temp).c_str());
      Serial.printf("Temp: %.2f \n", temp, "°C"); 
      prevTemp = temp;
    }
    
    if (ldrStatus > 300){
      thermostat = true;
    } else{
      thermostat = false;
    }

    if (prevThermostat != thermostat){
      if (thermostat){
        Serial.println("Thermostat ON");
        client.publish("stat/waterHeater/thermostat", "on");  
        prevThermostat = thermostat;
      } else {
       Serial.println("Thermostat OFF");
        client.publish("stat/waterHeater/thermostat", "off");    
        prevThermostat = thermostat;
      }
    }
    Serial.printf("Thermostat value: %d \n", ldrStatus);                       
  }
}
