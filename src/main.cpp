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
#include <soc/rtc_cntl_reg.h>

/* Private define ------------------------------------------------------------*/
#define RX2D2 16
#define TX2D2 17

#define RELAY_PIN 5

/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void fSim800_CommandHandler(sSim800RecievedMassgeDone *pArgs);
static void fCommand_lampAct(const String receivedMessage);

/* Variables -----------------------------------------------------------------*/

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                          ##### SETUP #####                                      ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/
void setup() {

  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RX2D2, TX2D2);
  Serial.println("Power on");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

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
  fSim800_SMSSendToAll("Test massage");
}

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                          ##### LOOP #####                                       ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/

void loop() {

  fSim800_Run();
  delay(2000);
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
      break;
    }

    case eLAMP_COMMAND: {

      fCommand_lampAct(pArgs->MassageData.Massage);
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
static void fCommand_lampAct(const String receivedMessage) {

  if (receivedMessage.indexOf(OFF) != -1) {
    Serial.println("Turning off lamp...");
    digitalWrite(RELAY_PIN, LOW);
    fSim800_SMSSendToAll(LAMPOFF);
    // fSim800_SMSSend("09024674437" ,LAMPOFF);

  } else if(receivedMessage.indexOf(ON) != -1) {

    Serial.println("Turning on lamp!");
    digitalWrite(RELAY_PIN, HIGH);
    // fSim800_SMSSendToAll(&Ssim800, LAMPON);
    fSim800_SMSSend("09024674437" ,LAMPON);
  }
}