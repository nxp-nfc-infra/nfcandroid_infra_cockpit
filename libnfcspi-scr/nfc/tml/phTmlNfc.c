/*
 * Copyright 2010-2014,2022-2023 NXP
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
 * TML Implementation.
 */

#include <phDal4Nfc_messageQueueLib.h>
#include <phNxpLog.h>
#include <phNxpNciHal_utils.h>
#include <phOsalNfc_Timer.h>
#include <phTmlNfc.h>
#include <phTmlNfc_i2c.h>

/*
 * Duration of Timer to wait after sending an Nci packet
 */
#define PHTMLNFC_MAXTIME_RETRANSMIT (200U)
#define MAX_WRITE_RETRY_COUNT 0x03
/* Retry Count = Standby Recovery time of NFCC / Retransmission time + 1 */
static uint8_t bCurrentRetryCount = (2000 / PHTMLNFC_MAXTIME_RETRANSMIT) + 1;

/* Value to reset variables of TML  */
#define PH_TMLNFC_RESET_VALUE (0x00)

/* Indicates a Initial or offset value */
#define PH_TMLNFC_VALUE_ONE (0x01)

/* Initialize Context structure pointer used to access context structure */
phTmlNfc_Context_t *gpphTmlNfc_Context = NULL;
extern phTmlNfc_i2cfragmentation_t fragmentation_enabled;
/* Local Function prototypes */
static NFCSTATUS phTmlNfc_StartThread(void);
static void phTmlNfc_CleanUp(void);
static void phTmlNfc_ReadDeferredCb(void *pParams);
static void phTmlNfc_TmlThread(void *pParam);

/* Function definitions */

/*******************************************************************************
**
** Function         phTmlNfc_Init
**
** Description      Provides initialization of TML layer and hardware interface
**                  Configures given hardware interface and sends handle to the
**                  caller
**
** Parameters       pConfig     - TML configuration details as provided by the
**                                upper layer
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS            - initialization successful
**                  NFCSTATUS_INVALID_PARAMETER  - at least one parameter is
**                                                 invalid
**                  NFCSTATUS_FAILED             - initialization failed
**                                                 (for example, unable to open
**                                                 hardware interface)
**                  NFCSTATUS_INVALID_DEVICE     - device has not been opened or
**                                                 has been disconnected
**
*******************************************************************************/
NFCSTATUS phTmlNfc_Init(pphTmlNfc_Config_t pConfig) {
  NFCSTATUS wInitStatus = NFCSTATUS_SUCCESS;

  /* Check if TML layer is already Initialized */
  if (NULL != gpphTmlNfc_Context) {
    /* TML initialization is already completed */
    wInitStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_ALREADY_INITIALISED);
  }
  /* Validate Input parameters */
  else if ((NULL == pConfig) ||
           (PH_TMLNFC_RESET_VALUE == pConfig->dwGetMsgThreadId)) {
    /*Parameters passed to TML init are wrong */
    wInitStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_INVALID_PARAMETER);
  } else {
    /* Allocate memory for TML context */
    gpphTmlNfc_Context = malloc(sizeof(phTmlNfc_Context_t));

    if (NULL == gpphTmlNfc_Context) {
      wInitStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_FAILED);
    } else {
      /* Initialise all the internal TML variables */
      memset(gpphTmlNfc_Context, PH_TMLNFC_RESET_VALUE,
             sizeof(phTmlNfc_Context_t));
      /* Make sure that the thread runs once it is created */
      gpphTmlNfc_Context->bThreadDone = 1;

      /* Open the device file to which data is read/written */
      wInitStatus = phTmlNfc_i2c_open_and_configure(
          pConfig, &(gpphTmlNfc_Context->pDevHandle));
      NXPLOG_TML_D("gpphTmlNfc_Context->pDevHandle : %x, wInitStatus : %d",
                   (unsigned int)gpphTmlNfc_Context->pDevHandle, wInitStatus);
      {
        gpphTmlNfc_Context->tReadInfo.bEnable = 0;
        gpphTmlNfc_Context->tWriteInfo.bEnable = 0;
        gpphTmlNfc_Context->tReadInfo.bThreadBusy = FALSE;
        gpphTmlNfc_Context->tWriteInfo.bThreadBusy = FALSE;

        if (0 != sem_init(&gpphTmlNfc_Context->rxSemaphore, 0, 0)) {
          wInitStatus = NFCSTATUS_FAILED;
        } else if (0 != sem_init(&gpphTmlNfc_Context->txSemaphore, 0, 0)) {
          wInitStatus = NFCSTATUS_FAILED;
        } else if (0 != sem_init(&gpphTmlNfc_Context->postMsgSemaphore, 0, 0)) {
          wInitStatus = NFCSTATUS_FAILED;
        } else {
          sem_post(&gpphTmlNfc_Context->postMsgSemaphore);
          /* Start TML thread (to handle write and read operations) */
          if (NFCSTATUS_SUCCESS != phTmlNfc_StartThread()) {
            wInitStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_FAILED);
          } else {
            /* Create Timer used for Retransmission of NCI packets */
            gpphTmlNfc_Context->dwTimerId = phOsalNfc_Timer_Create();
            if (PH_OSALNFC_TIMER_ID_INVALID != gpphTmlNfc_Context->dwTimerId) {
              /* Store the Thread Identifier to which Message is to be posted */
              gpphTmlNfc_Context->dwCallbackThreadId =
                  pConfig->dwGetMsgThreadId;
              /* Enable retransmission of Nci packet & set retry count to
               * default */
              gpphTmlNfc_Context->eConfig = phTmlNfc_e_DisableRetrans;
              /** Retry Count = Standby Recovery time of NFCC / Retransmission
               * time + 1 */
              gpphTmlNfc_Context->bRetryCount =
                  (2000 / PHTMLNFC_MAXTIME_RETRANSMIT) + 1;
              gpphTmlNfc_Context->bWriteCbInvoked = FALSE;
            } else {
              wInitStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_FAILED);
            }
          }
        }
      }
    }
  }
  /* Clean up all the TML resources if any error */
  if (NFCSTATUS_SUCCESS != wInitStatus) {
    /* Clear all handles and memory locations initialized during init */
    phTmlNfc_CleanUp();
  }

  return wInitStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_ConfigNciPktReTx
