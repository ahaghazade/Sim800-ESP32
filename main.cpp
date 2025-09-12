#include <Arduino.h>

#include "Sim800_cdrv.h"

//sms initial datas
int CommandAttempts=3;
volatile int inboxCounter = 1;
volatile int CreditCheckCounter = 1;

volatile int BatteryInterval = CheckBatteryInterval - 1 ;
//---------------- Sensors Config -----------
DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;  
float humidityMargine = 60;
//BMP280
// Adafruit_BMP280 bmp;
//pir

//Gas
MQ7 mq7(COPIN,5.0);
float  CORo = 10;
float  fireRo= 10;
float fireMargine = 100.0;
float COMargine = 100.0;
float CO = 0.0;
float fire = 0.0;
float  COCurve[3]  =  {2.3,0.72,-0.34};    //two points are taken from the curve.  
                                                    //with these two points,  a line is formed which is "approximately equivalent" 
                                                    //to  the original curve.
                                                    //data  format:{ x, y, slope}; point1: (lg200, 0.72), point2: (lg10000,  0.15) 
float  fireCurve[3] ={2.3,0.53,-0.44};

float calibration(int pin);
float MQCalibration(int mq_pin);
float MQResistanceCalculation(int rawADC);
int COGetGasPercentage(float rsRoRatio);
int fireGetGasPercentage(float rsRoRatio);
int  MQGetPercentage(float rsRoRatio, float *pcurve);
float MQRead(int mq_pin);
void ReadDHT();
void CheckCNY70();
// void BMP280Values();
//---------------- Config Time -----------------
ESP32Time rtc(0);  // offset in seconds GMT+1
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3.5 * 60 * 60;
const int   daylightOffset_sec = 0;
int WeekDay = 0; //0 Sun - 6 Sat
int TimeSec = 0;
int TimeH = 0; //0-23
int TimeMin = 0;
int year  = 2025;
int month = 1;
int day = 1;

int TryGetNTP = 0;
#define NTPthr 3
JsonDocument WeekNameNum;
JsonDocument RelayTimes;
JsonDocument SocketClients;

bool GoInTimeFrame = false;


bool LogTimeShow = false;

JsonDocument        phoneNumbers;   //FROM GW CONFIG  //{"PHN1": "09127176496"}}
JsonDocument        adminPhoneNumbers; 
JsonDocument        systemConfig;
JsonDocument        SensorsValueJson;
JsonDocument        LoginPass;
JsonDocument        SystemLog;
JsonDocument        RebootCount;
JsonDocument        WifiConfig;
JsonDocument        TimeJsonDoc;

// JsonArray logArray = SystemLog.createNestedArray("logs");

const char* SystemStatusPath    = "/SystemStatus.json";
const char* phoneNumsPath       = "/PhoneNums.json";
const char* adminPhoneNumsPath  = "/adminPhoneNums.json";
const char* RelayClockPath      = "/Clock.json";
const char* LoginPassPath       = "/LoginPass.json";
const char* LastLogsPath        = "/LastLogs.json";
const char* RebootCounterPath   = "/BootCount.json";
const char* WifiConfigPath      = "/WifiConfig.json";
const char* TimeJsonDocPath      = "/TimeJsonDoc.json";
// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "NodeID";

//Variables to save values from HTML form
String ssidSPIFF;
String passSPIFF;
String MQQTipSPIFF;
String DeviceNameSPIFF;

// File paths to save input values permanently
// const char* ssidPath = "/ssid.txt";
// const char* passPath = "/pass.txt";
// const char* ipPath = "/ip.txt";
// const char* DeviceNamepath = "/DeviceName.txt";

// const String SSIDWifi = "";
// const String PASSWifi = "";
// const String SSIDWifi = "Myphone";
// const String PASSWifi = "amir1996";
// const String SSIDWifi = "Sharif-WiFi";
// const String PASSWifi = "";

const char* correctUsername = "admin";
bool isAuthenticated = true; // Flag to check if the user is logged in

//webserver
#define PORT_1 80
AsyncWebServer server(PORT_1);
const char* PARAM_MESSAGE = "message";
IPAddress localIP;

//websocket
AsyncWebSocket ws("/ws");

String WSmessage = "";

void notifyClients(String text) {
  // bool ClientExist = false;
  // for (JsonPair kv : SocketClients.as<JsonObject>()) {
  //       bool value = kv.value().as<bool>(); // Get the value (true/false)
  //       // Check if the client is connected
  //       if (value)
  //         ClientExist = true;
  // }
  // if (ClientExist)
    ws.textAll(text);
  // else
    // Serial.println("---- no client availble! ----");
}

void addLog(String newLog) {
    while (SystemLog["logs"].size() > systemConfig["LogCount"].as<int>()) {
        SystemLog["logs"].remove(0); // Remove the oldest log if we exceed maxLogs
    }
    SystemLog["logs"].add(newLog); // Add the new log to the end
}

void saveJson(const char* filename, JsonDocument& doc) {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(doc, file);
  file.close();
}

void loadJson(const char* filename, JsonDocument& doc) {
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  deserializeJson(doc, file);
  file.close();
}

JsonDocument convertToOriginalFormat(JsonDocument& phoneNumbersDoc, JsonDocument& adminPhoneNumbersDoc) {
  // Create a new JSON document to hold the final format
  int SavedPhoneCount = 0;
  JsonDocument originalDoc;

  // Create a JSON array to hold phone numbers with isAdmin values
  JsonArray phoneNumbersArray = originalDoc.createNestedArray("PhoneNumbers");

  // Process adminPhoneNumbers and add them to the array
  for (JsonPair kv : adminPhoneNumbersDoc.as<JsonObject>()) {
    SavedPhoneCount++;
    JsonObject entry = phoneNumbersArray.createNestedObject();
    entry["Number" + String(SavedPhoneCount)] = kv.value().as<const char*>();
    entry["isAdmin"] = true;  // All numbers in adminPhoneNumbers are admins
  }

  // Process phoneNumbers and add them to the array
  for (JsonPair kv : phoneNumbersDoc.as<JsonObject>()) {
    SavedPhoneCount++;
    JsonObject entry = phoneNumbersArray.createNestedObject();
    entry["Number" + String(SavedPhoneCount)] = kv.value().as<const char*>();
    entry["isAdmin"] = false;  // All numbers in phoneNumbers are not admins
  }

  for(int i = SavedPhoneCount+1; i <= 4; i++) // add no nimbers to reach Number4 in list
  {
    JsonObject entry = phoneNumbersArray.createNestedObject();
    entry["Number" + String(i)] = "";
    entry["isAdmin"] = false;
  }

  serializeJsonPretty(originalDoc, Serial);
  return originalDoc;
}

//websocketfunc
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      SocketClients[client->id()] = true;
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      SocketClients[client->id()] = false;
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}
// Initialize WiFi
bool initWiFi() {  

  WiFi.mode(WIFI_STA);
  if(passSPIFF == "")
    WiFi.begin(ssidSPIFF.c_str());
  else
    WiFi.begin(ssidSPIFF.c_str(), passSPIFF.c_str());
  // WiFi.begin(SSIDWifi.c_str(), PASSWifi.c_str());
  Serial.print("Connecting to WiFi ");
  Serial.print(ssidSPIFF);

  unsigned long currentMillis = millis();
  previousMillisWifi = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    //esp_task_wdt_reset();
    digitalWrite(LEDPIN, !digitalRead(LEDPIN));
    // if(systemConfig["RgbRing"].as<int>())
    BlinkColorSmooth(CRGB::Blue, NUM_LEDS, 1, 20, leds);
    currentMillis = millis();
    if (currentMillis - previousMillisWifi >= intervalWifi) {
      Serial.println("Failed to connect.");
      return false;
    }
    Serial.print(".");
    delay(500);
  }
  digitalWrite(LEDPIN,LOW);

  
  // if(systemConfig["RgbRing"].as<int>())
  if(systemConfig["SystemStatus"].as<bool>())
    LinearColorFill((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
  else
    LinearColorFill((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);


  //esp_task_wdt_reset();
  Serial.println();
  Serial.println(WiFi.localIP());
  String ipString = WiFi.localIP().toString();
  Serial.println(ipString);
  WifiConnected = true;
  return true;
}
// Initialize SPIFFS
void initSPIFFS() {
  int Try = 0;
  bool MountSuccess = false;
  while (Try < INITTryes) 
  {
    if(!SPIFFS.begin(true)) 
    {
      Serial.println("An error has occurred while mounting SPIFFS");
      Try++;
    }
    else
    {
      MountSuccess = true;
      break;
    }
    sleep(1);
  }
  if(MountSuccess)
  {
    Serial.println("SPIFFS mounted successfully");
  }
  else
    ESP.restart();
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

bool GetTime(String TimeURL)
{
  Serial.println("Getting time...");
  HTTPClient http;
  http.begin(TimeURL); // Specify the URL
  int httpResponseCode = http.GET(); // Make the request

  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
    String response = http.getString(); // Get the response payload
    Serial.println("Response:");
    Serial.println(response);
    http.end(); 

    JsonDocument ReadedTimeJsonDoc;
    DeserializationError err = deserializeJson(ReadedTimeJsonDoc, response);
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
        return 0;
    }
    //api.keybit
    /*if(ReadedTimeJsonDoc.containsKey("season") && ReadedTimeJsonDoc.containsKey("time24") && ReadedTimeJsonDoc.containsKey("date"))
    {
      Serial.println("\n\n>>>Extracted Date and Time:");

      TimeH   = ReadedTimeJsonDoc["time24"]["hour"]["en"].as<int>();
      TimeMin = ReadedTimeJsonDoc["time24"]["minute"]["en"].as<int>();
      TimeSec = ReadedTimeJsonDoc["time24"]["second"]["en"].as<int>();

      String datefull = ReadedTimeJsonDoc["date"]["other"]["gregorian"]["iso"]["en"].as<String>(); 
      int year  = datefull.substring(0, 4).toInt();
      int month = datefull.substring(5, 7).toInt();
      int day   = datefull.substring(8, 10).toInt();
      int session =  ReadedTimeJsonDoc["season"]["number"]["en"].as<int>();

      if(session < 6){
        if(TimeH < 1 ) {TimeH = 23;}
        else {TimeH --;}
      }*/

      //timeapi
      if(ReadedTimeJsonDoc.containsKey("hour")  && ReadedTimeJsonDoc.containsKey("minute")  && ReadedTimeJsonDoc.containsKey("seconds") 
      && ReadedTimeJsonDoc.containsKey("year")  && ReadedTimeJsonDoc.containsKey("month") && ReadedTimeJsonDoc.containsKey("day")) {

        Serial.println("\n\n>>>Extracted Date and Time:");

        TimeH   = ReadedTimeJsonDoc["hour"].as<int>();
        TimeMin = ReadedTimeJsonDoc["minute"].as<int>();
        TimeSec = ReadedTimeJsonDoc["second"].as<int>();

        year  = ReadedTimeJsonDoc["year"].as<int>();
        month = ReadedTimeJsonDoc["month"].as<int>();
        day   = ReadedTimeJsonDoc["day"].as<int>();

      } else {

        Serial.println("Failed To obtain Time");
        return 0;
      }
      

      TimeJsonDoc["hour"] = TimeH;
      TimeJsonDoc["minute"] = TimeMin;
      TimeJsonDoc["seconds"] = TimeSec;
      TimeJsonDoc["day"] = day;
      TimeJsonDoc["year"] = year;
      TimeJsonDoc["month"] = month;
      saveJson(TimeJsonDocPath, TimeJsonDoc);

      Serial.printf("Time: %d:%d:%d\n", TimeH, TimeMin, TimeSec);
      Serial.printf("Date: %d-%d-%d\n", year,month,day);
      Serial.printf("dayOfWeek: %s\n", ReadedTimeJsonDoc["date"]["weekday"]["number"]["en"].as<String>());
    

      Serial.println("----RTC----");
      rtc.setTime(TimeSec, TimeMin, TimeH, day, month, year); //second,min,hour,day,month,year
      Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));

      return 1;

    } else {

    Serial.printf("Error in HTTP request: %d\n", httpResponseCode);
    return 0;
    }
}

