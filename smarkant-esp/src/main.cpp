/*
 * This file is part of Smarkant project
 *
 * (C) 2017 Dirk Grappendorf, www.grappendorf.net
 */

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <AWSWebSocketClient.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
#include "config.h"

const int PIN_STATUS_LED = 2;
const int JSON_BUFFER_LENGTH = 512;
const int NUM_POSITION_BUTTONS = 4;
const uint16_t HEIGHT_MAX = 6000;
const uint16_t HEIGHT_MIN = 500;
const unsigned long WAIT_FOR_I2C_BYTES_TIMEOUT_MS = 1000;
const uint8_t I2C_ADDRESS = 0x10;
const int WEBSOCKET_PORT = 443;
const int WEBSOCKET_BUFFER_SIZE = 1000;
const int MQTT_MAX_PACKAGE_SIZE = 512;
const int MQTT_MAX_MESSAGE_HANDLERS = 1;
const int MQTT_YIELD_TIMEOUT_MS = 10;
const unsigned long SERIAL_BAUD_RATE = 115200;

enum I2CCommand {
  I2C_CMD_NOOP,
  I2C_CMD_MOVE_STOP,
  I2C_CMD_MOVE_UP,
  I2C_CMD_MOVE_DOWN,
  I2C_CMD_MOVE_HEIGHT,
  I2C_CMD_MOVE_POSITION,
  I2C_CMD_STORE_POSITION,
  I2C_CMD_STORE_CURRENT_POSITION,
  I2C_CMD_STORE_THRESHOLD,
  I2C_CMD_READ_HEIGHT,
  I2C_CMD_READ_HEIGHT_THRESHOLD,
  I2C_CMD_READ_POSITIONS
};

MDNSResponder mdns;
ESP8266WebServer server(80);
AWSWebSocketClient awsIotClient(WEBSOCKET_BUFFER_SIZE);
IPStack mqttIpStack(awsIotClient);
MQTT::Client<IPStack, Countdown, MQTT_MAX_PACKAGE_SIZE, MQTT_MAX_MESSAGE_HANDLERS> *mqttClient = NULL;

void log(const char *str, ...);
void logProgress();
void logProgressEnd();
void setup();
void setupWiFi();
void setupOTA();
void setupWebServer();
void setupAwsIot();
void loop();
bool waitForI2CBytesAvailable(int waitForNumBytess);
bool awsIotConnect ();
void awsIotSubscribeToShadowUpdates();
void awsIotMessageReceived(MQTT::MessageData& message);
void tableStop();
void tableMoveUp();
void tableMoveDown();
void tableMoveToPosition(int position);
void tableMoveToHeight(int height);

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  log("Smarkant ready...");

  Wire.begin();

  setupWiFi();
  setupOTA();
  setupWebServer();
  setupAwsIot();
}

void setupWiFi() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  WiFi.begin(WLAN_SSID, WLAN_PASSPHRASE);
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_STATUS_LED, digitalRead(PIN_STATUS_LED) == LOW ? HIGH : LOW);
    delay(100);
  }
  digitalWrite(PIN_STATUS_LED, HIGH);
}

void setupOTA() {
  mdns.begin(HOSTNAME, WiFi.localIP());

  ArduinoOTA.onStart([]() {
    log("OTA update started");
    pinMode(PIN_STATUS_LED, OUTPUT);
  });

  ArduinoOTA.onEnd([]() {
    logProgressEnd();
    log("OTA upate finished");
    digitalWrite(PIN_STATUS_LED, HIGH);
    pinMode(PIN_STATUS_LED, INPUT);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    logProgress();
    digitalWrite(PIN_STATUS_LED, digitalRead(PIN_STATUS_LED) == LOW ? HIGH : LOW);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    logProgressEnd();
    log("OTA upate error");
    digitalWrite(PIN_STATUS_LED, HIGH);
    pinMode(PIN_STATUS_LED, INPUT);
  });

  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  if (awsIotClient.connected ()) {
     mqttClient->yield(MQTT_YIELD_TIMEOUT_MS);
  } else {
    if (awsIotConnect()){
      awsIotSubscribeToShadowUpdates();
    }
  }
}