**
** Description      Provides Enable/Disable Retransmission of NCI packets
**                  Needed in case of Timeout between Transmission and Reception
**                  of NCI packets Retransmission can be enabled only if standby
**                  mode is enabled.
**
** Parameters       eConfig     - values from phTmlNfc_ConfigRetrans_t
**                  bRetryCount - Number of times Nci packets shall be
**                                retransmitted (default = 3)
**
** Returns          None
**
*******************************************************************************/
void phTmlNfc_ConfigNciPktReTx(phTmlNfc_ConfigRetrans_t eConfiguration,
                               uint8_t bRetryCounter) {
  /* Enable/Disable Retransmission */

  gpphTmlNfc_Context->eConfig = eConfiguration;
  if (phTmlNfc_e_EnableRetrans == eConfiguration) {
    /* Check whether Retry counter passed is valid */
    if (0 != bRetryCounter) {
      gpphTmlNfc_Context->bRetryCount = bRetryCounter;
    }
    /* Set retry counter to its default value */
    else {
      /** Retry Count = Standby Recovery time of NFCC / Retransmission time + 1
       */
      gpphTmlNfc_Context->bRetryCount =
          (2000 / PHTMLNFC_MAXTIME_RETRANSMIT) + 1;
    }
  }

  return;
}

static uint8_t bFlushBuf = 0;
void phNxpNciHal_setflush(uint8_t value) { bFlushBuf = value; }

