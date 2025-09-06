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

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static sim800_res_t fSaveJson(JsonDocument *JsonDoc, String path);
static sim800_res_t fLoadJson(JsonDocument *JsonDoc, String path);
static String fNormalizedPhoneNum(String PhoneNum);

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

    if(me == NULL || me->SavePhoneNumbersPath.isEmpty()) {
        return SIM800_RES_INIT_FAIL;
    }

    me->Init = false;

    fLoadJson(&me->SavedPhoneNumbers, me->SavePhoneNumbersPath);


    me->Enable = true;
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

}

/**
 * @brief 
 * 
 * @param me 
 * @param PhoneNumber 
 * @return sim800_res_t 
 */
sim800_res_t fSim800_RemovePhoneNumber(sSim800 * const me, String PhoneNumber) {

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
static sim800_res_t fSaveJson(JsonDocument *JsonDoc, String path) {

    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        return SIM800_RES_LOAD_JSON_FIAL;
    }
    serializeJson(*JsonDoc, file);
    file.close();
}

/**
 * @brief 
 * 
 * @param JsonDoc 
 * @param path 
 * @return sim800_res_t 
 */
static sim800_res_t fLoadJson(JsonDocument *JsonDoc, String path) {

    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
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
static String fNormalizedPhoneNum(String PhoneNum) {

  String Normalized = "";

  if (PhoneNum.length() == 11 && PhoneNum.startsWith("0")) {
    Normalized = "+98" + PhoneNum.substring(1);
  } 
  else if (PhoneNum.length() == 13 && PhoneNum.startsWith("+")) {
    Normalized = PhoneNum;
  }

  return Normalized;
}


/**End of Group_Name
  * @}
  */
/************************ © COPYRIGHT DideGroup *****END OF FILE****/