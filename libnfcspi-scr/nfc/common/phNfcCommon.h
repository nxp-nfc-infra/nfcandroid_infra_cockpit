/*
 * Copyright 2010-2014,2023 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 *  OSAL header files related to memory, debug, random, semaphore and mutex
 * functions.
 */

#ifndef PHNFCCOMMON_H
#define PHNFCCOMMON_H

/*
************************* Include Files ****************************************
*/

#include <phDal4Nfc_messageQueueLib.h>
#include <phNfcCompId.h>
#include <phNfcStatus.h>
#include <phOsalNfc_Timer.h>
#include <pthread.h>
#include <semaphore.h>

/* HAL Version number (Updated as per release) */
#define NXP_MW_VERSION_MAJ (3U)
#define NXP_MW_VERSION_MIN (1U)

/*
 *  information to configure OSAL
 */
typedef struct phOsalNfc_Config {
  uint8_t *pLogFile;            /* Log File Name*/
  uintptr_t dwCallbackThreadId; /* Client ID to which message is posted */
} phOsalNfc_Config_t, *pphOsalNfc_Config_t /* Pointer to #phOsalNfc_Config_t */;

/*
 * Deferred call declaration.
 * This type of API is called from ClientApplication (main thread) to notify
 * specific callback.
 */
typedef void (*pphOsalNfc_DeferFuncPointer_t)(void *);

/*
 * Deferred message specific info declaration.
 */
typedef struct phOsalNfc_DeferedCallInfo {
  pphOsalNfc_DeferFuncPointer_t pDeferedCall; /* pointer to Deferred callback */
  void *pParam; /* contains timer message specific details*/
} phOsalNfc_DeferedCallInfo_t;

/*
 * States in which a OSAL timer exist.
 */
typedef enum {
  eTimerIdle = 0,          /* Indicates Initial state of timer */
  eTimerRunning = 1,       /* Indicate timer state when started */
  eTimerStopped = 2        /* Indicates timer state when stopped */
} phOsalNfc_TimerStates_t; /* Variable representing State of timer */

/*
 **Timer Handle structure containing details of a timer.
 */
typedef struct phOsalNfc_TimerHandle {
  uint32_t TimerId;     /* ID of the timer */
  timer_t hTimerHandle; /* Handle of the timer */
  pphOsalNfc_TimerCallbck_t
      Application_callback; /* Timer callback function to be invoked */
  void *pContext; /* Parameter to be passed to the callback function */
  phOsalNfc_TimerStates_t eState; /* Timer states */
  phLibNfc_Message_t
      tOsalMessage; /* Osal Timer message posted on User Thread */
  phOsalNfc_DeferedCallInfo_t tDeferedCallInfo; /* Deferred Call structure to
                                                   Invoke Callback function */
} phOsalNfc_TimerHandle_t,
    *pphOsalNfc_TimerHandle_t; /* Variables for Structure Instance and Structure
                                  Ptr */

#endif /*  PHOSALNFC_H  */