/*******************************************************************************
**
** Function         phTmlNfc_StartThread
**
** Description      Initializes comport, reader and writer threads
**
** Parameters       None
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS    - threads initialized successfully
**                  NFCSTATUS_FAILED     - initialization failed due to system
*error
**
*******************************************************************************/
static NFCSTATUS phTmlNfc_StartThread(void) {
  NFCSTATUS wStartStatus = NFCSTATUS_SUCCESS;
  void *h_threadsEvent = 0x00;
  int pthread_create_status = 0;

  /* Create Reader and Writer threads */
  NXPLOG_TML_D("phTmlNfc_StartThread, bFlushBuf : %d", bFlushBuf);
  if (bFlushBuf == 0) {
    pthread_create_status =
        pthread_create(&gpphTmlNfc_Context->readerThread, NULL,
                       (void *)&phTmlNfc_TmlThread, (void *)h_threadsEvent);
  } else {
    pthread_create_status = 0;
  }

  return wStartStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_TmlThread
**
** Description      Read the data from the lower layer driver
**
** Parameters       pParam  - parameters for Writer thread function
**
** Returns          None
**
*******************************************************************************/
static void phTmlNfc_TmlThread(void *pParam) {
  NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
  int32_t dwNoBytesWrRd = PH_TMLNFC_RESET_VALUE;
  uint8_t temp[524];
  /* Transaction info buffer to be passed to Callback Thread */
  static phTmlNfc_TransactInfo_t tTransactionInfo;
  /* Structure containing Tml callback function and parameters to be invoked
     by the callback thread */
  static phLibNfc_DeferredCall_t tDeferredInfo;
  /* Initialize Message structure to post message onto Callback Thread */
  static phLibNfc_Message_t tMsg;
  UNUSED(pParam);
  NXPLOG_TML_D("PN548AD - Tml Reader Thread Started................\n");

  /* Writer thread loop shall be running till shutdown is invoked */
  while ((gpphTmlNfc_Context != NULL) && (gpphTmlNfc_Context->bThreadDone)) {
    /* If Tml write is requested */
    /* Set the variable to success initially */
    wStatus = NFCSTATUS_SUCCESS;
    sem_wait(&gpphTmlNfc_Context->rxSemaphore);

    /* If Tml read is requested */
    if (1 == gpphTmlNfc_Context->tReadInfo.bEnable) {
      NXPLOG_TML_D("PN548AD - Read requested.....\n");
      /* Set the variable to success initially */
      wStatus = NFCSTATUS_SUCCESS;

      /* Variable to fetch the actual number of bytes read */
      dwNoBytesWrRd = PH_TMLNFC_RESET_VALUE;

      /* Read the data from the file onto the buffer */
      // DY_DEBUG temp            if (NFCSTATUS_INVALID_DEVICE !=
      // (uintptr_t)gpphTmlNfc_Context->pDevHandle)
      {
        NXPLOG_TML_D("PN548AD - Invoking I2C Read.....\n");
        dwNoBytesWrRd =
            phTmlNfc_i2c_read(gpphTmlNfc_Context->pDevHandle, temp, 260);

        if (-1 == dwNoBytesWrRd) {
          NXPLOG_TML_E("PN548AD - Error in I2C Read.....\n");
          sem_post(&gpphTmlNfc_Context->rxSemaphore);
        } else {
          memcpy(gpphTmlNfc_Context->tReadInfo.pBuffer, temp, dwNoBytesWrRd);

          NXPLOG_TML_D("PN548AD - I2C Read successful.....\n");
          /* This has to be reset only after a successful read */
          gpphTmlNfc_Context->tReadInfo.bEnable = 0;
          if ((phTmlNfc_e_EnableRetrans == gpphTmlNfc_Context->eConfig) &&
              (0x00 != (gpphTmlNfc_Context->tReadInfo.pBuffer[0] & 0xE0))) {
            NXPLOG_TML_D("PN548AD - Retransmission timer stopped.....\n");
            /* Stop Timer to prevent Retransmission */
            uint32_t timerStatus =
                phOsalNfc_Timer_Stop(gpphTmlNfc_Context->dwTimerId);
            if (NFCSTATUS_SUCCESS != timerStatus) {
              NXPLOG_TML_E("PN548AD - timer stopped returned failure.....\n");
            } else {
              gpphTmlNfc_Context->bWriteCbInvoked = FALSE;
            }
          }
          /* Update the actual number of bytes read including header */
          gpphTmlNfc_Context->tReadInfo.wLength = (uint16_t)(dwNoBytesWrRd);
          if (bFlushBuf == 0)
            phNxpNciHal_print_packet("RECV",
                                     gpphTmlNfc_Context->tReadInfo.pBuffer,
                                     gpphTmlNfc_Context->tReadInfo.wLength);

          dwNoBytesWrRd = PH_TMLNFC_RESET_VALUE;

          /* Fill the Transaction info structure to be passed to Callback
           * Function */
          tTransactionInfo.wStatus = wStatus;
          tTransactionInfo.pBuff = gpphTmlNfc_Context->tReadInfo.pBuffer;
          /* Actual number of bytes read is filled in the structure */
          tTransactionInfo.wLength = gpphTmlNfc_Context->tReadInfo.wLength;

          /* Read operation completed successfully. Post a Message onto Callback
           * Thread*/
          /* Prepare the message to be posted on User thread */
          tDeferredInfo.pCallback = &phTmlNfc_ReadDeferredCb;
          tDeferredInfo.pParameter = &tTransactionInfo;
          tMsg.eMsgType = PH_LIBNFC_DEFERREDCALL_MSG;
          tMsg.pMsgData = &tDeferredInfo;
          tMsg.Size = sizeof(tDeferredInfo);
          NXPLOG_TML_D("PN548AD - Posting read message.....\n");
          phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId, &tMsg);
        }
      }
    } else {
      NXPLOG_TML_D("PN548AD - read request NOT enabled");
      usleep(10 * 1000);
    }
  } /* End of While loop */

  return;
}