void StartServers()
{
  initWebSocket();
  //#################################################

  // // Web Server Root URL
  server.on("/lamp", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      if (doc.containsKey("Lamp")) {

        int lampState = doc["Lamp"];
        // Control relay
        digitalWrite(RELAYPIN, lampState);
        // Save to systemConfig
        systemConfig["LampStatus"] = lampState;
        saveJson(SystemStatusPath, systemConfig);

        // Prepare response
        DynamicJsonDocument resp(128);
        resp["status"] = "OK";
        resp["Lamp"] = lampState;
        String jsonResponse;
        serializeJson(resp, jsonResponse);

        request->send(200, "application/json", jsonResponse);
      } else {
        request->send(400, "application/json", "{\"error\":\"Missing Lamp field\"}");
      }
    });


  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/wifimanager.html", "text/html");
  });
  
  // server.serveStatic("/config", SPIFFS, "/config");
  
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {

    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST ssid value
        if (p->name() == PARAM_INPUT_1) 
        {
          ssidSPIFF = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(ssidSPIFF);
          // Write file to save value
          WifiConfig["ssid"] = ssidSPIFF;
          saveJson(WifiConfigPath, WifiConfig);
          // writeFile(SPIFFS, ssidPath, ssidSPIFF.c_str());
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INPUT_2) {
          passSPIFF = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(passSPIFF);
          WifiConfig["pass"] = passSPIFF;
          saveJson(WifiConfigPath, WifiConfig);
          // Write file to save value
          // writeFile(SPIFFS, passPath, passSPIFF.c_str());
        }
        // HTTP POST ip value
        // if (p->name() == PARAM_INPUT_3) {
        //   MQQTipSPIFF = p->value().c_str();
        //   Serial.print("IP Address set to: ");
        //   Serial.println(MQQTipSPIFF);
        //   // Write file to save value
        //   writeFile(SPIFFS, ipPath, MQQTipSPIFF.c_str());
        // }
        // // HTTP POST ServerName value
        // if (p->name() == PARAM_INPUT_4) {
        //   DeviceNameSPIFF = p->value().c_str();
        //   Serial.print("IP DeviceName set to: ");
        //   Serial.println(DeviceNameSPIFF);
        //   // Write file to save value
        //   writeFile(SPIFFS, DeviceNamepath, DeviceNameSPIFF.c_str());
        // }
      }
    }
    WiFi.disconnect();
    delay(1000);
    if(ssidSPIFF != "")
    {
      if(initWiFi()) 
      {
        Serial.println("WiFi Connected");
        // Init and get the time
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        while(!GetTime(timeApiUrl))
        {
          //esp_task_wdt_reset();
          Serial.println("Failed to obtain time");
          //esp_task_wdt_reset();
          sleep(1);
          TryGetNTP++;
          if(TryGetNTP > NTPthr)
          {
            break;
          }
        }
        // printLocalTime();
      } 
    }
    if(!WifiConnected) 
    {
        // Connect to Wi-Fi network with SSID and password
        Serial.println("Setting AP (Access Point)");
        // NULL sets an open Access Point
        WiFi.softAP("Didomak-WIFI-MANAGER", NULL);

        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);   
        LinearColorFill(CRGB(76,0,153), NUM_LEDS, leds);
    }

    // request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + MQQTipSPIFF);
    request->send(SPIFFS, "/Page1.html", "text/html", false);
    // delay(3000);
    // ESP.restart();
    
  });
  
  server.on("/wifi.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/wifi.css", "text/css");
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    if(isAuthenticated)
      request->send(SPIFFS, "/Page1.html", "text/html", false);
    else
      request->send(SPIFFS, "/Denied.html", "text/html", false);
  });
  
  server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request){
    if(isAuthenticated)
      request->send(SPIFFS, "/Page2.html", "text/html", false);
    else
      request->send(SPIFFS, "/Denied.html", "text/html", false);
  });

  server.on("/clock", HTTP_GET, [](AsyncWebServerRequest *request){
    if(isAuthenticated)
      request->send(SPIFFS, "/date.html", "text/html", false);
    else
      request->send(SPIFFS, "/Denied.html", "text/html", false);
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    if(isAuthenticated)
      request->send(SPIFFS, "/Log.html", "text/html", false);
    else
      request->send(SPIFFS, "/Denied.html", "text/html", false);
  });

  server.on("/styles1.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/styles1.css", "text/css");
  });

  server.on("/styles2.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/styles2.css", "text/css");
  });

  server.on("/date.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/date.css", "text/css");
  });

  server.on("/log.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/log.css", "text/css");
  });
  
  server.on("/iot.jpg", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/iot.jpg", "image/png");
  });

  server.on("/alarm.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/alarm.png", "image/png");
  });
 
  server.on("/temp.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/temp.png", "image/png");
  });
  server.on("/cloud.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/cloud.png", "image/png");
  });
  server.on("/humidity.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/humidity.png", "image/png");
  });
  server.on("/fire.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/fire.png", "image/png");
  });
  server.on("/person.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/person.png", "image/png");
  });
  server.on("/lamp.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/lamp.png", "image/png");
  });
  server.on("/alarm.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/alarm.png", "image/png");
  });
  server.on("/battery.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/alarm.png", "image/png");
  });
  server.on("/sim.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/sim.png", "image/png");
  });
  server.on("/home.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/home.png", "image/png");
  });
  server.on("/setting.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/setting.png", "image/png");
  });
  server.on("/log.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/log.png", "image/png");
  });
  server.on("/logout.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/logout.png", "image/png");
  });
  server.on("/clock.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/clock.png", "image/png");
  });
  // server.on("/speaker.png", HTTP_GET, [](AsyncWebServerRequest *request){
  // request->send(SPIFFS, "/speaker.png", "image/png");
  // });
  server.on("/radar.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/radar.png", "image/png");
  });
  server.on("/phone.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/phone.png", "image/png");
  });
  server.on("/key.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/key.png", "image/png");
  });
  server.on("/palette.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/palette.png", "image/png");
  });

  server.on("/Didomak.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/Didomak.png", "image/png");
  });

  server.on("/alarmhome.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/alarmhome.png", "image/png");
  });
  
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(204);
  });

  server.on("/script1.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/script1.js", "application/javascript");
  });

  server.on("/script2.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/script2.js", "application/javascript");
  });
  
  server.on("/log.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/log.js", "application/javascript");
  });

  server.on("/date.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/date.js", "application/javascript");
  });

  server.on("/Didomak192.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/Didomak192.png", "image/png");
  });

  server.on("/Didomak512.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/Didomak512.png", "image/png");
  });

  server.on("/GetPhones", HTTP_GET, [](AsyncWebServerRequest *request){
    loadJson(phoneNumsPath, phoneNumbers);
    loadJson(adminPhoneNumsPath, adminPhoneNumbers);

    Serial.println("GetPhones Req:");
    String output;
    serializeJson(convertToOriginalFormat(phoneNumbers,adminPhoneNumbers) , output); // Serialize the JSON document to a String
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/GetClock", HTTP_GET, [](AsyncWebServerRequest *request){
    loadJson(RelayClockPath, RelayTimes);
    String output;
    serializeJson(RelayTimes, output); // Serialize the JSON document to a String
    Serial.println("GetClock Req - Last RelayTimes:");
    Serial.println(output);    
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/LogNum", HTTP_GET, [](AsyncWebServerRequest *request){
    loadJson(SystemStatusPath, systemConfig);
    Serial.println("LogNum Req:");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
    JsonDocument LastLogNum;
    LastLogNum["LogNum"] = systemConfig["LogCount"].as<int>();
    String output;
    serializeJson(LastLogNum, output); // Serialize the JSON document to a String
    Serial.println("LogNum Req - Last LogNum:");
    Serial.println(output);    
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/SystemState", HTTP_GET, [](AsyncWebServerRequest *request){
    loadJson(SystemStatusPath, systemConfig);
    Serial.println("systemConfig req:");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
    String output;
    serializeJson(systemConfig, output); // Serialize the JSON document to a String
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/auth", HTTP_POST, [](AsyncWebServerRequest *request){
    loadJson(LoginPassPath, LoginPass);
    Serial.println("LoginPass:");
    serializeJsonPretty(LoginPass, Serial);
    Serial.println();
    String username;
    String password;

    // Check if the request contains the required parameters
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
        username = request->getParam("username", true)->value();
        password = request->getParam("password", true)->value();

        // Validate the credentials
        if (username == correctUsername && password == LoginPass["Pass"].as<String>()) 
        {
            isAuthenticated = true;
            request->send(200, "text/plain", "Login Successful! Redirecting...");
        } else {
            isAuthenticated = false;
            request->send(401, "text/plain", "Invalid username or password");
        }
    } else {
        request->send(400, "text/plain", "Missing username or password");
    }
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Login.html", "text/html", false);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    isAuthenticated = false; // Clear the session flag
    request->send(200, "text/plain", "Logged out successfully.");
  });

  server.on("/getlogs", HTTP_GET, [](AsyncWebServerRequest *request) {
        loadJson(LastLogsPath, RebootCount);
        String jsonResponse;
        serializeJson(SystemLog, jsonResponse); // Serialize JSON document to a String
        request->send(200, "application/json", jsonResponse); // Send JSON response
  });

  server.on("/boot", HTTP_GET, [](AsyncWebServerRequest *request) {
        loadJson(RebootCounterPath, RebootCount);
        String jsonResponse;
        serializeJson(RebootCount, jsonResponse); // Serialize JSON document to a String
        request->send(200, "application/json", jsonResponse); // Send JSON response
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/Page1.html", "text/html", false);
    delay(1000);
    ESP.restart();
  });
  
  server.begin();


  while(!MDNS.begin("didomak"))
  {
     Serial.println("Starting mDNS...");
     delay(1000);
  }
  
  Serial.println("MDNS started");
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  Serial.println("HTTP server started");
  // client.setServer(MQQTipSPIFF.c_str(), mqttPort);
  // client.setCallback(MQTTcallback);
}

void checkAndControlLED(int currentDay, int currentTimeH, int currentTimeM) {
  // Access the JSON structure

  bool isInTimeFrame = false;

  // Iterate through the RelayTime array
  JsonArray relayTimeArray = RelayTimes["RelayTime"].as<JsonArray>();

  for (JsonObject relay : relayTimeArray) {
    String onTime = relay["onTime"];
    String offTime = relay["offTime"];
    JsonArray days = relay["days"];

    int colonIndexOn  = onTime.indexOf(':');
    int colonIndexOff = offTime.indexOf(':');
    
    int onTimeH  = onTime.substring(0, colonIndexOn).toInt();
    int onTimeM  = onTime.substring(colonIndexOn + 1).toInt();
    int offTimeH = offTime.substring(0, colonIndexOff).toInt();
    int offTimeM = offTime.substring(colonIndexOff + 1).toInt();

    // For each day, check if the current day matches
    for (String day : days) {
      // if(LogTimeShow)
      //   Serial.printf("%s - %d:%d -> %d:%d\n", day.c_str(), onTimeH,onTimeM , offTimeH,offTimeM);

      if (WeekNameNum[day] == currentDay) 
      {
        if (currentTimeH <= offTimeH && currentTimeH >= onTimeH) 
        {
          if(currentTimeH == offTimeH || currentTimeH == onTimeH)
          {
            if(currentTimeM < offTimeM && currentTimeM >= onTimeM)
            {
              isInTimeFrame = true;
              break;
            }
          }
          else
          {
            isInTimeFrame = true;
            break;
          }
        }
      }
    }
    if (isInTimeFrame) break;
  }

  if(isInTimeFrame && !GoInTimeFrame)
  {
    Serial.println("Going to frame time...");
    GoInTimeFrame = true;
    systemConfig["LampStatus"] = 1;
    String SystemConf;
    Serial.println("[Timer check on] systemConfig: ");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
    saveJson(SystemStatusPath,systemConfig);
    String readings;
    serializeJson(systemConfig, readings);
    notifyClients(readings);
  }
  // Control the LED based on whether we are in the time frame
  if (isInTimeFrame && !digitalRead(RELAYPIN) && systemConfig["LampStatus"]) {
    Serial.println("Turning LED ON");
    digitalWrite(RELAYPIN, HIGH);  // Turn the LED on
  } 
  if (!isInTimeFrame)
  {
    if(GoInTimeFrame)
    {
      GoInTimeFrame = false;
      if(digitalRead(RELAYPIN))
      {
        Serial.println("Out of time frame. Turning LED off");
        digitalWrite(RELAYPIN, LOW);  // Turn the LED off
        systemConfig["LampStatus"] = 0;
        String SystemConf;
        Serial.println("[Timer check off] systemConfig: ");
        serializeJsonPretty(systemConfig, Serial);
        Serial.println();
        saveJson(SystemStatusPath,systemConfig);
        String readings;
        serializeJson(systemConfig, readings);
        notifyClients(readings);
      }
    }
  }
}

