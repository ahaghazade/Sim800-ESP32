/**
 ******************************************************************************
 * @file           : main.c
 * @brief          :
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 DiodeGroup.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component
 *
 *
 ******************************************************************************
 * @verbatim
 * @endverbatim
 */

/* Includes ------------------------------------------------------------------*/
#include <Arduino.h>

#include "Sim800_cdrv.h"
#include "ws2812_cdrv.h"
#include "ld2420_cdrv.h"

#include <soc/rtc_cntl_reg.h>
#include <WiFi.h> 
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>
#include "ESP32Time.h"

/* Private define ------------------------------------------------------------*/
#define RX1D1 18
#define TX1D1 25

#define RX2D2 16
#define TX2D2 17

#define RELAY_PIN 5
#define HEARTBIT_PIN 12
#define fBUZZER_PIN 13

#define SERVER_PORT 80
#define WIFI_CHECKCONNECTION 5

#define INIT_SPIFF_TRY 3

#define RGB_NUM_LEDS 52
#define RGB_BRIGHTNESS  10
#define RGB_FULL_BRIGHTNESS 40
#define RGB_DATA_PIN 19

#define LD2420_PIN 22
#define Motionthr 3

#define WIFI_CONNECTION_INTERVAL  10000
#define SENSOR_CHECK_INTERVAL_MS  2000
#define SensorDoubleCheckTime     10000
#define ALARM_INTERVAL_MS         20000
#define HEARTBIT_INTERVAL_MS      1000
#define SIM800_CHECK_INTERVAL_MS  3000


/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static JsonDocument        systemConfig;
static JsonDocument        SensorsValueJson;
static JsonDocument        LoginPass;
static JsonDocument        SystemLog;
static JsonDocument        RebootCount;
static JsonDocument        WifiConfig;
static JsonDocument        TimeJsonDoc;
static const char* SystemStatusPath    = "/SystemStatus.json";
static const char* RelayClockPath      = "/Clock.json";
static const char* LoginPassPath       = "/LoginPass.json";
static const char* LastLogsPath        = "/LastLogs.json";
static const char* RebootCounterPath   = "/BootCount.json";
static const char* WifiConfigPath      = "/WifiConfig.json";
static const char* TimeJsonDocPath      = "/TimeJsonDoc.json";

static const char* PARAM_INPUT_1 = "ssid";
static const char* PARAM_INPUT_2 = "pass";
static const char* PARAM_INPUT_3 = "ip";
static const char* PARAM_INPUT_4 = "NodeID";
static String ssidSPIFF;
static String passSPIFF;
static String MQQTipSPIFF;
static String DeviceNameSPIFF;

static const char* correctUsername = "admin";
static bool isAuthenticated = true; // Flag to check if the user is logged in

//websocket and webserver
static AsyncWebServer server(SERVER_PORT);
static const char* PARAM_MESSAGE = "message";
static IPAddress localIP;
static AsyncWebSocket ws("/ws");
static String WSmessage = "";
static JsonDocument SocketClients;

//wifi
static bool WifiConnected = false;
static volatile int WifiNotConnectedCount = 1;
static unsigned long previousMillisWifi = 0;
static unsigned long prevWIFI_CHECKCONNECTIONTime = 0;
static const long WIFI_CHECKCONNECTIONInterval = 10000;

//Buzzer
static const int TONE_PWM_CHANNEL = 0;

//Config Time
static ESP32Time rtc(0);  // offset in seconds GMT+1
static const char* ntpServer = "pool.ntp.org";
static const long  gmtOffset_sec = 3.5 * 60 * 60;
static const int   daylightOffset_sec = 0;
static int WeekDay = 0; //0 Sun - 6 Sat
static int TimeSec = 0;
static int TimeH = 0; //0-23
static int TimeMin = 0;
static int year  = 2025;
static int month = 1;
static int day = 1;

//led
static sWs2812 ws2812;
unsigned long PreviousMillisHeartBit = 0;

//tasks handler
TaskHandle_t fRGBTaskHandler;

