#include <Arduino.h>

#include "Sim800_cdrv.h"

sSim800 Sim800;


void setup() {

  Serial.begin(115200);

  Sim800.Serial = &Serial;
  Serial.print("init sim800 : ");
  Serial.println(fSim800_Init(&Sim800));

  fSim800_AddPhoneNumber(&Sim800, "09127176496", 1);
  fSim800_AddPhoneNumber(&Sim800, "+989024674437", 0);

  Serial.println("Saved phone numbers:");
  serializeJsonPretty(Sim800.SavedPhoneNumbers, Serial);

}

void loop() {


}