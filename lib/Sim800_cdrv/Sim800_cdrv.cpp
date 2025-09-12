/**
 ******************************************************************************
 * @file           : sim800_cdrv.c
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
#include "Sim800_cdrv.h"

#include <SPIFFS.h>
#include <Arduino.h>

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
const char* SavedPhoneNumbersPath = "/PhoneNumbers.json";

/* Private function prototypes -----------------------------------------------*/
static sim800_res_t fLoadPhoneNumbers(JsonDocument *JsonDoc, String path);
static sim800_res_t fSavePhoneNumbers(JsonDocument *JsonDoc, String path);
static sim800_res_t fNormalizedPhoneNumber(String PhoneNumber, String *Normalized);
static sim800_res_t fReadInbox();
static sim800_res_t fClearInbox();

/* Variables -----------------------------------------------------------------*/

/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                          ##### Exported Functions #####                         ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/
/**
 * @brief 
 * 
 * @param me 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_Init(sSim800 * const me) {

  if(me == NULL) {
    return SIM800_RES_INIT_FAIL;
  }

  me->Init = false;

  if (!SPIFFS.begin(true)) { 
    return SIM800_RES_INIT_FAIL;
  }

  if(fLoadPhoneNumbers(&me->SavedPhoneNumbers, SavedPhoneNumbersPath) != SIM800_RES_OK) {
    return SIM800_RES_INIT_FAIL;
  }

  me->EnableSengingSMS = true;
  me->Init = true;

  return SIM800_RES_OK;
}

void fSim800_Run(sSim800 * const me) {

  if (me == NULL || !me->Init) {
    return;
  }

}

/**
 * @brief 
 * 
 * @param me 
 * @param PhoneNumber 
 * @param IsAdmin 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_AddPhoneNumber(sSim800 * const me, String PhoneNumber, bool IsAdmin) {

  if(me == NULL || !me->Init) {
    return SIM800_RES_INIT_FAIL;
  }

  String NormalizedPhoneNumber;
  if(fNormalizedPhoneNumber(PhoneNumber, &NormalizedPhoneNumber) != SIM800_RES_OK) {
    return SIM800_RES_PHONENUMBER_INVALID;
  }

  me->SavedPhoneNumbers[NormalizedPhoneNumber] = IsAdmin? 1 : 0;
  fSavePhoneNumbers(&me->SavedPhoneNumbers, SavedPhoneNumbersPath);

  return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param me 
 * @param PhoneNumber 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_RemovePhoneNumber(sSim800 * const me, String PhoneNumber) {

  if (me == NULL || !me->Init) {
    return SIM800_RES_INIT_FAIL;
  }

  String NormalizedPhoneNumber;
  if (fNormalizedPhoneNumber(PhoneNumber, &NormalizedPhoneNumber) != SIM800_RES_OK) {
    return SIM800_RES_PHONENUMBER_INVALID;
  }

  if (!me->SavedPhoneNumbers.containsKey(NormalizedPhoneNumber)) {
    return SIM800_RES_PHONENUMBER_NOT_FOUND;
  }

  me->SavedPhoneNumbers.remove(NormalizedPhoneNumber);
  fSavePhoneNumbers(&me->SavedPhoneNumbers, SavedPhoneNumbersPath);

  return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param me 
 * @param PhoneNumber 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_RemoveAllPhoneNumbers(sSim800 * const me, String PhoneNumber) {

  if(me == NULL || !me->Init) {
    return SIM800_RES_INIT_FAIL;
  }

  return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param me 
 * @param PhoneNumber 
 * @param Text 
 * @param DeliveryCheck 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_SendSMS(sSim800 * const me, String PhoneNumber, String Text, bool DeliveryCheck) {

	return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param me 
 * @param Command 
 * @param DesiredResponse 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_SendCommand(sSim800 * const me, String Command, String DesiredResponse) {


	return SIM800_RES_OK;
}


/*
╔═════════════════════════════════════════════════════════════════════════════════╗
║                            ##### Private Functions #####                        ║
╚═════════════════════════════════════════════════════════════════════════════════╝*/
/**
 * @brief 
 * 
 * @param JsonDoc 
 * @param path 
 * @return sim800_res_t 
 */
static sim800_res_t fSavePhoneNumbers(JsonDocument *JsonDoc, String path) {

  File file = SPIFFS.open(path, FILE_WRITE);
  if(!file) {
    return SIM800_RES_LOAD_JSON_FIAL;
  }

  serializeJson(*JsonDoc, file);
  file.close();

  return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param JsonDoc 
 * @param path 
 * @return sim800_res_t 
 */
static sim800_res_t fLoadPhoneNumbers(JsonDocument *JsonDoc, String path) {

  File file = SPIFFS.open(path, FILE_READ);
  if(!file) {
    return SIM800_RES_LOAD_JSON_FIAL;
  }
  deserializeJson(*JsonDoc, file);
  file.close();

  return SIM800_RES_OK;
}

/**
 * @brief 
 * 
 * @param PhoneNum 
 * @return String 
 */
static sim800_res_t fNormalizedPhoneNumber(String PhoneNumber, String *Normalized) {

  if(PhoneNumber.length() == 11 && PhoneNumber.startsWith("0")) {
    *Normalized = PhoneNumber;
  } 
  else if(PhoneNumber.length() == 13 && PhoneNumber.startsWith("+")) {
    *Normalized = "0" + PhoneNumber.substring(3);
  } else {
    return SIM800_RES_PHONENUMBER_INVALID;
  }

  return SIM800_RES_OK;
}


/**End of Group_Name
  * @}
  */
/************************ © COPYRIGHT DideGroup *****END OF FILE****/