//senosrs
static unsigned long previousMilliSensorCheck = 0;
static int MotionDetection = 0;
static unsigned long lastAlarmTime;
static unsigned long lastSensorDoubleCheck;

static unsigned long previousMilliSim800Check = 0;

/* Private function prototypes -----------------------------------------------*/
static void fSim800_CommandHandler(sSim800RecievedMassgeDone *pArgs);
static void fCommand_SystemAct(const String receivedMessage);
static void fCommand_IpAct(const String receivedMessage);
static void fCommand_LampAct(const String receivedMessage);
static void fCommand_AlarmAct(const String receivedMessage);
static void fCommand_fireAct(const String receivedMessage);
static void fCommand_CarbonSensorAct(const String receivedMessage);
static void fCommand_TempSensorAct(const String receivedMessage);
static void fCommand_HumiditySensorAct(const String receivedMessage);

static void fInitWebSocket(void);
static void fHandleWebSocketMessage(void *arg, uint8_t *data, size_t len);
static void fNotifyClients(String text);
static void fOnEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);

static void fSaveJson(const char* filename, JsonDocument& doc);
static void fLoadJson(const char* filename, JsonDocument& doc);

static bool fWiFi_Init(void);
static void fStartServers(void);
static void fAddLog(String newLog);
static void fSendLog(String LogMessage);

static void fInitSPIFFS(void);
static void fSystemConfigInitial(void);

static void fParsePhoneNumbers(JsonDocument& doc);
static void fConvertToOriginalFormat(JsonDocument *pPhoneNumbersDoc, JsonDocument *pOriginalDoc);

static void fBuzzer(void);
static void fRGBTask(void *param);
static bool fCheckLd2420(void);
static void fCheckMotion(void);

/* Variables -----------------------------------------------------------------*/

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                          ##### SETUP #####                                      ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/
void setup() {

//------------- init serials -------------
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1D1, TX1D1);
  Serial2.begin(115200, SERIAL_8N1, RX2D2, TX2D2);

  Serial.println("Power on");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