void printLocalTime(){
  struct tm timeinfo;

  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    sleep(1);
    TryGetNTP++;
    if(TryGetNTP > NTPthr)
      break;
  }

  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();

  WeekDay = timeinfo.tm_wday;
  TimeH   = timeinfo.tm_hour;
  TimeSec = timeinfo.tm_sec;
  TimeMin = timeinfo.tm_min;
  Serial.printf("%d:%d:%d - %d\n", TimeH,TimeMin,TimeSec , WeekDay);
  // rtc.setTime(TimeSec, TimeMin, TimeH, timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year);  //sec,min,houre ,day,month,year
  rtc.setTimeStruct(timeinfo); 
  Serial.println("----RTC----");
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));   // (String) returns time with specified format 

}

void UpdateTime()
{
  WeekDay = rtc.getDayofWeek();
  TimeH   = rtc.getHour(true);
  TimeSec = rtc.getSecond();
  TimeMin = rtc.getMinute();
  
  TimeJsonDoc["day"] = rtc.getDay();
  TimeJsonDoc["month"] = rtc.getMonth();
  TimeJsonDoc["year"] = rtc.getYear();
  
  // TimeJsonDoc["hour"] = TimeH;
  TimeJsonDoc["minute"] = TimeMin;
  TimeJsonDoc["seconds"] = TimeSec;

  if(TimeJsonDoc["month"].as<int>() < 6)
    TimeJsonDoc["hour"] = TimeH--;
  else
    TimeJsonDoc["hour"] = TimeH;
  
  saveJson(TimeJsonDocPath, TimeJsonDoc);

  if(LogTimeShow) {
    Serial.printf("Time: %d:%d:%d - %d\n", TimeH,TimeMin,TimeSec , WeekDay);
    Serial.printf("Date: %d-%d-%d\n", TimeJsonDoc["year"].as<int>(),TimeJsonDoc["month"].as<int>() , TimeJsonDoc["day"].as<int>());
  }
  checkAndControlLED(WeekDay, TimeH, TimeMin);
}

void systemConfigInitial();