/*******************************************************************************
**
** Function         phTmlNfc_CleanUp
**
** Description      Clears all handles opened during TML initialization
**
** Parameters       None
**
** Returns          None
**
*******************************************************************************/
static void phTmlNfc_CleanUp(void) {
  if (NULL != gpphTmlNfc_Context->pDevHandle) {
    // scr        (void) phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 0);
    gpphTmlNfc_Context->bThreadDone = 0;
  }
  sem_destroy(&gpphTmlNfc_Context->rxSemaphore);
  sem_destroy(&gpphTmlNfc_Context->txSemaphore);
  sem_destroy(&gpphTmlNfc_Context->postMsgSemaphore);
  phTmlNfc_i2c_close(gpphTmlNfc_Context->pDevHandle);
  gpphTmlNfc_Context->pDevHandle = NULL;
  /* Clear memory allocated for storing Context variables */
  free((void *)gpphTmlNfc_Context);
  /* Set the pointer to NULL to indicate De-Initialization */
  gpphTmlNfc_Context = NULL;

  return;
}

/*******************************************************************************
**
** Function         phTmlNfc_Shutdown
**
** Description      Uninitializes TML layer and hardware interface
**
** Parameters       None
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS            - TML configuration released
**                                                 successfully
**                  NFCSTATUS_INVALID_PARAMETER  - at least one parameter is
**                                                 invalid
**                  NFCSTATUS_FAILED             - un-initialization failed
**                                          (example: unable to close interface)
**
*******************************************************************************/
NFCSTATUS phTmlNfc_Shutdown(void) {
  NFCSTATUS wShutdownStatus = NFCSTATUS_SUCCESS;

  /* Check whether TML is Initialized */
  if (NULL != gpphTmlNfc_Context) {
    /* Reset thread variable to terminate the thread */
    gpphTmlNfc_Context->bThreadDone = 0;
    usleep(1000);
    /* Clear All the resources allocated during initialization */
    sem_post(&gpphTmlNfc_Context->rxSemaphore);
    usleep(1000);
    sem_post(&gpphTmlNfc_Context->txSemaphore);
    usleep(1000);
    sem_post(&gpphTmlNfc_Context->postMsgSemaphore);
    usleep(1000);
    sem_post(&gpphTmlNfc_Context->postMsgSemaphore);
    usleep(1000);

    if (0 != pthread_join(gpphTmlNfc_Context->readerThread, (void **)NULL)) {
      NXPLOG_TML_E("Fail to kill reader thread!");
    }

    NXPLOG_TML_D("bThreadDone == 0");

    phTmlNfc_CleanUp();
  } else {
    wShutdownStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_NOT_INITIALISED);
  }

  return wShutdownStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_Write
