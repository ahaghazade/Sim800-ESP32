#include <Arduino.h>

#include "Sim800_cdrv.h"


#define RX2D2 16
#define TX2D2 17

sSim800 Sim800;


void setup() {

  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2D2, TX2D2);

  Sim800.ComPort = &Serial2;
  Sim800.CommandSendRetries = 3;
  Sim800.EnableDeliveryReport = true;

  Serial.print("init sim800 : ");
  Serial.println(fSim800_Init(&Sim800));

  fSim800_AddPhoneNumber(&Sim800, "09127176496", 1);
  fSim800_AddPhoneNumber(&Sim800, "+989024674437", 0);

  Serial.println("Saved phone numbers:");
  serializeJsonPretty(Sim800.SavedPhoneNumbers, Serial);

  // Serial.print("\nsend sms result: ");
  // Serial.println(fSim800_SendSMS(&Sim800, "09127176496", "1Helloooo"));
  // Serial.println(fSim800_SendSMS(&Sim800, "09127176496", "2Helloooo"));

  Serial.println("-------");

}

void loop() {

  fSim800_Run(&Sim800);
  delay(5000);
}