void setupWebServer() {
  server.on("/", [](){
    server.send(200, "text/plain", "Hello from Smarkant!");
  });

  server.on("/height", HTTP_GET, [](){
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(I2C_CMD_READ_HEIGHT);
    Wire.endTransmission();
    Wire.requestFrom(I2C_ADDRESS, 2);
    if (waitForI2CBytesAvailable(2)) {
      uint16_t height = Wire.read() + (Wire.read() << 8);
      StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
      JsonObject &json = jsonBuffer.createObject();
      json["value"] = height;
      String responseString;
      json.printTo(responseString);
      server.send(200, "application/json", responseString);
    } else {
      server.send(500);
    }
  });

  server.on("/move", HTTP_PUT, [](){
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
    if (json.success()) {
      if (json.containsKey("height")) {
        tableMoveToHeight((int) json["height"]);
      }
      else if (json.containsKey("position")) {
        tableMoveToPosition((int) json["position"]);
      }
      server.send(204);
    } else {
      server.send(400);
    }
  });

  server.on("/config", HTTP_GET, [](){
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(I2C_CMD_READ_HEIGHT_THRESHOLD);
    Wire.endTransmission();
    Wire.requestFrom(I2C_ADDRESS, 2);
    if (waitForI2CBytesAvailable(2)) {
      uint16_t threshold = Wire.read() + (Wire.read() << 8);
      json["threshold"] = threshold;
    }
    String responseString;
    json.printTo(responseString);
    server.send(200, "application/json", responseString);
  });

  server.on("/config", HTTP_PUT, [](){
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
    if (json.success()) {
      if (json.containsKey("threshold")) {
        int threshold = json["threshold"];
        uint8_t data[] = {I2C_CMD_STORE_THRESHOLD, (uint8_t) (threshold & 0xff), (uint8_t) (threshold >> 8)};
        Wire.beginTransmission(I2C_ADDRESS);
        Wire.write(data, 3);
        Wire.endTransmission();
      }
      server.send(204);
    } else {
      server.send(400);
    }
  });

  server.on("/stop", HTTP_PUT, [](){
    tableStop();
    server.send(204);
  });

  server.on("/up", HTTP_PUT, [](){
    tableMoveUp();
    server.send(204);
  });

  server.on("/down", HTTP_PUT, [](){
    tableMoveDown();
    server.send(204);
  });

  server.on("/positions", HTTP_GET, [](){
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(I2C_CMD_READ_POSITIONS);
    Wire.endTransmission();
    Wire.requestFrom(I2C_ADDRESS, 2 * NUM_POSITION_BUTTONS);
    if (waitForI2CBytesAvailable(2)) {
      StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
      JsonObject &json = jsonBuffer.createObject();
      uint16_t position = Wire.read() + (Wire.read() << 8);
      json["position0"] = position;
      position = Wire.read() + (Wire.read() << 8);
      json["position1"] = position;
      position = Wire.read() + (Wire.read() << 8);
      json["position2"] = position;
      position = Wire.read() + (Wire.read() << 8);
      json["position3"] = position;
      String responseString;
      json.printTo(responseString);
      server.send(200, "application/json", responseString);
    } else {
      server.send(500);
    }
  });

  server.on("/positions", HTTP_PUT, [](){
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
    if (json.success()) {
      char * attrName = "positionX";
      for (int i = 0; i < NUM_POSITION_BUTTONS; ++i) {
        sprintf(attrName, "position%1d", i);
        if (json.containsKey(attrName)) {
          int position = json[attrName];
          uint8_t data[] = {I2C_CMD_STORE_POSITION, i, (uint8_t) (position & 0xff), (uint8_t) (position >> 8)};
          Wire.beginTransmission(I2C_ADDRESS);
          Wire.write(data, 3);
          Wire.endTransmission();
        }
      }
      server.send(204);
    } else {
      server.send(400);
    }
  });

  server.onNotFound([]() {
    server.send(404);
  });

  server.begin();
}

void setupAwsIot() {
  awsIotClient.setAWSDomain(AWS_ENDPOINT);
  awsIotClient.setAWSRegion(AWS_REGION);
  awsIotClient.setAWSKeyID(AWS_ACCESS_KEY_ID);
  awsIotClient.setAWSSecretKey(AWS_SECRET_ACCESS_KEY);
  awsIotClient.setUseSSL(true);
}

bool waitForI2CBytesAvailable(int waitForNumBytess) {
  unsigned long waitForI2CBytesTimeout = millis() + WAIT_FOR_I2C_BYTES_TIMEOUT_MS;
  while (Wire.available() < waitForNumBytess) {
    if (millis() > waitForI2CBytesTimeout) {
      return false;
    }
  }
  return true;
}