**
** Description      Asynchronously writes given data block to hardware
**                  interface/driver Enables writer thread if there are no write
**                  requests pending Returns successfully once writer thread
**                  completes write operation Notifies upper layer using
**                  callback mechanism.
**                  NOTE:
**                  * it is important to post a message with id
**                    PH_TMLNFC_WRITE_MESSAGE to IntegrationThread after data
**                    has been written to PN548AD.
**                  * if CRC needs to be computed, then input buffer should be
**                    capable to store two more bytes apart from length of
**                    packet.
**
** Parameters       pBuffer              - data to be sent
**                  wLength              - length of data buffer
**                  pTmlWriteComplete    - pointer to the function to be invoked
**                                         upon completion
**                  pContext             - context provided by upper layer
**
** Returns          NFC status:
**                  NFCSTATUS_PENDING             - command is yet to be
**                                                  processed
**                  NFCSTATUS_INVALID_PARAMETER   - at least one parameter is
**                                                  invalid
**                  NFCSTATUS_BUSY                - write request is already in
**                                                  progress
**
*******************************************************************************/
extern int8_t *GKI_get_time_stamp(int8_t *tbuf, int len);
NFCSTATUS phTmlNfc_Write(uint8_t *pBuffer, uint16_t wLength) {
  NFCSTATUS wWriteStatus;
  if (NULL != gpphTmlNfc_Context) {
    if ((NULL != gpphTmlNfc_Context->pDevHandle) && (NULL != pBuffer) &&
        (PH_TMLNFC_RESET_VALUE != wLength)) {
      NXPLOG_TML_D("PN548AD - Write requested.....\n");
      int32_t dwNoBytesWrRd = PH_TMLNFC_RESET_VALUE;
      static uint16_t retry_cnt;

      do {
        dwNoBytesWrRd = PH_TMLNFC_RESET_VALUE;
        /* Write the data in the buffer onto the file */
        NXPLOG_TML_D("PN548AD - Invoking I2C Write.....\n");
        wWriteStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_SUCCESS);
        /*DY_DEBUG*/
        if (bFlushBuf == 0) {
          int i;
          int8_t tbuf[LEN_TML_TIME_BUF] = {0};
          printf("%s SEND   : %3d > ",
                 GKI_get_time_stamp(tbuf, LEN_TML_TIME_BUF), wLength);
          for (i = 0; i < wLength; i++) printf("%02X", *(pBuffer + i));
          printf("\n");
          phNxpNciHal_print_packet("SEND", pBuffer, wLength);
        }
        dwNoBytesWrRd = phTmlNfc_i2c_write(gpphTmlNfc_Context->pDevHandle,
                                           pBuffer, wLength);
        wWriteStatus = NFCSTATUS_SUCCESS;
      } while ((-1 == dwNoBytesWrRd) && (retry_cnt++ < MAX_WRITE_RETRY_COUNT));

      if (-1 == dwNoBytesWrRd) {
        uint8_t error_msg[] = {0xDE, 0xAD, 0xBE, 0xEF};
        if (bFlushBuf == 0)
          phNxpNciHal_print_packet("SEND", error_msg, sizeof(error_msg));
        wWriteStatus = NFCSTATUS_FAILED;
        NXPLOG_TML_E("PN548AD - Error in I2C Write.....\n");
      }
      retry_cnt = 0;
      if (NFCSTATUS_SUCCESS == wWriteStatus) {
        NXPLOG_TML_D("PN548AD - I2C Write successful.....\n");
        dwNoBytesWrRd = PH_TMLNFC_VALUE_ONE;
      }
    } else {
      wWriteStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_INVALID_PARAMETER);
    }
  } else {
    wWriteStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_NOT_INITIALISED);
  }
  return wWriteStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_Read
