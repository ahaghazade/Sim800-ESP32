/**
******************************************************************************
* @file           : sim800_cdrv.h
* @brief          : Header file for WS2812 LED driver
* @note           :
* @copyright      : COPYRIGHT© 2025 DiodeGroup
******************************************************************************
* @attention
*
* <h2><center>&copy; Copyright© 2025 DiodeGroup.
* All rights reserved.</center></h2>
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
* @verbatim
* @endverbatim
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CDRV_SIM800_H
#define CDRV_SIM800_H

/* Includes ------------------------------------------------------------------*/
#include <Arduino.h>

#include <ArduinoJson.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exported defines ----------------------------------------------------------*/
/**
 * @brief Return codes for sim800 operations
 * 
 */
typedef uint8_t sim800_res_t;

#define SIM800_RES_OK                           ((uint8_t)1)
#define SIM800_RES_INIT_FAIL                    ((uint8_t)2)
#define SIM800_RES_SEND_COMMAND_FAIL            ((uint8_t)3)
#define SIM800_RES_SEND_SMS_FAIL                ((uint8_t)4)
#define SIM800_RES_SAVE_JSON_FAIL               ((uint8_t)5)
#define SIM800_RES_LOAD_JSON_FIAL               ((uint8_t)6)

/* Exported macro ------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/**
 * @brief sim800 configuration structure
 * 
 */
typedef struct {

    bool Init;

    bool Enable;

    JsonDocument SavedPhoneNumbers;

    String SavePhoneNumbersPath;

    size_t(*fpSend)(String Data);


}sSim800;

/* Exported constants --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
sim800_res_t fSim800_Init(sSim800 * const me);
void fSim800_Run(sSim800 * const me);
sim800_res_t fSim800_AddPhoneNumber(sSim800 * const me, String PhoneNumber, bool IsAdmin);
sim800_res_t fSim800_RemovePhoneNumber(sSim800 * const me, String PhoneNumber);
sim800_res_t fSim800_SendSMS(sSim800 * const me, String PhoneNumber, String Text, bool DeliveryCheck);
sim800_res_t fSim800_SendCommand(sSim800 * const me, String Command, String DesiredResponse);

/* Exported variables --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* CDRV_WS2812_H */

/************************ © COPYRIGHT DiodeGroup *****END OF FILE****/