bool awsIotConnect () {
  if (mqttClient == NULL) {
    mqttClient = new MQTT::Client<IPStack, Countdown, MQTT_MAX_PACKAGE_SIZE, MQTT_MAX_MESSAGE_HANDLERS>(mqttIpStack);
  } else {
    if (mqttClient->isConnected ()) {
      mqttClient->disconnect ();
    }
    delete mqttClient;
    mqttClient = new MQTT::Client<IPStack, Countdown, MQTT_MAX_PACKAGE_SIZE, MQTT_MAX_MESSAGE_HANDLERS>(mqttIpStack);
  }

  log("Connecting to AWS IOT WebSocket...");
  if (mqttIpStack.connect((char *) AWS_ENDPOINT, WEBSOCKET_PORT) != 1) {
    log("Unable to connect to AWS IOT WebSocket");
    return false;
  } else {
    log("AWS IOT WebSocket connection established");
  }

  log("Connecting to AWS IOT MQTT broker...");
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  char* clientID = new char[23];
  for (int i = 0; i < 22; ++i)
    clientID[i]=(char) random(1, 256);
  data.clientID.cstring = clientID;
  if (mqttClient->connect(data) != 0) {
    delete[] clientID;
    log("Unable to connect to AWS IOT MQTT broker");
    return false;
  }
  delete[] clientID;
  log("AWS IOT MQTT broker connection established");
  return true;
}

void awsIotSubscribeToShadowUpdates() {
  if (mqttClient->subscribe(AWS_DELTA_TOPIC, MQTT::QOS0, awsIotMessageReceived) != 0) {
    log("Unable to subscribe to MQTT topic");
    return;
  }
  log("Subscribed to MQTT topic");
}

void awsIotMessageReceived(MQTT::MessageData& data)
{
  MQTT::Message &message = data.message;
  log("Message %s", message.payload);
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject((const char *) message.payload);
  if (json.success()) {
    if (json.containsKey("state")) {
      JsonObject &state = json["state"];
      if (state.containsKey("move")) {
        const char *move = (const char *) state["move"];
        if (strcmp("stop", move) == 0) {
          tableStop();
        } else if (strcmp("up", move) == 0) {
          tableMoveUp();
        } else if (strcmp("down", move) == 0) {
          tableMoveDown();
        } else if (strcmp("position", move) == 0) {
          tableMoveToPosition((int) state["position"]);
        } else if (strcmp("height", move) == 0) {
          tableMoveToHeight((int) state["height"]);
        }
      }
    }
  }
}

void tableStop() {
  log("Table stop");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(I2C_CMD_MOVE_STOP);
  Wire.endTransmission();
}

void tableMoveUp() {
  log("Table move up");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(I2C_CMD_MOVE_UP);
  Wire.endTransmission();
}

void tableMoveDown() {
  log("Table move down");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(I2C_CMD_MOVE_DOWN);
  Wire.endTransmission();
}

void tableMoveToPosition(int position) {
  if (position >= 1 && position <= NUM_POSITION_BUTTONS) {
    log("Table move to position %d", position);
    uint8_t data[] = {I2C_CMD_MOVE_POSITION, 4 - position};
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(data, 2);
    Wire.endTransmission();
  }
}

void tableMoveToHeight(int height) {
  if (height >= HEIGHT_MIN && height <= HEIGHT_MAX) {
    log("Table move to height %d", height);
    uint8_t data[] = {I2C_CMD_MOVE_HEIGHT, (uint8_t) (height & 0xff), (uint8_t) (height >> 8)};
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(data, 3);
    Wire.endTransmission();
  }
}

/**
 * https://gist.github.com/asheeshr/9004783
 */
void log(const char *str, ...)
{
  const int LOG_BUFFER_LENGTH = 32;
  int i;
  int j;
  int count = 0;
  for(i = 0; str[i] != '\0'; i++) {
    if(str[i]=='%') {
      count++;
    }
  }
  if (count == 0) {
    Serial.println (str);
    return;
  }
  char temp[LOG_BUFFER_LENGTH + 1];
  va_list argv;
  va_start(argv, str);
  for(i = 0, j = 0; str[i] != '\0'; i++)
  {
    if(str[i] == '%')
    {
      temp[j] = '\0';
      Serial.print(temp);
      j = 0;
      temp[0] = '\0';
      switch(str[++i])
      {
        case 'd':
          Serial.print(va_arg(argv, int));
          break;
        case 'l':
          Serial.print(va_arg(argv, long));
          break;
        case 'f':
          Serial.print(va_arg(argv, double));
          break;
        case 'c':
          Serial.print((char)va_arg(argv, int));
          break;
        case 's':
          Serial.print(va_arg(argv, char *));
          break;
        default:
          break;
      };
    }
    else
    {
      temp[j] = str[i];
      j = (j+1) % LOG_BUFFER_LENGTH;
      if(j==0)
      {
        temp[LOG_BUFFER_LENGTH] = '\0';
        Serial.print(temp);
        temp[0] = '\0';
      }
    }
  };
  Serial.println();
}

void logProgress() {
  Serial.print('.');
}

void logProgressEnd() {
  Serial.println();
}