**
** Description      Asynchronously reads data from the driver
**                  Number of bytes to be read and buffer are passed by upper
**                  layer Enables reader thread if there are no read requests
**                  pending Returns successfully once read operation is
**                  completed Notifies upper layer using callback mechanism
**
** Parameters       pBuffer              - location to send read data to the
**                                         upper layer via callback
**                  wLength              - length of read data buffer passed
**                                         by upper layer
**                  pTmlReadComplete     - pointer to the function to be
**                                         invoked upon completion of read
**                                         operation
**                  pContext             - context provided by upper layer
**
** Returns          NFC status:
**                  NFCSTATUS_PENDING             - command is yet to be
**                                                  processed
**                  NFCSTATUS_INVALID_PARAMETER   - at least one parameter is
**                                                  invalid
**                  NFCSTATUS_BUSY                - read request is already in
**                                                  progress
**
*******************************************************************************/
NFCSTATUS phTmlNfc_Read(uint8_t *pBuffer, uint16_t wLength,
                        pphTmlNfc_TransactCompletionCb_t pTmlReadComplete,
                        void *pContext) {
  NFCSTATUS wReadStatus;

  /* Check whether TML is Initialized */
  if (NULL != gpphTmlNfc_Context) {
    if ((gpphTmlNfc_Context->pDevHandle != NULL) && (NULL != pBuffer) &&
        (PH_TMLNFC_RESET_VALUE != wLength) && (NULL != pTmlReadComplete)) {
      if (!gpphTmlNfc_Context->tReadInfo.bThreadBusy) {
        /* Setting the flag marks beginning of a Read Operation */
        gpphTmlNfc_Context->tReadInfo.bThreadBusy = TRUE;
        /* Copy the buffer, length and Callback function,
           This shall be utilized while invoking the Callback function in
           thread
         */
        gpphTmlNfc_Context->tReadInfo.pBuffer = pBuffer;
        gpphTmlNfc_Context->tReadInfo.wLength = wLength;
        gpphTmlNfc_Context->tReadInfo.pThread_Callback = pTmlReadComplete;
        gpphTmlNfc_Context->tReadInfo.pContext = pContext;
        wReadStatus = NFCSTATUS_PENDING;

        /* Set event to invoke Reader Thread */
        gpphTmlNfc_Context->tReadInfo.bEnable = 1;
        sem_post(&gpphTmlNfc_Context->rxSemaphore);
      } else {
        wReadStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_BUSY);
      }
    } else {
      wReadStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_INVALID_PARAMETER);
    }
  } else {
    wReadStatus = PHNFCSTVAL(CID_NFC_TML, NFCSTATUS_NOT_INITIALISED);
  }

  return wReadStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_ReadAbort