//sim functions
void parsePhoneNumbers(JsonDocument& doc);
bool checkPhonenumber(String msg);
void creatPhonenumber(String newphone);
void creatAdminPhonenumber(String newphone);
String sendReqtoSim(String Command, String DesiredRes);
int checkCredit();
void updateSerial();
String currentUser = "";
bool checkForDeliveryReport(); //new
bool waitForCallResponse(); //new
String parsePhoneNumber(String cmgrResponse) ;
int parseIndex(String line);
void make_call(String phonenumber_call); //new
void GSMInit();
void RestartGSM();
void sendMessage(String sms, String PhNum);
void configTextMode();
void processSMS(String sms);
String normalizePhone(String phoneNumber);
bool checkPhonenumber(String msg);
String findCommand(String receivedMessage);
void systemAct(const String& receivedMessage);
void ipAct(const String& receivedMessage);
void lampAct(const String& receivedMessage);
void alarmAct(const String& receivedMessage);
void fireAct(const String& receivedMessage);
void carbonSensorAct(const String& receivedMessage);
void tempSensorAct(const String& receivedMessage);
void HumiditySensorAct(const String& receivedMessage);
void sendSms_act(String sms);
bool checkPIR();
// bool checkMW();
void CheckBattery();
void sendToAll(String payload);
String convertJsonToString(JsonDocument& doc);
void alarmSpeaker(void *param);
void alarmSound();
String phoneNumberToUCS2(const String& phoneNumber);
void initPersianMode();
void initReceiveMode();
String getLast36Chars(const String& input);
void MQ2Read(int MQ2Pin);
void MQ7Read();
void SendLog(String LogMessage);
void Buzzer();
void CheckMotion();
void CheckSensors(void *param);
void RGBTask(void *param);
void HardRestartSim();
void TimeThread(void *param);
void ResetFactory();
void AlarmSpeaker(void *param);
void AlarmSound();
void WifiReconnect();

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2D2, TX2D2);
  Serial1.begin(115200, SERIAL_8N1, RX1D1, TX1D1);
  Serial2.flush();
  // esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  //esp_task_wdt_add //add current thread to WDT watch
  //esp_task_wdt_reset();

  // Serial2.setTimeout(200);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.println("Starting....");
  delay(500);
  InitialABDThresh();
  Serial.println("Config Thresh done!");
  // delay(500);
  // ConfigABDParams(LD2420_MaxDisConf,1);
  delay(500);
  ConfigABDParams(LD2420_DelayTimeConf,2);
  //---------IO-------------
  pinMode(RELAYPIN,OUTPUT);
  pinMode(LEDPIN, OUTPUT);
  ledcAttachPin(BUZZERPin, TONE_PWM_CHANNEL);
  ledcWrite(TONE_PWM_CHANNEL, 0);
  pinMode(SpekaerPin, OUTPUT); 
  digitalWrite(RELAYPIN, LOW);
  pinMode(COPIN, INPUT);
  pinMode(FIREPIN, INPUT);
  pinMode(pirPin, INPUT_PULLDOWN);
  // pinMode(LDOUT, INPUT);
  pinMode(ACIn, INPUT);
  pinMode(Simpower,OUTPUT);
  digitalWrite(Simpower,1);
  pinMode(ResetFact,INPUT_PULLUP);
  //=========Sensors===============
  dht.begin();
  // bool status = bmp.begin(0x76);  
  // if (!status) {
  //   Serial.println("Could not find a valid BME280 sensor, check wiring!");
  //   while (1);
  // }
  // Serial.println("-- Default BMP280 Test --");
  // bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
  //                 Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
  //                 Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
  //                 Adafruit_BMP280::FILTER_X16,      /* Filtering. */
  //                 Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  //--------initial weeks json------
  WeekNameNum["Sun"] = 0;
  WeekNameNum["Mon"] = 1;
  WeekNameNum["Tue"] = 2;
  WeekNameNum["Wed"] = 3;
  WeekNameNum["Thu"] = 4;
  WeekNameNum["Fri"] = 5;
  WeekNameNum["Sat"] = 6;

  initSPIFFS();
  //system phone nums and confs
  loadJson(SystemStatusPath, systemConfig);
  loadJson(RelayClockPath, RelayTimes);
  loadJson(phoneNumsPath, phoneNumbers);
  loadJson(adminPhoneNumsPath, adminPhoneNumbers);
  loadJson(LoginPassPath, LoginPass);
  SystemLog["logs"] = JsonArray();
  loadJson(LastLogsPath, SystemLog);
  loadJson(RebootCounterPath, RebootCount);
  loadJson(WifiConfigPath, WifiConfig);
  loadJson(TimeJsonDocPath, TimeJsonDoc);

  
  if(!TimeJsonDoc.containsKey("day"))
  {
    TimeJsonDoc["hour"] = 18;
    TimeJsonDoc["minute"] = 50;
    TimeJsonDoc["seconds"] = 0;
    TimeJsonDoc["day"] = 24;
    TimeJsonDoc["year"] = 2025;
    TimeJsonDoc["month"] = 6;
    saveJson(TimeJsonDocPath, TimeJsonDoc);
  }

  if(!WifiConfig.containsKey("ssid"))
  {
    WifiConfig["ssid"] = "";
    saveJson(WifiConfigPath, WifiConfig);
  }
  if(!WifiConfig.containsKey("pass"))
  {
    WifiConfig["pass"] = "";
    saveJson(WifiConfigPath, WifiConfig);
  }

  ssidSPIFF = WifiConfig["ssid"].as<String>();
  passSPIFF = WifiConfig["pass"].as<String>();

  if(!LoginPass.containsKey("Pass"))
  {
    LoginPass["Pass"] = "pass";
    saveJson(LoginPassPath, LoginPass);
  }
  else if(LoginPass["Pass"] == "")
  {
    LoginPass["Pass"] = "pass";
    saveJson(LoginPassPath, LoginPass);
  }

  if(!RebootCount.containsKey("Count"))
  {
    RebootCount["Count"] = 1;
    saveJson(RebootCounterPath, RebootCount);
  }
  else
  {
    RebootCount["Count"] = RebootCount["Count"].as<int>() + 1;
    saveJson(RebootCounterPath, RebootCount);
  }

  Serial.println("=====Saved Wifi Config=====");
  serializeJsonPretty(WifiConfig, Serial);
  Serial.println();

  Serial.println("=====Saved Phone Numbers=====");
  serializeJsonPretty(phoneNumbers, Serial);
  Serial.println();

  Serial.println("=====Saved admin Phone Numbers=====");
  serializeJsonPretty(adminPhoneNumbers, Serial);
  Serial.println();

  Serial.println("=====Saved systemConfig=====");
  serializeJsonPretty(systemConfig, Serial);
  Serial.println();
  systemConfigInitial();

  ConfigABDParams(LD2420_MaxDisConf, systemConfig["RadarDistance"].as<int>()); 

  digitalWrite(RELAYPIN, systemConfig["LampStatus"].as<int>());

  Serial.println("=====Saved RelayTimes=====");
  serializeJsonPretty(RelayTimes, Serial);
  Serial.println();

  Serial.println("=====Saved Login Pass=====");
  serializeJsonPretty(LoginPass, Serial);
  Serial.println();

  Serial.println("=====Saved Last Logs=====");
  serializeJsonPretty(SystemLog, Serial);
  Serial.println();

  lastAlarmTime  = millis() - systemConfig["AlarmDuration"].as<int>() * 1000;

  if(systemConfig["SystemStatus"].as<bool>())
    LinearColorFill((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
  else
    LinearColorFill((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);
  //esp_task_wdt_reset();
  //----------RGB------------------
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  FastLED.setBrightness(systemConfig["RgbRing"].as<int>() * FullBrightness / 100);
  FullColor(CRGB::Black, NUM_LEDS, leds);
  Fancy(CRGB::Green, NUM_LEDS, leds);
  //esp_task_wdt_reset();
  //#################################################
  bool GetTimeFlag = false;

  if(ssidSPIFF != "")
  {
    if(initWiFi()) {
      Serial.println("WiFi Connected");
      // Init and get the time
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      bool GetTimeFlag = false;
      TryGetNTP = 0;
      while(TryGetNTP < NTPthr)
      {
        GetTimeFlag = GetTime(timeApiUrl);
        if(GetTimeFlag)
          break;

        //esp_task_wdt_reset();
        Serial.println("Failed to obtain time");
        //esp_task_wdt_reset();
        sleep(1);
        TryGetNTP++;
        if(TryGetNTP > NTPthr)
        {
          break;
        }
      }
      // TimeH   = 18;
      // TimeMin = 17;
      // TimeSec = 0;

      // int year  = 2025;
      // int month = 6;
      // int day   = 18;
    } 
  }
  if(!WifiConnected) 
  {

    WiFi.mode(WIFI_MODE_AP);
    Serial.println("----------7");
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("Didomak-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);   
    LinearColorFill(CRGB(76,0,153), NUM_LEDS, leds);
  }
  if(!GetTimeFlag)
  {
    Serial.println("Load time from last connection");
    TimeH   = TimeJsonDoc["hour"].as<int>();
    TimeMin = TimeJsonDoc["minute"].as<int>();
    TimeSec = TimeJsonDoc["seconds"].as<int>();

    int year  = TimeJsonDoc["year"].as<int>();
    int month = TimeJsonDoc["month"].as<int>();
    int day   = TimeJsonDoc["day"].as<int>();
    rtc.setTime(TimeSec, TimeMin, TimeH, day, month, year); //second,min,hour,day,month,year
  }
  // printLocalTime();

  Serial.println("----------8");
  digitalWrite(LEDPIN, LOW);
  StartServers();

  SensorsValueJson["Temperature"] = -1;
  SensorsValueJson["Pressure"]    = -1;
  SensorsValueJson["Humidity"]    = -1;
  SensorsValueJson["Co"]          = -1;
  SensorsValueJson["FireSensor"]  = -1;
  SensorsValueJson["Credit"]      = -1;
  SensorsValueJson["Power"]       = "-";

  lastAlarmTime = millis() - alarmInterval;
  lastSensorCheckTime = millis() - SensorsInterval;
  lastSensorDoubleCheck = millis();

  FastLED.setBrightness(systemConfig["RgbRing"].as<int>() * FullBrightness / 100);
  FastLED.show();

  
  //esp_task_wdt_reset();
  if(SIMCARDEN)
  {
    //check sim state(it should response to at command)
    String ResAT = "";
    int GSMStateCheck = 0;
    do
    {
      GSMStateCheck++;
      //esp_task_wdt_reset();
      ResAT = sendReqtoSim(AT, ATOK);
      if(ResAT == "Failed")
        RestartGSM();
    }while((ResAT == "Failed") && GSMStateCheck < GSMStateThr);
    if(ResAT == "Failed") //restart esp
      ESP.restart();

    GSMInit();
    if (englishFlag == false)
      initPersianMode();
    //esp_task_wdt_reset();
    configTextMode();

    int CreditValue = -1;
    // CreditValue = checkCredit();
    Serial.println("Credit. balance is: ");
    Serial.println(CreditValue);
    systemConfig["Credit"] = CreditValue;
    SensorsValueJson["Credit"] = CreditValue;
    saveJson(SystemStatusPath,systemConfig);
    String message;
    serializeJson(systemConfig, message);
    notifyClients(message);
    CreditCheckCounter = 0;

    sendToAll(SatartupMSG);
  }
  xTaskCreatePinnedToCore(CheckSensors, "Sensors_Task", 4096*8, NULL, 2, &SensorsTaskHandle, 1);
  xTaskCreatePinnedToCore(RGBTask, "RGB_Task", 4096, NULL, 1, &RGBTaskHandler, 0);
  // xTaskCreatePinnedToCore(AlarmSpeaker, "Spekaer_Task", 4096, NULL, 1, &SpekaerTaskHandler, 0);
  xTaskCreatePinnedToCore(TimeThread, "Time_Task", 4096, NULL, 3, &TimeTaskHandler, 1);

}

void loop() {
  //esp_task_wdt_reset();
  //check reset botton
  if(!digitalRead(ResetFact))
  {
    unsigned long ResetTime = millis();
    while((millis() - ResetTime <= ResetFactTime) && !digitalRead(ResetFact))
    {
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    if(millis() - ResetTime > ResetFactTime)
      ResetFactory();
  }

  //check AC/Battery
  if(!digitalRead(ACIn) && systemConfig["AcStatus"].as<bool>())
  {
    //AC was on and now it turnes off
    systemConfig["AcStatus"] = false;
    saveJson(SystemStatusPath, systemConfig);

    Serial.println("AC power is going off");
    Buzzer();
    BatteryInterval = CheckBatteryInterval;
    // CheckBattery();
    if(SIMCARDEN && !SendingSMS) {
      sendToAll(ACOFFMSG);
    }

    // WifiReconnect();

  } else if(digitalRead(ACIn) && !systemConfig["AcStatus"].as<bool>()) {

    //AC Was off and now it turnes on
    Serial.println("AC power is going on");
    systemConfig["AcStatus"] = true;
    saveJson(SystemStatusPath, systemConfig);
    
    Buzzer();
    BlinkColor(CRGB::Blue, NUM_LEDS, 250, 2, leds);
    
    if(systemConfig["SystemStatus"].as<bool>())
      LinearColorFill((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
    else
      LinearColorFill((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);
    
    SensorsValueJson["Power"] = "AC - 100";
    if(SIMCARDEN && !SendingSMS) {
      sendToAll(ACONMSG);
    }

    //  WifiReconnect();
  }

  if(millis() - prevWifiCheckConnectionTime > WifiCheckConnectionInterval)//check each 10s
  {
    prevWifiCheckConnectionTime = millis();
    if(WiFi.status() != WL_CONNECTED) {

      //search if wifissid is now in area
      int n = WiFi.scanNetworks();
      bool ssidFound = false;
      for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == ssidSPIFF.c_str()) {
          Serial.println("***** SSID found: " + WiFi.SSID(i));
          ssidFound = true;
          break;
        }
      }
      if(ssidFound) {

        if(initWiFi()) {

          bool GetTimeFlag = false; //TODO: fix this as a function
          TryGetNTP = 0;
          while(TryGetNTP < NTPthr)
          {
            GetTimeFlag = GetTime(timeApiUrl);
            if(GetTimeFlag)
              break;

            //esp_task_wdt_reset();
            Serial.println("Failed to obtain time");
            //esp_task_wdt_reset();
            sleep(1);
            TryGetNTP++;
            if(TryGetNTP > NTPthr)
            {
              break;
            }
          }
        }

      } else if(WifiConnected) {

        WifiNotConnectedCount++;
        if(WifiNotConnectedCount > WifiCheckConnection) {

          WifiNotConnectedCount = 0;
          WifiConnected = false;
          WiFi.mode(WIFI_MODE_AP);
          Serial.println("Setting AP (Access Point)");
          // NULL sets an open Access Point
          WiFi.softAP("Didomak-WIFI-MANAGER", NULL); //TODO: get from string define

          IPAddress IP = WiFi.softAPIP();
          Serial.print("AP IP address: ");
          Serial.println(IP);   
          LinearColorFill(CRGB(76,0,153), NUM_LEDS, leds);
        }
      }
    }
    //  else //STA mode
    // {
    //   // wifi mode but not connected
    //   Serial.printf(">>>> Wifi status = %d\n", WiFi.status());
    //   Serial.printf(">>>> WifiCheck = %d\n",WifiCheck);

    //   if (WiFi.status() != WL_CONNECTED)
    //   {
    //     WifiConnected=false;
    //   }

    //   WifiCheck++;
    //   // Serial.println("Wifi not connected");
    //   if(WifiCheck > WifiCheckConnection)
    //   {
    //     WifiConnected = false;
    //     // WiFi.disconnect();
    //     Serial.println("AP and Wifi Not Set Up!");
    //     Serial.println("Setting AP (Access Point)");
    //     // NULL sets an open Access Point
    //     WiFi.softAP("Didomak-WIFI-MANAGER", NULL);

    //     IPAddress IP = WiFi.softAPIP();
    //     Serial.print("AP IP address: ");
    //     Serial.println(IP);   
    //     LinearColorFill(CRGB(76,0,153), NUM_LEDS, leds);
    //   }
    // }
  }


    // if (WiFi.status() != WL_CONNECTED)
    // {
    //   Serial.printf("Wifi status = %d\n", WiFi.status());
    //   WiFi.reconnect();
    //   WifiCheck++;
    //   // Serial.println("Wifi not connected");
    //   if(!WifiConnected && WifiCheck > WifiCheckConnection)
    //   {
    //     Serial.println("AP and Wifi Not Set Up!");
    //     if(initWiFi())
    //     {
    //       WifiCheck = 0;
    //       Serial.println("WiFi Connected");
    //       // server.end();
    //       // StartServers();
    //     }
    //   }
    // }
  // }
  ws.cleanupClients();

  //SMS
  if(!SendingSMS)
  {
    if(Serial2.available())
    {
      String line = Serial2.readString();
      if(line != "")
        Serial.printf("### Read SMS commands in loop #####\n%s\n",line);  

      if (line.indexOf("+CMTI:") != -1) 
      { // Check if the line contains '+CMT:'
        int index = parseIndex(line);
        Serial2.println("AT+CMGR=" + String(index));
        String line = Serial2.readString();
        // Serial.println("line");  
        Serial.println(line);
        String phoneN = parsePhoneNumber(line); 
        Serial.println(phoneN);
        if (checkPhonenumber(phoneN))
        {
          currentUser = phoneN;
          digitalWrite(LEDPIN,1);
          vTaskDelay(250 / portTICK_PERIOD_MS);
          digitalWrite(LEDPIN,0);
          Serial.println("hi, Receive command from valid phone num.");
          Serial.println(line);
          
          String b = findCommand(line);
          
          inboxCounter++;
          Serial.printf(">>> inbox counter = %d\n", inboxCounter);
          if(inboxCounter > INBOXCpacity)
          {
            Serial.println("Clearing SMS inbox");
            inboxCounter = 0;
            // sendReqtoSim(DELETALL, ATOK);
            // sendReqtoSim(DELETINBOX, ATOK);
            sendReqtoSim(DELAll, ATOK);
            sendReqtoSim(DELRead, ATOK);
          }
          
        }
      }
    }
    
    if(SIMCARDEN && (CreditCheckCounter > CreditCheckthr))
    {
      CreditCheckCounter = 0;
      Serial.println("checking Credit. balance is: ");
      int CreditValue = -1;
      CreditValue = checkCredit();
      Serial.println(CreditValue);
      systemConfig["Credit"] = CreditValue;
      saveJson(SystemStatusPath,systemConfig);
      String message;
      serializeJson(systemConfig, message);
      notifyClients(message);
    }
    // updateSerial();
  }
  if(millis() - previousMillisheart > intervalheart)
  {
    digitalWrite(LEDPIN, !digitalRead(LEDPIN));
    previousMillisheart = millis();
  }
  vTaskDelay(1 / portTICK_PERIOD_MS); // Yield control 1ms
  // yield();
}

void ResetFactory()
{
  Serial.println("!!!!!!!!!! Reseting to factory config !!!!!!!");
  BlinkColorSmooth(CRGB::DarkViolet, NUM_LEDS, 1, 20, leds);
  systemConfig.clear();
  RelayTimes.clear();
  phoneNumbers.clear();
  adminPhoneNumbers.clear();
  LoginPass.clear();
  SystemLog.clear();
  RebootCount.clear();
  WifiConfig.clear();

  saveJson(SystemStatusPath, systemConfig);
  saveJson(RelayClockPath, RelayTimes);
  saveJson(phoneNumsPath, phoneNumbers);
  saveJson(adminPhoneNumsPath, adminPhoneNumbers);
  saveJson(LoginPassPath, LoginPass);
  saveJson(LastLogsPath, SystemLog);
  saveJson(RebootCounterPath, RebootCount);
  saveJson(WifiConfigPath, WifiConfig);
  delay(300);
  BlinkColorSmooth(CRGB::DarkViolet, NUM_LEDS, 1, 20, leds);
  ESP.restart();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {

  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    Serial.println("\nSocket message: \n");
    Serial.println(message);
    Serial.println("\n");

    // Parse the message as JSON
    JsonDocument doc;
    deserializeJson(doc, message);

    if (doc.containsKey("Lamp")) {
      int lampState = doc["Lamp"];
      Serial.println("Lamp State: " + String(lampState));
      if(systemConfig.containsKey("LampStatus"))
      {
        systemConfig["LampStatus"] = lampState;
        digitalWrite(RELAYPIN, lampState);
      }
    }
    if (doc.containsKey("System")) {
      int systemState = doc["System"];

      if(systemState == 0) {
        lastAlarmTime  = millis() - systemConfig["AlarmDuration"].as<int>() * 1000; //stop alarm
        AlarmRGB = false;
        sendToAll(ALARMOFF);
      } else {
        sendToAll(ALARMON);
      }

      Serial.println("System State: " + String(systemState));
      if(systemConfig.containsKey("SystemStatus"))
      {
        systemConfig["SystemStatus"] = systemState;
      }
      // if(systemState == 0)
      //   DefaultColor = CRGB(204,102,0);
      // else
      //   DefaultColor = CRGB::Green;
    }
    if (doc.containsKey("PhoneNumbers"))
    {
      Serial.print("Phone number added to list: ");
      Serial.println(doc["PhoneNumbers"].as<String>());
      phoneNumbers.clear();
      adminPhoneNumbers.clear();
      parsePhoneNumbers(doc);
      JsonDocument PhoneList = convertToOriginalFormat(phoneNumbers,adminPhoneNumbers);
      String readings;
      serializeJson(PhoneList, readings);
      notifyClients(readings);
    }
    if (doc.containsKey("RelayTime")) {
      String RelayTimeWeb = doc["RelayTime"].as<String>();
      RelayTimes.clear();
      RelayTimes = doc;
      Serial.println("RelayTimes new: ");
      serializeJson(RelayTimes, Serial);
      Serial.println();
      saveJson(RelayClockPath, RelayTimes);
    }
    if (doc.containsKey("NewPassword") && doc.containsKey("ConfirmPassword")) {
      String NewPassword = doc["NewPassword"].as<String>();
      String ConfirmPassword = doc["ConfirmPassword"].as<String>();
      if(NewPassword != "" && ConfirmPassword != "")
      {
        if(strcmp(NewPassword.c_str(), ConfirmPassword.c_str()) == 0)
        {
          Serial.println("New pass gets correct. changing login password...");
          LoginPass["Pass"] = NewPassword;
          Serial.println("LoginPass new: ");
          serializeJson(LoginPass, Serial);
          Serial.println();
          saveJson(LoginPassPath, LoginPass);
        }
        else
        {
          Serial.println("pass and confirm not match!");
        }
      }
    }
    if (doc.containsKey("TempSensorEn")) {
      int TempSensorState = doc["TempSensorEn"];
      Serial.println("TempSensor State: " + String(TempSensorState));
      if(systemConfig.containsKey("TempSensorEn"))
      {
        systemConfig["TempSensorEn"] = TempSensorState;
      }
    }
    if (doc.containsKey("HumiditySensorEn")) {
      int HumiditySensorState = doc["HumiditySensorEn"];
      Serial.println("HumiditySensor State: " + String(HumiditySensorState));
      if(systemConfig.containsKey("HumiditySensorEn"))
      {
        systemConfig["HumiditySensorEn"] = HumiditySensorState;
      }
    }
    if (doc.containsKey("FireSensorEn")) {
      int FireSensorState = doc["FireSensorEn"];
      Serial.println("FireSensor State: " + String(FireSensorState));
      if(systemConfig.containsKey("FireSensorEn"))
      {
        systemConfig["FireSensorEn"] = FireSensorState;
      }
    }
    if (doc.containsKey("CoSensorEn")) {
      if(doc["CoSensor"].as<String>() != "") 
      {
        int CoSensorState = doc["CoSensorEn"];
        Serial.println("CoSensor State: " + String(CoSensorState));
        if(systemConfig.containsKey("CoSensorEn"))
        {
          systemConfig["CoSensorEn"] = CoSensorState;
        }
      }
    }
    if (doc.containsKey("LogCount")) {
      if(doc["LogCount"].as<String>() != "") 
      {
        int logCountWeb = doc["logCount"];
        Serial.println("logCount set to: " + String(logCountWeb));
        if(systemConfig.containsKey("LogCount"))
        {
          systemConfig["LogCount"] = logCountWeb;
        }
      }
    }
    if (doc.containsKey("RadarDistance")) {
      int RadarDistanceState = doc["RadarDistance"].as<int>();
      Serial.println("RadarDistance State: " + String(RadarDistanceState));
      ConfigABDParams(LD2420_MaxDisConf, RadarDistanceState);
      if(systemConfig.containsKey("RadarDistance"))
      {
        systemConfig["RadarDistance"] = RadarDistanceState;
      }
    }
    if (doc.containsKey("RadarDistanceVariable")) {
      int RadarDistanceState = doc["RadarDistanceVariable"].as<int>();
      Serial.println("RadarDistance var set State: " + String(RadarDistanceState));
    }
    if (doc.containsKey("RgbRingVariable")) {
      int rgbRingState = doc["RgbRingVariable"].as<int>();
      Serial.println("rgbRing set State: " + String(rgbRingState));
      FastLED.setBrightness(rgbRingState * FullBrightness / 100);
      FastLED.show();
    }
    if (doc.containsKey("RgbRing")) {
      int rgbRingState = doc["RgbRing"].as<int>();
      Serial.println("rgbRing brightness State: " + String(rgbRingState));
      if(systemConfig.containsKey("RgbRing"))
      {
        systemConfig["RgbRing"] = rgbRingState;
      }
    }
    if (doc.containsKey("AlarmDelay")) {
      if(doc["AlarmDelay"].as<String>() != "") 
      {
        int AlarmDelayState = doc["AlarmDelay"];
        Serial.println("AlarmDelay set to: " + String(AlarmDelayState));
        if(systemConfig.containsKey("AlarmDelay"))
        {
          systemConfig["AlarmDelay"] = AlarmDelayState;
        }
      }
    }
    if (doc.containsKey("Templimit")) {
      if(doc["Templimit"].as<String>() != "") 
      {
        int TemplimitWeb = doc["Templimit"];
        Serial.println("Templimit set to: " + String(TemplimitWeb));
        if(systemConfig.containsKey("Templimit"))
        {
          systemConfig["Templimit"] = TemplimitWeb;
        }
      }
    }
    if (doc.containsKey("AlarmDuration")) {
      if(doc["AlarmDuration"].as<String>() != "") 
      {
        int AlarmDurationWeb = doc["AlarmDuration"];
        Serial.println("AlarmDuration set to: " + String(AlarmDurationWeb));
        if(systemConfig.containsKey("AlarmDuration"))
        {
          systemConfig["AlarmDuration"] = AlarmDurationWeb;
        }
      }
    }
    if (doc.containsKey("Creditlimit")) {
      if(doc["Creditlimit"].as<String>() != "") 
      {
        int CreditlimitWeb = doc["Creditlimit"];
        Serial.println("Creditlimit set to: " + String(CreditlimitWeb));
        if(systemConfig.containsKey("Creditlimit"))
        {
          systemConfig["Creditlimit"] = CreditlimitWeb;
        }
      }
    }
    if (doc.containsKey("Colimit")) {
      if(doc["Colimit"].as<String>() != "") 
      {
        int ColimitWeb = doc["Colimit"];
        Serial.println("Colimit set to: " + String(ColimitWeb));
        if(systemConfig.containsKey("Colimit"))
        {
          systemConfig["Colimit"] = ColimitWeb;
        }
      }
    }
    if (doc.containsKey("Firelimit")) {
      if(doc["Firelimit"].as<String>() != "") 
      {
        int FirelimitWeb = doc["Firelimit"];
        Serial.println("Firelimit set to: " + String(FirelimitWeb));
        if(systemConfig.containsKey("Firelimit"))
        {
          systemConfig["Firelimit"] = FirelimitWeb;
        }
      }
    }  
    if (doc.containsKey("SystemOnColor")) {
      String SystemOnColor = doc["SystemOnColor"];
      Serial.println("SystemOnColor change to: " + SystemOnColor);
      if(systemConfig.containsKey("SystemOnColor"))
      {
        systemConfig["SystemOnColor"] = SystemOnColor;
      }
    }
    if (doc.containsKey("SystemOffColor")) {
      String SystemOffColor = doc["SystemOffColor"];
      Serial.println("SystemOffColor change to: " + SystemOffColor);
      if(systemConfig.containsKey("SystemOffColor"))
      {
        systemConfig["SystemOffColor"] = SystemOffColor;
      }
    }
    if (doc.containsKey("AlarmColor")) {
      String AlarmColor = doc["AlarmColor"];
      Serial.println("AlarmColor change to: " + AlarmColor);
      if(systemConfig.containsKey("AlarmColor"))
      {
        systemConfig["AlarmColor"] = AlarmColor;
      }
    }

    if (!doc.containsKey("RgbRingVariable") && !doc.containsKey("RadarDistanceVariable")) { //if it isnt rgb slider changing state

      Buzzer();
      BlinkColor(CRGB::White, NUM_LEDS, 250, 2, leds);

      //reset rgb ring color to default
      FastLED.setBrightness(systemConfig["RgbRing"].as<int>() * FullBrightness / 100);
      FastLED.show();
      if(systemConfig["SystemStatus"].as<bool>())
        FullColor((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
      else
        FullColor((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);


      delay(200);
      
      Serial.println("New System config: ");
      serializeJsonPretty(systemConfig, Serial);
      Serial.println();
      saveJson(SystemStatusPath, systemConfig);
      String readings;
      serializeJson(systemConfig, readings);
      notifyClients(readings);
    }
  }
}

void systemConfigInitial()
{
  if(!systemConfig.containsKey("SystemStatus")  || !systemConfig.containsKey("LampStatus") || !systemConfig.containsKey("AlarmStatus") ||
     !systemConfig.containsKey("TempSensorEn")  || !systemConfig.containsKey("HumiditySensorEn") || !systemConfig.containsKey("AcStatus") ||
     !systemConfig.containsKey("FireSensorEn")  || !systemConfig.containsKey("CoSensorEn") || !systemConfig.containsKey("LogCount") ||
     !systemConfig.containsKey("RadarDistance") || !systemConfig.containsKey("RgbRing")  || !systemConfig.containsKey("AlarmDelay") || 
     !systemConfig.containsKey("Credit") || !systemConfig.containsKey("Templimit")|| !systemConfig.containsKey("Creditlimit") ||
     !systemConfig.containsKey("Firelimit") || !systemConfig.containsKey("Colimit") || !systemConfig.containsKey("AlarmDuration") ||
     !systemConfig.containsKey("SystemOnColor") || !systemConfig.containsKey("SystemOffColor") || !systemConfig.containsKey("AlarmColor"))
  {
    Serial.println("First initial of system config(all off).");
    systemConfig["SystemStatus"]   = 0;
    systemConfig["LampStatus"]     = 0;
    systemConfig["AlarmStatus"]    = 0;
    // systemConfig["sendMessage"] = false;
    systemConfig["TempSensorEn"]   = 0;
    systemConfig["HumiditySensorEn"] = 0;
    systemConfig["FireSensorEn"]   = 0;
    systemConfig["CoSensorEn"]     = 0;
    systemConfig["LogCount"]       = 50;
    systemConfig["RadarDistance"]  = 5;
    systemConfig["RgbRing"]        = 15;
    systemConfig["AlarmDelay"]     = 0;
    systemConfig["Credit"]         = -1;
    systemConfig["Templimit"]      = 35;
    systemConfig["Creditlimit"]    = 10000;
    systemConfig["Firelimit"]      = 50;
    systemConfig["Colimit"]        = 50;
    systemConfig["AlarmDuration"]  = 60;
    systemConfig["SystemOnColor"]  = "008000";
    systemConfig["SystemOffColor"] = "FF8C00";
    systemConfig["AlarmColor"]     = "FF0000";
    systemConfig["AcStatus"]       = false;
    saveJson(SystemStatusPath, systemConfig);
  }
  else
  {
    Serial.println("=====System Config Last Status=====");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
  }
}

float calibration(int pin )
{
  float ro;
  ro = MQCalibration(pin);
  Serial.print("Ro=");
  Serial.print(ro);
  Serial.println("kohm");
  return ro;

}

float MQCalibration(int mq_pin)
{
  int i;
  float val=0;
 
  for (i=0;i<CALIBARAION_SAMPLE_TIMES;i++) {            //take multiple samples
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBARAION_SAMPLE_TIMES;                   //calculate the average  value
 
  val = val/RO_CLEAN_AIR_FACTOR;                        //divided  by RO_CLEAN_AIR_FACTOR yields the Ro 
                                                        //according  to the chart in the datasheet 
 
  return val; 
}

float MQResistanceCalculation(int rawADC)
{
  return ( ((float)RL_VALUE*(1023-rawADC)/rawADC));
}

int COGetGasPercentage(float rsRoRatio)
{
  return MQGetPercentage(rsRoRatio,COCurve); 
}

int fireGetGasPercentage(float rsRoRatio)
{
  return MQGetPercentage(rsRoRatio,fireCurve); 
}

int  MQGetPercentage(float rsRoRatio, float *pcurve)
{
  return (pow(10,(  ((log(rsRoRatio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}

float MQRead(int mq_pin)
{
  int i;
  float rs=0;
 
  for (i=0;i<READ_SAMPLE_TIMES;i++)  {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
 
  rs = rs/READ_SAMPLE_TIMES;
 
  return rs;  
}

String convertJsonToString(JsonDocument& doc) {
    String result;
    for (JsonPair kv : doc.as<JsonObject>()) {
        if (!result.isEmpty()) {
            result += "\n";
        }
        // Append key-value pair in the desired format
        result += kv.key().c_str();
        result += ":";
        
        if (kv.value().is<bool>()) {
            // For boolean values, append "true" or "false" without quotes
            result += kv.value().as<bool>() ? "On" : "Off";
        } 
        // else {
        //     // Implement other types as needed (e.g., Strings, numbers)
        //     // This example only handles booleans for brevity
        // }
    }

    return result;
}

//==== sim functions =====
int checkCredit()
{
  sendReqtoSim("AT+CUSD=1",ATOK); 
  sendReqtoSim("AT+CUSD=1,\"*555*4*3#\"","72");
  sendReqtoSim("AT+CUSD=1,\"2\"","English"); 
  //esp_task_wdt_reset();
  delay(2000);
  //esp_task_wdt_reset();
  String balanceLine = sendReqtoSim("AT+CUSD=1,\"*555*1*2#\"","Credit:"); 
  sendReqtoSim("AT+CUSD=0",ATOK); 
  //"CUSD: 0, \"On 1403/07/01.your balance is 687348 RialsA new generation of MyIrancell super app *45#\", 15␍";
  int startIndex = balanceLine.indexOf("Credit:") + 7;
  int endIndex   = balanceLine.indexOf("IRR");
  String balanceStr = balanceLine.substring(startIndex, endIndex);
  balanceStr.replace(",", "");
  long balanceValue = balanceStr.toInt() / 10; //
  Serial.printf(">>>>>>>>  balance is: %d\n", balanceValue);
  return balanceValue;
}

String sendReqtoSim(String Command, String DesiredRes)
{
  //esp_task_wdt_reset();
  SendingSMS = true;
  boolean at_flag = 1;
  int Attempts = 0;
  while (at_flag && Attempts < CommandAttempts)
  {
    //esp_task_wdt_reset();
    Attempts++;
    Serial.printf("\nSending %s  ...(%d) -- desired response: %s\n", Command.c_str(), Attempts, DesiredRes);
    delay(10);
    Serial2.println(Command);
    // delay(100);
    unsigned long startTime = millis();
    // bool recComand = false;
    while (millis() - startTime < waitingforReqRes) 
    {
      //esp_task_wdt_reset();
      while (Serial2.available() > 0)
      {
        //esp_task_wdt_reset();
        String line = Serial2.readString();
        Serial.println("==== readed data =====");  
        Serial.println(line);
        // if (Serial2.find(DesiredRes.c_str())) at_flag = 0;
        Serial.print("line.indexOf(DesiredRes): ");
        Serial.println(line.indexOf(DesiredRes));
        if (line.indexOf(DesiredRes) != -1) 
        {
          Serial.printf("Request %s : Success.\n", Command.c_str());
          at_flag = 0;
          SendingSMS = false;
          return line;
        }
        // else
        // {
        //   recComand = true;
        //   break;
        // }
      }
      // if(recComand)
      //   break;
    }
    Serial.println("###############");
    delay(10);
  }
  if (at_flag == 1)
  {
    Serial.printf("Request %s : Failed!\n", Command.c_str());
    SendingSMS = false;
    return "Failed";
    //restart ESP or Sim800
  }
  else Serial.printf("Request %s : Success.\n", Command.c_str());
  SendingSMS = false;
  return "";
}

String phoneNumberToUCS2(const String& phoneNumber) {
  String ucs2Number = "";
  for (unsigned int i = 0; i < phoneNumber.length(); i++) {
    char c = phoneNumber.charAt(i);
    ucs2Number += "00"; // UCS-2 encoding adds '00' prefix for each character
    ucs2Number += String(c, HEX); // Convert the character to its hexadecimal representation
  }
  return ucs2Number;
}

void initPersianMode()
{
  sendReqtoSim(SetUCS2Mode1, ATOK);
  sendReqtoSim(SetUCS2Mode2, ATOK);
}

void initReceiveMode()
{
  sendReqtoSim(SMSRECEIVE, ATOK);
}

String getLast36Chars(const String& input) {
  // Ensure the input String has at least 36 characters
  if (input.length() < 36) {
    return "";
  }
  
  // Extract the last 36 characters
  String last36Chars = input.substring(input.length() - 36);
  return last36Chars;
}

String extractPersianPhoneNumber(const String& input) {
  int firstQuote = input.indexOf("\"");
  int secondQuote = input.indexOf("\"", firstQuote + 1);
  int thirdQuote = input.indexOf("\"", secondQuote + 1);
  int fourthQuote = input.indexOf("\"", thirdQuote + 1);

  if (thirdQuote != -1 && fourthQuote != -1) {
    return input.substring(thirdQuote + 1, fourthQuote);
  }
  return "";
}

String extractPersianMessage(const String& input) {
  int lastQuoteIndex = input.lastIndexOf("\"");
  
  // The message starts right after the last quote and comma
  int messageStart = input.indexOf("\n", lastQuoteIndex) + 1;
  
  if (messageStart > 0 && messageStart < input.length()) {
    return input.substring(messageStart); // Extract the message content after the last quote and newline
  }
  return ""; // Return an empty String if not found
}

String parsePhoneNumber(String cmgrResponse) {
  int firstQuoteIndex = cmgrResponse.indexOf('"') + 1; // Index of the first quote
  int secondQuoteIndex = cmgrResponse.indexOf('"', firstQuoteIndex); // Index of the quote after the first quote
  int thirdQuoteIndex = cmgrResponse.indexOf('"', secondQuoteIndex + 1) + 1; // Index of the third quote
  int fourthQuoteIndex = cmgrResponse.indexOf('"', thirdQuoteIndex); // Index of the fourth quote, which is right after the phone number
  
  if (firstQuoteIndex >= 0 && secondQuoteIndex >= 0 && thirdQuoteIndex >= 0 && fourthQuoteIndex >= 0) {
    // Extract the phone number
    String phoneNumber = cmgrResponse.substring(thirdQuoteIndex, fourthQuoteIndex);
    return phoneNumber;
  }
  
  return ""; // Return an empty String if the phone number couldn't be extracted
}

int parseIndex(String line) {
  int indexStart = line.indexOf(",") + 1; // Find the start of the index (after the comma)
  int index = line.substring(indexStart).toInt(); // Extract the index part and convert to integer
  return index; // Return the parsed index
}

void GSMInit()
{
  Serial.println("Initialing sim800!");
  //reset software sim800
  //sendReqtoSim(RESETSIM800,ATOK);
  //AT
  // String ResAT = sendReqtoSim(AT, ATOK);
  // if(ResAT == "Failed")
  //   RestartGSM();

  //esp_task_wdt_reset();
  //sim800 factory reset
  sendReqtoSim("AT&F", ATOK);
  //esp_task_wdt_reset();
  //Signal Quality
  sendReqtoSim(SignalQ, SignalQres);
  //esp_task_wdt_reset();
  //Irancell and sms flow time
  sendReqtoSim(IRANCELL, ATOK);
  //esp_task_wdt_reset();
  //delete all messages
  // sendReqtoSim(DELETINBOX, ATOK);
  sendReqtoSim(DELAll, ATOK);
}

void RestartGSM()
{
  Serial.println("Restarting GSM... turning off");
  digitalWrite(Simpower , 0);
  delay(1000);
  digitalWrite(Simpower , 1);
  delay(1000);
  Serial.println("Gsm off, now turning on");
  digitalWrite(Simpower , 0);
  delay(1000);
  digitalWrite(Simpower , 1);
  delay(10000);
  Serial.println("GSM Ready.");


}

void updateSerial()
{
  delay(500);
  while (Serial.available())
  {
    Serial2.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while (Serial2.available())
  {
    Serial.write(Serial2.read());//Forward what Software Serial received to Serial Port
  }
}

void configTextMode()
{
  sendReqtoSim(SetTextMode, ATOK);
  sendReqtoSim(EnDelivery,ATOK);
}

void processSMS(String sms)
{
  Serial.print("Received SMS: ");
  Serial.println(sms);
}

void sendMessage(String sms, String PhNum)
{
  if(SIMCARDEN)
  {
    SendingSMS = true;
    configTextMode();
    String phonenumber_str = "AT+CMGS=\"" + PhNum + "\"";
    Serial2.println(phonenumber_str.c_str());
    updateSerial();
    Serial2.print(sms); //text content
    updateSerial();
    Serial.println();
    Serial2.write(26);
    Serial.println("Message Sent");
    SendingSMS = false;
  }
}

void sendToAll(String payload)
{
  if(SIMCARDEN)
  {
   // configTextMode();
    SendingSMS = true;
    Serial.printf("Sending msg [%s] to all\n", payload.c_str());
    Serial.println(">>>> Sending to Normals");
    serializeJsonPretty(phoneNumbers, Serial);
    Serial.println();
    if(phoneNumbers.size() != 0)
    {
      CreditCheckCounter++;
      for (JsonPair kv : phoneNumbers.as<JsonObject>()) 
      {
        //esp_task_wdt_reset();
        Serial.println("\n>>> Phone num:");
        Serial.println(kv.value().as<String>());
        sendMessage(payload, String(kv.value().as<String>().c_str()) );
        Serial.println(">>> deliver");
        if (!checkForDeliveryReport() )
        {
          //esp_task_wdt_reset();
          // delay(200);
          vTaskDelay(200 / portTICK_PERIOD_MS); 
          Serial.println("retry2.....");
          sendMessage(payload, String(kv.value().as<String>().c_str()) );
          Serial.println(">>> deliver2");
          if (!checkForDeliveryReport() )
          {
            //esp_task_wdt_reset();
            // delay(200);
            vTaskDelay(200 / portTICK_PERIOD_MS); 
            Serial.println("retry3.....");
            sendMessage(payload, String(kv.value().as<String>().c_str()) );
            Serial.println(">>> deliver3");
            if (!checkForDeliveryReport() )
            {
              //esp_task_wdt_reset();
              Serial.println("make call.....");
              make_call(String(kv.value().as<String>().c_str()));
            }
          }
        }
        // delay(10);
        vTaskDelay(10 / portTICK_PERIOD_MS); 
      }
    }
    else
    {
      Serial.println("Normal contacts are null!");
    }
    Serial.println(">>>> Sending to Admins");
    serializeJsonPretty(adminPhoneNumbers, Serial);
    Serial.println();
    if(adminPhoneNumbers.size() != 0)
    {
      CreditCheckCounter++;
      for (JsonPair kv : adminPhoneNumbers.as<JsonObject>())
      {
        //esp_task_wdt_reset();
        sendMessage(payload, String(kv.value().as<String>().c_str()) );
        Serial.println("deliver admin");
        if (!checkForDeliveryReport() )
        {
          //esp_task_wdt_reset();
          // delay(200);
          vTaskDelay(200 / portTICK_PERIOD_MS); 
          Serial.println("retry2 admin.....");
          sendMessage(payload, String(kv.value().as<String>().c_str()) );
          Serial.println("deliver admin2");
          if (!checkForDeliveryReport() )
          {
            //esp_task_wdt_reset();
            // delay(200);
            vTaskDelay(200 / portTICK_PERIOD_MS); 
            Serial.println("retry3 admin.....");
            sendMessage(payload, String(kv.value().as<String>().c_str()) );
            Serial.println("deliver admin3");
            if (!checkForDeliveryReport() )
            {
              //esp_task_wdt_reset();
              Serial.println("make call admin.....");
              make_call(String(kv.value().as<String>().c_str()));
            }
          }
        }
        // delay(10);  
        vTaskDelay(10 / portTICK_PERIOD_MS); 
      }
    }
    else
    {
      Serial.println("Admin contacts are null!");
    }
    // delay(10);
    vTaskDelay(10 / portTICK_PERIOD_MS); 
    // initReceiveMode();
    SendingSMS = false;
  }
  yield();
}

String normalizePhone(String phoneNumber)
{
  String normalized = "";
  if (phoneNumber.length() == 11 && phoneNumber.startsWith("0")) {
    normalized = "+98" + phoneNumber.substring(1);
  } 
  else if (phoneNumber.length() == 13) {
      normalized = phoneNumber;
  }
  Serial.print("Normalized phone num: ");
  Serial.println(normalized);
  return normalized;
}

bool checkPhonenumber(String phoneNumber)
{
  
  String normalized = "";
  if (phoneNumber.length() == 11 && phoneNumber.startsWith("0")) {
    normalized = "+98" + phoneNumber.substring(1);
  } 
  else if (phoneNumber.length() == 13) {
      normalized = phoneNumber;
  }
  Serial.print("Normalized phone num: ");
  Serial.println(normalized);
  for (JsonPair kv : adminPhoneNumbers.as<JsonObject>()) 
  {
        if ((kv.value().as<String>()) == normalized) {
            return true;
        }
  }
  return false;
  
}

void creatPhonenumber(String newphone)   // +98 is essential
{
  int json_size = phoneNumbers.size();
  String phonenumber_key = "PHN" + String(json_size + 1);
  phoneNumbers[ phonenumber_key] = normalizePhone(newphone);
  saveJson(phoneNumsPath, phoneNumbers);
}

void creatAdminPhonenumber(String newphone)   // +98 is essential
{
  int json_size = adminPhoneNumbers.size();
  String phonenumber_key = "PHN" + String(json_size + 1);
  adminPhoneNumbers[ phonenumber_key] = normalizePhone(newphone);
  saveJson(adminPhoneNumsPath, adminPhoneNumbers);
}

String findCommand(String sms)
{
  String receivedMessage = String(sms);
  // Serial.println("received...");
  // Serial.println(receivedMessage);
  receivedMessage.toLowerCase();
  // Serial.println("Lowercase message:");
  // Serial.println(receivedMessage);
  bool ValidCommand = false;
  if (receivedMessage.indexOf(SYSTEM) != -1) 
  {
    Serial.println("System Command received.");
    systemAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(LAMP) != -1)
  {
    lampAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(SMSIP) != -1)
  {
    ipAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(ALARM) != -1)
  {
    alarmAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(MONOXIDE)!=-1) //monoxid منوکسیدکربن
  {
    carbonSensorAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(FIRE)!=-1) //fire اتش
  {
    fireAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(HUMIDITY)!=-1) // humidity رطوبت
  {
  
    HumiditySensorAct(receivedMessage);
    ValidCommand = true;
  }
  else if (receivedMessage.indexOf(TEMP)!=-1) //temp دما
  {
    tempSensorAct(receivedMessage);
    ValidCommand = true;
  }
  if(ValidCommand)
  {
    Buzzer();
    // if(systemConfig["RgbRing"].as<int>())
    BlinkColor(CRGB::White, NUM_LEDS, 250, 2, leds);
    //reset rgb ring color to default
    if(systemConfig["SystemStatus"].as<bool>())
      LinearColorFill((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
    else
      LinearColorFill((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);
      
    CreditCheckCounter++;
    if(CreditCheckCounter > CreditCheckthr)
    {
      CreditCheckCounter = 0;
      Serial.println("checking Credit. balance is: ");
      int CreditValue = -1;
      CreditValue = checkCredit();
      Serial.println(CreditValue);
      systemConfig["Credit"] = CreditValue;
      saveJson(SystemStatusPath,systemConfig);
      String message;
      serializeJson(systemConfig, message);
      notifyClients(message);
    }
    saveJson(SystemStatusPath, systemConfig);
    Serial.println("=====System Config New Status=====");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
    String message;
    serializeJsonPretty(systemConfig, message);
    notifyClients(message);
  //   //Send Sms back to let all users know new configs
    // sendToAll(convertJsonToString(systemConfig));
  }
  return "";
}

void systemAct(const String& receivedMessage)
{
  Serial.println("A system command received...");
  if (receivedMessage.indexOf(OFF) != -1) 
  {
    Serial.println("Turning off system...");
    systemConfig["SystemStatus"] = 0;
    sendMessage(SYSTEMOFF,currentUser);

  }
  else if (receivedMessage.indexOf(ON) != -1)
  {
    systemConfig["SystemStatus"] = 1;
    Serial.println("Turning on system!");
    sendMessage(SYSTEMON,currentUser);
  }
}

void ipAct(const String& receivedMessage)
{
  Serial.println("IP command received.");
  Serial.println("returning system ip...");
  String ipString = WiFi.localIP().toString();
  ipString.replace("." , ",");
  sendMessage("Didomak IP: " + ipString, currentUser);
}

void lampAct(const String& receivedMessage)
{
  Serial.println("Lamp command received...");
  // Serial.println(receivedMessage);
  if (receivedMessage.indexOf(OFF) != -1) 
  {
    Serial.println("Turning off lamp...");
    systemConfig["LampStatus"] = 0;
    digitalWrite(RELAYPIN, systemConfig["LampStatus"].as<bool>());
    sendMessage(LAMPOFF,currentUser);

  }
  else if (receivedMessage.indexOf(ON) != -1)
  {
    Serial.println("Turning on lamp!");
    systemConfig["LampStatus"] = 1;
    digitalWrite(RELAYPIN, systemConfig["LampStatus"].as<bool>());
    sendMessage(LAMPON,currentUser);
  }
}

void alarmAct(const String& receivedMessage)
{
  Serial.println("A alarm command received...");
  // Serial.println(receivedMessage);
  if (receivedMessage.indexOf(OFF) != -1)
  {
  Serial.println("Turning off alarm!");
   systemConfig["AlarmStatus"] = 0;
   sendMessage(ALARMOFF,currentUser); 
  }
  else
    {
      Serial.println("Turning on a alarm!");
      systemConfig["AlarmStatus"] = 1;
      sendMessage(ALARMON,currentUser);

    }
}

void tempSensorAct(const String& receivedMessage)
{
  Serial.println("A temp sensore command received...");
  // Serial.println(receivedMessage);
  if (receivedMessage.indexOf(OFF)!=-1)  
    {
      Serial.println("Turning off a temp sensore!");
      systemConfig["TempSensorEn"] = 0;
      sendMessage(TEMPOFF,currentUser);
    }
    else
    {
      Serial.println("Turning on a temp sensore!");
      systemConfig["TempSensorEn"] = 1;
      sendMessage(TEMPON,currentUser);

    }
}

void HumiditySensorAct(const String& receivedMessage)
{
  Serial.println("A humidity sensore command received...");
  if (receivedMessage.indexOf(OFF)!=-1)  
    {
      Serial.println("Turning off a humidity sensore!");
      systemConfig["FireSensorEn"] = 0;
      sendMessage(HUMIDITYOFF,currentUser);
    }
    else
    {
      Serial.println("Turning on a humidity sensore!");
      systemConfig["FireSensorEn"] = 1;
      sendMessage(HUMIDITYON,currentUser);

    }
}

void carbonSensorAct(const String& receivedMessage)
{
  Serial.println("A carbon sensore command received...");
  if (receivedMessage.indexOf(OFF)!=-1)  
    {
      Serial.println("Turning off a carbon sensore!");
      systemConfig["CoSensorEn"] = 0;
      sendMessage(MOCOFF,currentUser);
    }
    else
    {
      Serial.println("Turning on a carbon sensore!");
      systemConfig["CoSensorEn"] = 1;
      sendMessage(MOCON,currentUser);

    }

}

void fireAct(const String& receivedMessage)
{
  Serial.println("A fire sensore command received...");
  if (receivedMessage.indexOf(OFF)!=-1)  
    {
      Serial.println("Turning off a fire sensore!");
      systemConfig["FireSensorEn"] = 0;
      sendMessage(FIREOFF,currentUser);

    }
    else
    {
      Serial.println("Turning on a fire sensore!");
      systemConfig["FireSensorEn"] = 1;
      sendMessage(FIREON,currentUser);
    }
}

void sendSms_act(String sms)
{
  Serial.println("Sms command received...");
  if (sms.indexOf("on") != -1) 
  {
    Serial.println("Start Sending alarm sms...");
    systemConfig["SendMessage"] = 1;
  }
  else if (sms.indexOf("off") != -1)
  {
    Serial.println("Stop Sending alarm sms...");
    systemConfig["SendMessage"] = 0;
  }
}

void make_call(String phonenumber_call ) // new
{
  String phonenumber_str = "ATD+ " + phonenumber_call + ";";
  Serial2.println(phonenumber_str.c_str());
  updateSerial();
}

bool waitForCallResponse() {
  unsigned long startTime = millis();
  String responseLine = "";
  bool callEnd = false;

  Serial.println("Waiting for call response...");

  while (!callEnd && millis() - startTime < 30000) { // Extend wait time to 30 seconds if needed
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n') { // End of line character
        Serial.print("Response: "); Serial.println(responseLine); // Debug print

        if (responseLine.indexOf("+CLCC: 1,0,0,0,0,") != -1) {
          callEnd = true;
          return callEnd;
        }
        responseLine = ""; // Reset for the next line
      } else if (c != '\r') { // Ignore carriage return
        responseLine += c; // Build the line
      }
    }
  }

  if (!callEnd) {
    Serial.println("No call response received within the timeout period.");
    return false;
  }
  return false;
}

bool checkForDeliveryReport() 
{
  unsigned long startTime = millis();
  while (millis() - startTime < waitingDeliverTime) {
    if (Serial2.available()) 
    {
      String incomingData = Serial2.readString();
      if (incomingData.indexOf("+CDS:") != -1) {
          Serial.println("Delivery report received:");
          Serial.println(incomingData);
          return true;
      }
    }
  }
  return false;
}

void parsePhoneNumbers(JsonDocument& doc) {
    Serial.println("Parsing phone numbers list");
    bool ContainPhoneNum = false;
    // Get the array of phone numbers
    JsonArray phoneNumbersDoc = doc["PhoneNumbers"].as<JsonArray>();
    
    // go through each object in the array
    for (JsonObject phoneObj : phoneNumbersDoc) 
    {
      // Extract the phone number (key and value) and isAdmin
      for (JsonPair kv : phoneObj) {
        const char* key = kv.key().c_str();
        
        // Check if it's a phone number key (starts with "Number")
        if (strncmp(key, "Number", 6) == 0) {
          String phoneNumber = kv.value().as<String>();
          // Extract the corresponding isAdmin status
          bool isAdmin = phoneObj["isAdmin"].as<bool>();

          if(phoneNumber != "")
          {
            if(!ContainPhoneNum) // to ignore clearing list if no phone number inserted
            {
              //clear phone number lists to save new lists
              phoneNumbers.clear();
              adminPhoneNumbers.clear();
              ContainPhoneNum = true;
            }
            Serial.printf("Saving Phone number: %s admin[%d]\n", phoneNumber, isAdmin);
            if(isAdmin)
              creatAdminPhonenumber(phoneNumber);
            else
              creatPhonenumber(phoneNumber);
          }
        }
      }
    }
    Serial.println("---normal phone list----");
    serializeJson(phoneNumbers, Serial);
    Serial.println("\n---admin phone list----");
    serializeJson(adminPhoneNumbers, Serial);
    Serial.println();
    saveJson(adminPhoneNumsPath, adminPhoneNumbers);
    saveJson(phoneNumsPath, phoneNumbers);
}

void CheckSensors(void *param)
{
  //esp_task_wdt_add
  while(1)
  {
    //esp_task_wdt_reset();
    if(millis() - lastSensorCheckTime > SensorsInterval)
    {
      lastSensorCheckTime = millis();

      MQ2Read(FIREPIN);
      MQ7Read();
 //     CheckCNY70();
   //   BMP280Values();
      CheckMotion();

      if(BatteryInterval >= CheckBatteryInterval)
      {

        // ReadDHT();

        BatteryInterval = 0;
        CheckBattery();
      }
      BatteryInterval++;
    
      String message;
      serializeJson(SensorsValueJson, message);
      notifyClients(message);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Yield control 100ms
  }
}

void CheckCNY70()
{
  //esp_task_wdt_reset();
  bool ProtectionValue =  digitalRead(ProtectionKey);
  Serial.print("The CNY70 Value = ");Serial.println(ProtectionValue);
  if(!ProtectionValue)
  {
    Serial.printf("Warning sensor was removed from the ceiling!\n");
    // String NewLog = "(" + rtc.getDateTime() + ") Warning sensor was removed from the ceiling!";
    // SendLog(NewLog);
    // Serial.println("Sending Warning SMS to all contacts");
    // lastAlarmTime = millis();
    // Serial.println(CeileRemoved);
    // sendToAll(CeileRemoved);
  }
}

bool checkPIR()
{
  //esp_task_wdt_reset();
  int pirState = digitalRead(pirPin);
  if (pirState == HIGH)
  {
    Serial.printf("[PIR] Motion Detected!\n");
    return true;
  }
  else
  {
    return false;
  }
    
}

void CheckBattery()
{
  //esp_task_wdt_reset();
  if(!systemConfig["AcStatus"].as<bool>())
  {
    long sum = 0;
    for (int i = 0; i < 100; i++) {
      sum += analogRead(33);
      delay(1);
    }
    sum = sum / 100;
    float voltage = (sum * 3.3 ) / 4095 ;  // Assuming a 3.3V reference voltage
    voltage = (voltage * 30) / 20;
    // Apply calibration factor
    float calibrationFactor = 2.9 / 2.78; // Adjust based on your measurements
    float correctedVoltage = voltage * calibrationFactor;

    Serial.print("Battery Voltage sum: ");
    Serial.println(correctedVoltage, 3);  // Print voltage with 3 decimal places
    // convert to percentage
    float minVoltage = MinBatvalue; // 0% corresponds to 2.53V
    float maxVoltage = MaxBatvalue;  // 100% corresponds to 2.8V

    // Constrain voltage to the range [minVoltage, maxVoltage]
    correctedVoltage = constrain(correctedVoltage, minVoltage, maxVoltage);

    // Map voltage to percentage
    int percentage = ((correctedVoltage - minVoltage) / (maxVoltage - minVoltage)) * 100.0;
    Serial.printf("Battery percentage : %d\n", percentage);
    SensorsValueJson["Power"] = String(percentage);
  }
  else
    SensorsValueJson["Power"] = "AC - 100";
}

void MQ2Read(int MQ2Pin)
{
  //esp_task_wdt_reset();
  int sensorValue = map(analogRead(MQ2Pin), 0, 4095, 0, 255); 
  delay(100);
  sensorValue += map(analogRead(MQ2Pin), 0, 4095, 0, 255); 
  delay(100);
  sensorValue += map(analogRead(MQ2Pin), 0, 4095, 0, 255); 

  sensorValue = sensorValue / 3;
  
  SensorsValueJson["FireSensor"] = sensorValue;
  Serial.printf("Fire Sensor Value: %d\n", sensorValue);

  if(sensorValue >= systemConfig["Firelimit"].as<int>())
  {
    Serial.printf("\nWARNING!!! Fire Sensor(%d) is over than limit %d\n", sensorValue, systemConfig["Firelimit"].as<int>());
    if ((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000) && systemConfig["FireSensorEn"])
    {
      String NewLog = "(" + rtc.getDateTime() + ") Warning Fire Sensor Value = " + SensorsValueJson["FireSensor"].as<String>();
      SendLog(NewLog);
      Serial.println("Sending Warning SMS to all contacts");
      lastAlarmTime = millis();
      String  fireMessage = FIRE1 + String(sensorValue) + FIRE2;
      Serial.println(fireMessage);
      sendToAll(fireMessage);
      // CreditCheckCounter++;
    }
  }

}

/*void BMP280Values() {
  if (bmp.takeForcedMeasurement()) {
    double temp = bmp.readTemperature();//, 1).toDouble();
    int pressure = bmp.readPressure() / 100.0F;

    Serial.print("BMP Temperature = ");
    Serial.print(temp);
    Serial.println(" *C");
    
    Serial.print("BMP Pressure = ");
    Serial.print(pressure);
    Serial.println(" hPa");

    SensorsValueJson["Temperature"] = temp;
    SensorsValueJson["Pressure"]    = pressure;

    if(temp >= systemConfig["Templimit"].as<int>())
    {
      Serial.printf("\nWARNING!!! temp(%lf) is over than limit %d\n", temp, systemConfig["Templimit"].as<int>());
      if ((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000) && systemConfig["TempSensorEn"])
      {
        String NewLog = "(" + rtc.getDateTime() + ") Warning Temp Value = " + SensorsValueJson["Temperature"].as<String>();
        SendLog(NewLog);
        Serial.println("Sending Warning SMS to all contacts");
        lastAlarmTime = millis();
        String  temperatureMessage = TEMP1 + String(temp) +TEMP2;
        Serial.println(temperatureMessage);
        sendToAll(temperatureMessage);
        CreditCheckCounter++;
      }
    }
  }
  Serial.println();
}*/

void ReadDHT() {
  Serial.println(F("reading Humidity"));
  int h = dht.readHumidity();
  vTaskDelay(pdMS_TO_TICKS(1)); // Yield briefly
  Serial.println(F("reading Temperature"));
  int t = dht.readTemperature();
  vTaskDelay(pdMS_TO_TICKS(1)); // Yield again
  Serial.println(F("checking dht values"));
  if (isnan(h) || isnan(t) || h > 100 || t > 100) {
    Serial.println(F("Failed to read from DHT sensor!"));
  } else {
    Serial.printf("DHT Temp: %d, Humidity: %d\n", t, h);
    if (h < 120) {
      SensorsValueJson["Humidity"] = h;
      SensorsValueJson["Temperature"] = t;
    }
  }
}

void MQ7Read()
{
  int COValue = mq7.getPPM();
  Serial.printf("\nCo Value = %d\n", COValue);
  SensorsValueJson["Co"] = COValue;
  //esp_task_wdt_reset();
  if(COValue >= systemConfig["Colimit"].as<int>())
  {
    Serial.printf("\nWARNING!!! COValue(%d) is over than limit %d\n", COValue, systemConfig["Colimit"].as<int>());
    if ((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000) && systemConfig["CoSensorEn"])
    {
      String NewLog = "(" + rtc.getDateTime() + ") Warning Co Value = " + SensorsValueJson["Co"].as<String>();
      SendLog(NewLog);
      Serial.println("Sending Warning SMS to all contacts");
      lastAlarmTime = millis();
      String  COMessage = CO1 + String(COValue) + FIRE2;
      Serial.println(COMessage);
      sendToAll(COMessage);
      // CreditCheckCounter++;
    }
  }
}

// bool checkMW()
// {
//   int MWState = digitalRead(MWPin);
//   if (MWState == HIGH)
//   {
//     // MWMotionCounter++;
//     Serial.printf("[MW] Motion Detected!\n");
//     return true;
//   }
//   else
//   {
//     return false;
//   }  
// }

void SendLog(String LogMessage)
{
  JsonDocument Log;
  addLog(LogMessage);
  Log["Log"] = LogMessage;
  String LogString;
  serializeJson(Log, LogString);
  notifyClients(LogString);
  saveJson(LastLogsPath ,SystemLog);
}

void Buzzer()
{
  ledcWriteTone(TONE_PWM_CHANNEL, 500);
  delay(100);    
  ledcWrite(TONE_PWM_CHANNEL, 0); 
  delay(50);
  ledcWriteTone(TONE_PWM_CHANNEL, 1000);
  delay(80);    
  ledcWrite(TONE_PWM_CHANNEL, 0);
}

void CheckMotion()
{
  if(millis() - lastSensorDoubleCheck > SensorDoubleCheckTime) // reset sensor timer 
  {
    MotionDetection = 0;
    Serial.println("<><><><><><><><><> Resetting Motion Detection <><><><><><><><>");
  }
  bool PIRState = checkPIR();
  // bool MWState  = checkMW();
  // if(PIRState && !MWState)
  //   SensorsValueJson["Motion"] = "PIR";
  // else if(!PIRState && MWState)
  //   SensorsValueJson["Motion"] = "MW";
  // else if(!PIRState && !MWState)
  //   SensorsValueJson["Motion"] = "--";
  // else if(PIRState && MWState)
  if(!PIRState)
    SensorsValueJson["Motion"] = "--";
  if(PIRState)
  {
    MotionDetection++;
    lastSensorDoubleCheck = millis();
    
    // SensorsValueJson["Motion"] = "PIR + MW";
    SensorsValueJson["Motion"] = "Detected";
    Serial.printf("\nWARNING!!! Motion detected with pir and mw[%d]\n", MotionDetection);

    if((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000) && systemConfig["SystemStatus"] && MotionDetection >= Motionthr)
    {
        MotionDetection = 0;
        lastSensorDoubleCheck = millis();
        lastAlarmTime = millis();
        delay(20);
        AlarmRGB = true;
        LedCode = 1;//blink Red

        String NewLog = "(" + rtc.getDateTime() + ") Motion Detected!";
        SendLog(NewLog);

        Serial.println("Danger!!!!! >> Sending sms...");
        sendToAll(DANGER);
        // CreditCheckCounter++;
    }
  }
}

void RGBTask(void *param)
{
  //esp_task_wdt_add //add current thread to WDT watch
  while(1)
  {
    //esp_task_wdt_reset();
    // if(AlarmRGB && systemConfig["RgbRing"].as<int>())
    if(AlarmRGB)
    {
      if(millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000)
      {
        AlarmRGB = false;
        if(systemConfig["SystemStatus"].as<bool>())
          FullColor((CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16), NUM_LEDS, leds);
        else
          FullColor((CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16), NUM_LEDS, leds);
      }
      else
      {
        switch (LedCode)
        {
        case 1: //Red alarm blink;
          BlinkColor((CRGB)strtol(systemConfig["AlarmColor"], NULL, 16), NUM_LEDS, 500, 1, leds);
          break;
        case 2:
          BlinkColorSmooth(CRGB::Red, NUM_LEDS, 1,20, leds);
          break;
        case 3:
          BlinkColor(CRGB::Yellow, NUM_LEDS, 500, 1, leds);
          break;
        case 4:
          BlinkColorSmooth(CRGB::Yellow, NUM_LEDS, 1,20, leds);
          break;

        default:
          FullColor(DefaultColor, NUM_LEDS, leds);
          break;
        }
      }
    }
    // else if(!systemConfig["RgbRing"].as<int>())
    //   FullColor(CRGB::Black, NUM_LEDS, leds);

    vTaskDelay(10 / portTICK_PERIOD_MS); // Yield control 10ms
  }
}

void TimeThread(void *param)
{
  //esp_task_wdt_add //add current thread to WDT watch
  while(1)
  {
    //esp_task_wdt_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS); // Yield control 500ms
    // if(WifiConnected)
    // {
      UpdateTime();
      String Time = rtc.getDateTime();
      JsonDocument TimeStamp;
      TimeStamp["Time"] = Time;
      String TimeJsonString;
      serializeJson(TimeStamp, TimeJsonString);
      notifyClients(TimeJsonString);
    // }
  }
}

void HardRestartSim()
{
  digitalWrite(Simpower,0);
  delay(500);
  digitalWrite(Simpower,1);
  delay(500);
  digitalWrite(Simpower,0);
  delay(500);
  digitalWrite(Simpower,1);
  delay(500);
}

void AlarmSound()
{
  for(int hz = 440; hz < 1000; hz+=25){
    tone(SpekaerPin, hz, 50);
    delay(5);
  }
  // Whoop down
  for(int hz = 1000; hz > 440; hz-=25){
    tone(SpekaerPin, hz, 50);
    delay(5);
  }
}

void AlarmSpeaker(void *param)
{ 
  Serial.println("***** Speaker Task Started ******");
  bool alarmStart = false;
  while(true)
  {
    if(AlarmRGB) {
      
      if(millis() - lastAlarmTime < systemConfig["AlarmDuration"].as<int>() * 1000)
      { 
        if(!alarmStart)
        {
          Serial.println("Starting alarm!!!");
          alarmStart = true;
        } 
        AlarmSound();
      }
      else
      {
        if(alarmStart)
        {
          Serial.println("Stoping alarm!");
          alarmStart = false;
        }
        delay(100);
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void WifiReconnect() {

  WiFi.disconnect();
  delay(1000);
  if(ssidSPIFF != "")
  {
    if(initWiFi()) 
    {
      Serial.println("WiFi Connected");
    } 
  }
  if(!WifiConnected) 
  {
      // Connect to Wi-Fi network with SSID and password
      Serial.println("Setting AP (Access Point)");
      // NULL sets an open Access Point
      WiFi.softAP("Didomak-WIFI-MANAGER", NULL);

      IPAddress IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);   
      LinearColorFill(CRGB(76,0,153), NUM_LEDS, leds);
  }
}