//------------- init IO-------------
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(HEARTBIT_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  ledcAttachPin(fBUZZER_PIN, TONE_PWM_CHANNEL);

//------------- init sensros ----------
  if(fLd2420_Init() != LD2420_RES_OK) {
    Serial.println("fLd2420 init failed!");
  }

  delay(500);
  fLd2420_ConfigABDParams(SetDelayTime,2);
//------------- init spiff -------------
  fInitSPIFFS();

  fLoadJson(SystemStatusPath, systemConfig);
  fLoadJson(LoginPassPath, LoginPass);
  SystemLog["logs"] = JsonArray();
  fLoadJson(LastLogsPath, SystemLog);
  fLoadJson(RebootCounterPath, RebootCount);
  fLoadJson(WifiConfigPath, WifiConfig);
  fLoadJson(TimeJsonDocPath, TimeJsonDoc);

  if(!TimeJsonDoc.containsKey("day"))
  {
    TimeJsonDoc["hour"] = 18;
    TimeJsonDoc["minute"] = 50;
    TimeJsonDoc["seconds"] = 0;
    TimeJsonDoc["day"] = 24;
    TimeJsonDoc["year"] = 2025;
    TimeJsonDoc["month"] = 6;
    fSaveJson(TimeJsonDocPath, TimeJsonDoc);
  }
  
  if(!WifiConfig.containsKey("ssid"))
  {
    WifiConfig["ssid"] = "";
    fSaveJson(WifiConfigPath, WifiConfig);
  }
  if(!WifiConfig.containsKey("pass"))
  {
    WifiConfig["pass"] = "";
    fSaveJson(WifiConfigPath, WifiConfig);
  }

  ssidSPIFF = WifiConfig["ssid"].as<String>();
  passSPIFF = WifiConfig["pass"].as<String>();

  if(!LoginPass.containsKey("Pass"))
  {
    LoginPass["Pass"] = "pass";
    fSaveJson(LoginPassPath, LoginPass);
  }
  else if(LoginPass["Pass"] == "")
  {
    LoginPass["Pass"] = "pass";
    fSaveJson(LoginPassPath, LoginPass);
  }

  if(!RebootCount.containsKey("Count"))
  {
    RebootCount["Count"] = 1;
    fSaveJson(RebootCounterPath, RebootCount);
  }
  else
  {
    RebootCount["Count"] = RebootCount["Count"].as<int>() + 1;
    fSaveJson(RebootCounterPath, RebootCount);
  }

  Serial.println("=====Saved Wifi Config=====");
  serializeJsonPretty(WifiConfig, Serial);
  Serial.println();

  Serial.println("=====Saved systemConfig=====");
  serializeJsonPretty(systemConfig, Serial);
  Serial.println();
  fSystemConfigInitial();

  fLd2420_ConfigABDParams(SetMaxDistance, systemConfig["RadarDistance"].as<int>()); 

  lastAlarmTime  = millis() - systemConfig["AlarmDuration"].as<int>() * 1000; //stop alarm
  digitalWrite(HEARTBIT_PIN, systemConfig["LampStatus"].as<int>());

  Serial.println("=====Saved Login Pass=====");
  serializeJsonPretty(LoginPass, Serial);
  Serial.println();

  Serial.println("=====Saved Last Logs=====");
  serializeJsonPretty(SystemLog, Serial);
  Serial.println();

//------------- rgb ring -------------
  ws2812.LedNum = RGB_NUM_LEDS;
  ws2812.Effect = FillColor;
  ws2812.BetweanEffectDelayMs = (uint32_t)250;
  ws2812.Brightness = systemConfig["RgbRing"].as<int>() * RGB_FULL_BRIGHTNESS / 100;
  ws2812.Init = false;
  ws2812.DataPin = RGB_DATA_PIN;

  if(fWs2812_Init(&ws2812) != WS2812_RES_OK) {

    Serial.println("ws2812_17 init Failed!!!");
  }

  if(systemConfig["SystemStatus"].as<bool>()) {
    ws2812.MainColor = (CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16);
  } else {
    ws2812.MainColor = (CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16);
  }
  
  // ws2812.Effect = FillColor;
  // ws2812.Color = CRGB::Black;
  // fWs2812_Run(&ws2812);
  ws2812.Effect = Fancy;
  ws2812.Color = CRGB::Green;
  // fWs2812_Run(&ws2812);
  xTaskCreatePinnedToCore(fRGBTask, "RGB_Task", 4096, NULL, 2, &fRGBTaskHandler, 1);
  delay(5000);
//------------- init wifi and time -------------
  bool GetTimeFlag = false;

  if(ssidSPIFF != "")
  {
    if(fWiFi_Init()) {
      Serial.println("WiFi Connected");
      // Init and get the time
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      bool GetTimeFlag = false;

      TimeH   = 18;
      TimeMin = 17;
      TimeSec = 0;

      year  = 2025;
      month = 6;
      day   = 18;
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

    ws2812.Effect = LinearFill;
    ws2812.Color = CRGB(76,0,153);
    // fWs2812_Run(&ws2812);
  }
  // fWs2812_Run(&ws2812);

  if(!GetTimeFlag)
  {
    Serial.println("Load time from last connection");
    TimeH   = TimeJsonDoc["hour"].as<int>();
    TimeMin = TimeJsonDoc["minute"].as<int>();
    TimeSec = TimeJsonDoc["seconds"].as<int>();

    year  = TimeJsonDoc["year"].as<int>();
    month = TimeJsonDoc["month"].as<int>();
    day   = TimeJsonDoc["day"].as<int>();
    rtc.setTime(TimeSec, TimeMin, TimeH, day, month, year); //second,min,hour,day,month,year
  }
  // printLocalTime();

  Serial.println("----------8");
  digitalWrite(HEARTBIT_PIN, LOW);
  fStartServers();

  SensorsValueJson["Temperature"] = -1;
  SensorsValueJson["Pressure"]    = -1;
  SensorsValueJson["Humidity"]    = -1;
  SensorsValueJson["Co"]          = -1;
  SensorsValueJson["FireSensor"]  = -1;
  SensorsValueJson["Credit"]      = -1;
  SensorsValueJson["Power"]       = "-";

//------------- init sim800 -------------
  Sim800.ComPort = &Serial2;
  Sim800.CommandSendRetries = 3;
  Sim800.EnableDeliveryReport = true;

  if(fSim800_Init() != SIM800_RES_OK) {
    ESP.restart();
  }

  if(fSim800_RegisterCommandEvent(fSim800_CommandHandler) != SIM800_RES_OK) {
    ESP.restart();
  }

  Serial.println("Sim800 initial success");
  Sim800.Init = true;

  fSim800_RemovePhoneNumber("09127176496");
  fSim800_AddPhoneNumber("09024674437", 0);

  Serial.println("Saved phone numbers:");
  serializeJsonPretty(Sim800.SavedPhoneNumbers, Serial);
  Serial.println();

  JsonDocument PhoneList;
  fSim800_GetPhoneNumbers(&PhoneList);
  Serial.println("=======Get phone List===========");
  serializeJsonPretty(PhoneList, Serial);
  Serial.println();

  // fSim800_SMSSendToAll(STARTUP_MSG);

}

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                          ##### LOOP #####                                       ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/

void loop() {

  if(millis() - previousMilliSim800Check > SIM800_CHECK_INTERVAL_MS)
  {
    fSim800_Run();
    previousMilliSim800Check = millis();
  }

  if(millis() - previousMilliSensorCheck > SENSOR_CHECK_INTERVAL_MS)
  {
    fCheckMotion();
    previousMilliSensorCheck = millis();
  }

  if(millis() - PreviousMillisHeartBit > HEARTBIT_INTERVAL_MS)
  {
    digitalWrite(HEARTBIT_PIN, !digitalRead(HEARTBIT_PIN));
    PreviousMillisHeartBit = millis();
  }
}

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                            ##### Private Functions #####                        ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/
/**
 * @brief 
 * 
 * @param sender 
 * @param pArgs 
 */
static void fSim800_CommandHandler(sSim800RecievedMassgeDone *pArgs) {


  Serial.printf("Command recived from %s, and command %d\n", pArgs->MassageData.phoneNumber, pArgs->CommandType);
  
  switch(pArgs->CommandType) {

    case eSYSTEM_COMMAND: {

      fCommand_SystemAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eLAMP_COMMAND: {

      fCommand_LampAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eIP_COMMAND: {

      fCommand_IpAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eALARM_COMMAND: {

      fCommand_AlarmAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eFIRE_COMMAND: {

      fCommand_fireAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eMONIXIDE_COMMAND: {

      fCommand_CarbonSensorAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eTEMP_COMMAND: {

      fCommand_TempSensorAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    case eHUMIDITY_COMMAND: {

      fCommand_HumiditySensorAct(pArgs->MassageData.Massage);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
      fSaveJson(SystemStatusPath, systemConfig);
      fBuzzer();
      break;
    }

    default: {
      break;
    }
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_SystemAct(const String receivedMessage) {

  Serial.println("A system command received...");
  if (receivedMessage.indexOf(OFF) != -1) 
  {
    Serial.println("Turning off system...");
    systemConfig["SystemStatus"] = 0;
    fSim800_SMSSendToAll(SYSTEMOFF);

  }
  else if (receivedMessage.indexOf(ON) != -1)
  {
    systemConfig["SystemStatus"] = 1;
    Serial.println("Turning on system!");
    fSim800_SMSSendToAll(SYSTEMON);
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_IpAct(const String receivedMessage) {

  Serial.println("IP command received.");
  Serial.println("returning system ip...");
  String ipString = WiFi.localIP().toString();
  ipString.replace("." , ",");
  fSim800_SMSSendToAll("Didomak IP: " + ipString);
}


/**
 * @brief 
 * 
 * @param receivedMessage 
 */
static void fCommand_LampAct(const String receivedMessage) {

  if (receivedMessage.indexOf(OFF) != -1) {
    Serial.println("Turning off lamp...");
    systemConfig["LampStatus"] = 0;
    digitalWrite(RELAY_PIN, LOW);
    fSim800_SMSSendToAll(LAMPOFF);

  } else if(receivedMessage.indexOf(ON) != -1) {

    Serial.println("Turning on lamp!");
    systemConfig["LampStatus"] = 1;
    digitalWrite(RELAY_PIN, HIGH);
    fSim800_SMSSendToAll(LAMPON);
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_AlarmAct(const String receivedMessage) {

  Serial.println("A alarm command received...");

  if (receivedMessage.indexOf(OFF) != -1) {

  Serial.println("Turning off alarm!");
   systemConfig["AlarmStatus"] = 0;
   fSim800_SMSSendToAll(ALARMOFF); 
   
  } else {

    Serial.println("Turning on a alarm!");
    systemConfig["AlarmStatus"] = 1;
    fSim800_SMSSendToAll(ALARMON);
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_fireAct(const String receivedMessage) {

  Serial.println("A fire sensore command received...");
  if(receivedMessage.indexOf(OFF)!=-1) {

    Serial.println("Turning off a fire sensore!");
    systemConfig["FireSensorEn"] = 0;
    fSim800_SMSSendToAll(FIREOFF);

  } else {

    Serial.println("Turning on a fire sensore!");
    systemConfig["FireSensorEn"] = 1;
    fSim800_SMSSendToAll(FIREON);
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_CarbonSensorAct(const String receivedMessage) {

  Serial.println("A carbon sensore command received...");
  if(receivedMessage.indexOf(OFF)!=-1) {

    Serial.println("Turning off a carbon sensore!");
    systemConfig["CoSensorEn"] = 0;
    fSim800_SMSSendToAll(MOCOFF);
  } else {

    Serial.println("Turning on a carbon sensore!");
    systemConfig["CoSensorEn"] = 1;
    fSim800_SMSSendToAll(MOCON);
  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_TempSensorAct(const String receivedMessage) {

  Serial.println("A temp sensore command received...");

  if(receivedMessage.indexOf(OFF)!=-1) {

    Serial.println("Turning off a temp sensore!");
    systemConfig["TempSensorEn"] = 0;
    fSim800_SMSSendToAll(TEMPOFF);
  } else {
    
    Serial.println("Turning on a temp sensore!");
    systemConfig["TempSensorEn"] = 1;
    fSim800_SMSSendToAll(TEMPON);

  }
}

/**
 * @brief 
 * 
 * @param receivedMessage 
 */
void fCommand_HumiditySensorAct(const String receivedMessage) {

  Serial.println("A humidity sensore command received...");
  if(receivedMessage.indexOf(OFF)!=-1) {

    Serial.println("Turning off a humidity sensore!");
    systemConfig["FireSensorEn"] = 0;
    fSim800_SMSSendToAll(HUMIDITYOFF);
  } else {

    Serial.println("Turning on a humidity sensore!");
    systemConfig["FireSensorEn"] = 1;
    fSim800_SMSSendToAll(HUMIDITYON);
  }
}


static void fInitWebSocket(void) {

  ws.onEvent(fOnEvent);
  server.addHandler(&ws);
}

static void fHandleWebSocketMessage(void *arg, uint8_t *data, size_t len) {

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
        digitalWrite(RELAY_PIN, lampState);
      }
    }
    if (doc.containsKey("System")) {

      int systemState = doc["System"];


     if(systemState == 0) {

        lastAlarmTime  = millis() - systemConfig["AlarmDuration"].as<int>() * 1000; //stop alarm

        fSim800_SMSSendToAll(ALARMOFF);
      } else {
        fSim800_SMSSendToAll(ALARMON);
      }

      Serial.println("System State: " + String(systemState));
      if(systemConfig.containsKey("SystemStatus"))
      {
        systemConfig["SystemStatus"] = systemState;
      }
    }

    if (doc.containsKey("PhoneNumbers"))
    {
      Serial.print("Phone number added to list: ");
      Serial.println(doc["PhoneNumbers"].as<String>());
      fSim800_RemoveAllPhoneNumbers();
      fParsePhoneNumbers(doc);
      JsonDocument SavedPhoneList;
      JsonDocument OriginalFormatPhoneList;
      fSim800_GetPhoneNumbers(&SavedPhoneList);
      fConvertToOriginalFormat(&SavedPhoneList, &OriginalFormatPhoneList);
      String readings;
      serializeJson(OriginalFormatPhoneList, readings);
      fNotifyClients(readings);
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
          fSaveJson(LoginPassPath, LoginPass);
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
      fLd2420_ConfigABDParams(SetMaxDistance, RadarDistanceState);
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

      ws2812.Brightness = rgbRingState * RGB_FULL_BRIGHTNESS / 100;
      fWs2812_SetBrightness(&ws2812);
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

      fBuzzer();

      if(systemConfig["SystemStatus"].as<bool>()) {
        ws2812.MainColor = (CRGB)strtol(systemConfig["SystemOnColor"], NULL, 16);
      } else {
        ws2812.MainColor = (CRGB)strtol(systemConfig["SystemOffColor"], NULL, 16);
      }

      ws2812.Brightness = systemConfig["RgbRing"].as<int>() * RGB_FULL_BRIGHTNESS / 100;
      fWs2812_SetBrightness(&ws2812);

      ws2812.Effect = Blink;
      ws2812.Color = CRGB::White;
      ws2812.Brightness = systemConfig["RgbRing"].as<int>() * RGB_FULL_BRIGHTNESS / 100;
      
      Serial.println("New System config: ");
      serializeJsonPretty(systemConfig, Serial);
      Serial.println();
      fSaveJson(SystemStatusPath, systemConfig);
      String readings;
      serializeJson(systemConfig, readings);
      fNotifyClients(readings);
    }
  }
}

/**
 * @brief 
 * 
 * @param text 
 */
static void fNotifyClients(String text) {
  ws.textAll(text);
}

/**
 * @brief 
 * 
 * @param server 
 * @param client 
 * @param type 
 * @param arg 
 * @param data 
 * @param len 
 */
static void fOnEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {

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
      fHandleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

/**
 * @brief 
 * 
 * @param filename 
 * @param doc 
 */
static void fSaveJson(const char* filename, JsonDocument& doc) {

   File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(doc, file);
  file.close();
}

/**
 * @brief 
 * 
 * @param filename 
 * @param doc 
 */
static void fLoadJson(const char* filename, JsonDocument& doc) {

  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  deserializeJson(doc, file);
  file.close();
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool fWiFi_Init(void) {

    Serial.println("+++++++++++++++*");

  WiFi.mode(WIFI_STA);
      Serial.println("===========");

  if(passSPIFF == "") {
    WiFi.begin(ssidSPIFF.c_str());
  } else {
    WiFi.begin(ssidSPIFF.c_str(), passSPIFF.c_str());
  }
  // WiFi.begin(SSIDWifi.c_str(), PASSWifi.c_str());
  Serial.print("Connecting to WiFi ");
  Serial.print(ssidSPIFF);

  unsigned long currentMillis = millis();
  previousMillisWifi = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    //esp_task_wdt_reset();
    digitalWrite(HEARTBIT_PIN, !digitalRead(HEARTBIT_PIN));

    ws2812.Effect = BlinkSmooth;
    ws2812.Color = CRGB::Blue;
    // fWs2812_Run(&ws2812);
    
    currentMillis = millis();
    if (currentMillis - previousMillisWifi >= WIFI_CONNECTION_INTERVAL) {
      Serial.println("Failed to connect.");
      return false;
    }
    Serial.print(".");
    delay(500);
  }
  digitalWrite(HEARTBIT_PIN,LOW);

  Serial.println();
  Serial.println(WiFi.localIP());
  String ipString = WiFi.localIP().toString();
  Serial.println(ipString);
  WifiConnected = true;

  ws2812.Effect = LinearFill;
  
  return true;
}

/**
 * @brief 
 * 
 */
static void fStartServers(void) {

  fInitWebSocket();
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
        digitalWrite(HEARTBIT_PIN, lampState);
        // Save to systemConfig
        systemConfig["LampStatus"] = lampState;
        fSaveJson(SystemStatusPath, systemConfig);

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
          fSaveJson(WifiConfigPath, WifiConfig);
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INPUT_2) {
          passSPIFF = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(passSPIFF);
          WifiConfig["pass"] = passSPIFF;
          fSaveJson(WifiConfigPath, WifiConfig);
        }
      }
    }
    WiFi.disconnect();
    delay(1000);
    if(ssidSPIFF != "")
    {
      fWiFi_Init(); 
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
    }

    // request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + MQQTipSPIFF);
    request->send(SPIFFS, "/Page1.html", "text/html", false);    
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
    
    Serial.println("GetPhones Req:");
    String output;
    JsonDocument SavedPhoneList;
    JsonDocument OriginalFormatPhoneList;
    fSim800_GetPhoneNumbers(&SavedPhoneList);
    fConvertToOriginalFormat(&SavedPhoneList, &OriginalFormatPhoneList);
    serializeJson(OriginalFormatPhoneList , output); // Serialize the JSON document to a String
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/LogNum", HTTP_GET, [](AsyncWebServerRequest *request){
    fLoadJson(SystemStatusPath, systemConfig);
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
    fLoadJson(SystemStatusPath, systemConfig);
    Serial.println("systemConfig req:");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
    String output;
    serializeJson(systemConfig, output); // Serialize the JSON document to a String
    request->send(200, "application/json", output); // Send the serialized String
  });

  server.on("/auth", HTTP_POST, [](AsyncWebServerRequest *request){
    fLoadJson(LoginPassPath, LoginPass);
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
        fLoadJson(LastLogsPath, RebootCount);
        String jsonResponse;
        serializeJson(SystemLog, jsonResponse); // Serialize JSON document to a String
        request->send(200, "application/json", jsonResponse); // Send JSON response
  });

  server.on("/boot", HTTP_GET, [](AsyncWebServerRequest *request) {
        fLoadJson(RebootCounterPath, RebootCount);
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
}

/**
 * @brief 
 * 
 * @param newLog 
 */
static void fAddLog(String newLog) {

  while (SystemLog["logs"].size() > systemConfig["LogCount"].as<int>()) {
      SystemLog["logs"].remove(0); // Remove the oldest log if we exceed maxLogs
  }
  SystemLog["logs"].add(newLog); // Add the new log to the end
}

/**
 * @brief 
 * 
 * @param LogMessage 
 */
static void fSendLog(String LogMessage) {

  JsonDocument Log;
  fAddLog(LogMessage);
  Log["Log"] = LogMessage;
  String LogString;
  serializeJson(Log, LogString);
  fNotifyClients(LogString);
  fSaveJson(LastLogsPath ,SystemLog);
}

/**
 * @brief 
 * 
 */
static void fInitSPIFFS(void) {

  int Try = 0;
  bool MountSuccess = false;

  while (Try < INIT_SPIFF_TRY) 
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
  } else {
    ESP.restart();
  }
}

/**
 * @brief 
 * 
 */
static void fSystemConfigInitial(void) {

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
    // systemConfig["fSim800_SMSSend"] = false;
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
    fSaveJson(SystemStatusPath, systemConfig);
  }
  else
  {
    Serial.println("=====System Config Last Status=====");
    serializeJsonPretty(systemConfig, Serial);
    Serial.println();
  }
}

/**
 * @brief 
 * 
 * @param doc 
 */
static void fParsePhoneNumbers(JsonDocument& doc) {

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
            fSim800_RemoveAllPhoneNumbers();
            ContainPhoneNum = true;
          }
          Serial.printf("Saving Phone number: %s admin[%d]\n", phoneNumber, isAdmin);
          if(isAdmin)
            fSim800_AddPhoneNumber(phoneNumber, 1);
          else
            fSim800_AddPhoneNumber(phoneNumber, 0);
        }
      }
    }
  }
}

/**
 * @brief 
 * 
 * @param pPhoneNumbersDoc 
 * @param pOriginalDoc 
 */
static void fConvertToOriginalFormat(JsonDocument *pPhoneNumbersDoc, JsonDocument *pOriginalDoc) {

  int SavedPhoneCount = 0;

  // Clear output doc first
  pOriginalDoc->clear();

  // Create a JSON array to hold phone numbers with isAdmin values
  JsonArray phoneNumbersArray = pOriginalDoc->createNestedArray("PhoneNumbers");

  // Process adminPhoneNumbers and add them to the array
  for (JsonPair kv : pPhoneNumbersDoc->as<JsonObject>()) {
    SavedPhoneCount++;
    JsonObject entry = phoneNumbersArray.createNestedObject();
    entry["Number" + String(SavedPhoneCount)] = kv.key().c_str();
    entry["isAdmin"] = (kv.value().as<int>() == 1);
  }

  // Add placeholders up to Number4
  for (int i = SavedPhoneCount + 1; i <= 4; i++) {
    JsonObject entry = phoneNumbersArray.createNestedObject();
    entry["Number" + String(i)] = "";
    entry["isAdmin"] = false;
  }

  // Debug print
  serializeJsonPretty(*pOriginalDoc, Serial);
}

/**
 * @brief 
 * 
 */
static void fBuzzer(void) {

  ledcWriteTone(TONE_PWM_CHANNEL, 500);
  delay(100);    
  ledcWrite(TONE_PWM_CHANNEL, 0); 
  delay(50);
  ledcWriteTone(TONE_PWM_CHANNEL, 1000);
  delay(80);    
  ledcWrite(TONE_PWM_CHANNEL, 0);
}

/**
 * @brief 
 * 
 * @param param 
 */
static void fRGBTask(void *param) {

  Serial.println("RGB task started");

  while (true) {

    if(millis() - lastAlarmTime < systemConfig["AlarmDuration"].as<int>() * 1000) {
    
      ws2812.Effect = BlinkSmooth;
      ws2812.Color = CRGB::Red;
    }

    fWs2812_Run(&ws2812);

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool fCheckLd2420(void) {

  int ld2420State = digitalRead(LD2420_PIN);
  if (ld2420State == HIGH) {

    Serial.printf("[MW] Motion Detected!\n");
    return true;
  } else {
    return false;
  }
}

/**
 * @brief 
 * 
 */
static void fCheckMotion(void) {

  if(millis() - lastSensorDoubleCheck > SensorDoubleCheckTime) // reset sensor timer 
  {
    MotionDetection = 0;
    Serial.println("<><><><><><><><><> Resetting Motion Detection <><><><><><><><>");
  }
  bool ld2420State = fCheckLd2420();
  if(!ld2420State)
    SensorsValueJson["Motion"] = "--";
  if(ld2420State)
  {
    MotionDetection++;
    lastSensorDoubleCheck = millis();
    
    // SensorsValueJson["Motion"] = "PIR + MW";
    SensorsValueJson["Motion"] = "Detected";
    Serial.printf("\nWARNING!!! Motion detected[%d]\n", MotionDetection);
    Serial.println((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000));

    if((millis() - lastAlarmTime > systemConfig["AlarmDuration"].as<int>() * 1000) && systemConfig["SystemStatus"] && MotionDetection >= Motionthr)
    {
        MotionDetection = 0;
        lastSensorDoubleCheck = millis();
        lastAlarmTime = millis();
        delay(20);

        while(ws2812.IsUpdating) {
          delay(20);
        }
        ws2812.Color = CRGB::Red;
        ws2812.Effect = BlinkSmooth;

        String NewLog = "(" + rtc.getDateTime() + ") Motion Detected!";
        fSendLog(NewLog);

        Serial.println("Danger!!!!! >> Sending sms...");
        fSim800_SMSSendToAll(DANGER);
        // CreditCheckCounter++;
    }
  }
}

/************************ Copyright (c) 2025 DiodeGroup *****END OF FILE****/