**
** Description      Aborts pending read request (if any)
**
** Parameters       None
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS                    - ongoing read
**                                                         operation aborted
**                  NFCSTATUS_INVALID_PARAMETER          - at least one
**                                                         parameter is invalid
**                  NFCSTATUS_NOT_INITIALIZED            - TML layer is not
**                                                         initialized
**                  NFCSTATUS_BOARD_COMMUNICATION_ERROR  - unable to cancel
**                                                         read operation
**
*******************************************************************************/
NFCSTATUS phTmlNfc_ReadAbort(void) {
  NFCSTATUS wStatus = NFCSTATUS_INVALID_PARAMETER;
  gpphTmlNfc_Context->tReadInfo.bEnable = 0;

  /*Reset the flag to accept another Read Request */
  gpphTmlNfc_Context->tReadInfo.bThreadBusy = FALSE;
  wStatus = NFCSTATUS_SUCCESS;

  return wStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_WriteAbort
**
** Description      Aborts pending write request (if any)
**
** Parameters       None
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS                    - ongoing write
**                                                         operation aborted
**                  NFCSTATUS_INVALID_PARAMETER          - at least one
**                                                         parameter is invalid
**                  NFCSTATUS_NOT_INITIALIZED            - TML layer is not
**                                                         initialized
**                  NFCSTATUS_BOARD_COMMUNICATION_ERROR  - unable to cancel
**                                                         write operation
**
*******************************************************************************/
NFCSTATUS phTmlNfc_WriteAbort(void) {
  NFCSTATUS wStatus = NFCSTATUS_INVALID_PARAMETER;

  gpphTmlNfc_Context->tWriteInfo.bEnable = 0;
  /* Stop if any retransmission is in progress */
  bCurrentRetryCount = 0;

  /* Reset the flag to accept another Write Request */
  gpphTmlNfc_Context->tWriteInfo.bThreadBusy = FALSE;
  wStatus = NFCSTATUS_SUCCESS;

  return wStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_IoCtl
**
** Description      Resets device when insisted by upper layer
**                  Number of bytes to be read and buffer are passed by upper
**                  layer Enables reader thread if there are no read requests
**                  pending Returns successfully once read operation is
**                  completed Notifies upper layer using callback mechanism
**
** Parameters       eControlCode       - control code for a specific operation
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS  - ioctl command completed successfully
**                  NFCSTATUS_FAILED   - ioctl command request failed
**
*******************************************************************************/
NFCSTATUS phTmlNfc_IoCtl(phTmlNfc_ControlCode_t eControlCode) {
  NFCSTATUS wStatus = NFCSTATUS_SUCCESS;

  if (NULL == gpphTmlNfc_Context) {
    wStatus = NFCSTATUS_FAILED;
  } else {
    switch (eControlCode) {
      case phTmlNfc_e_ResetDevice: {
        /*Reset PN548AD*/
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 1);
        usleep(100 * 1000);
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 0);
        usleep(100 * 1000);
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 1);
        break;
      }
      case phTmlNfc_e_EnableNormalMode: {
        /*Reset PN548AD*/
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 0);
        usleep(10 * 1000);
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 1);
        usleep(100 * 1000);
        break;
      }
      case phTmlNfc_e_EnableDownloadMode: {
        phTmlNfc_ConfigNciPktReTx(phTmlNfc_e_DisableRetrans, 0);
        (void)phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 2);
        usleep(100 * 1000);
        break;
      }
      case 0: {
        /*Reset PN548AD*/
        phTmlNfc_i2c_reset(gpphTmlNfc_Context->pDevHandle, 0);
        usleep(10 * 1000);
        break;
      }
      default: {
        wStatus = NFCSTATUS_INVALID_PARAMETER;
        break;
      }
    }
  }

  return wStatus;
}

/*******************************************************************************
**
** Function         phTmlNfc_DeferredCall
**
** Description      Posts message on upper layer thread
**                  upon successful read or write operation
**
** Parameters       dwThreadId  - id of the thread posting message
**                  ptWorkerMsg - message to be posted
**
** Returns          None
**
*******************************************************************************/
void phTmlNfc_DeferredCall(uintptr_t dwThreadId,
                           phLibNfc_Message_t *ptWorkerMsg) {
  intptr_t bPostStatus;
  UNUSED(dwThreadId);
  /* Post message on the user thread to invoke the callback function */
  sem_wait(&gpphTmlNfc_Context->postMsgSemaphore);
  bPostStatus =
      phDal4Nfc_msgsnd(gpphTmlNfc_Context->dwCallbackThreadId, ptWorkerMsg, 0);
  sem_post(&gpphTmlNfc_Context->postMsgSemaphore);
}

/*******************************************************************************
**
** Function         phTmlNfc_ReadDeferredCb
**
** Description      Read thread call back function
**
** Parameters       pParams - context provided by upper layer
**
** Returns          None
**
*******************************************************************************/
static void phTmlNfc_ReadDeferredCb(void *pParams) {
  /* Transaction info buffer to be passed to Callback Function */
  phTmlNfc_TransactInfo_t *pTransactionInfo =
      (phTmlNfc_TransactInfo_t *)pParams;

  /* Reset the flag to accept another Read Request */
  gpphTmlNfc_Context->tReadInfo.bThreadBusy = FALSE;
  gpphTmlNfc_Context->tReadInfo.pThread_Callback(
      gpphTmlNfc_Context->tReadInfo.pContext, pTransactionInfo);

  return;
}
