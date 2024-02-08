/*
 * Copyright 2010-2014,2022 NXP
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
#ifndef _PHNXPSPILIB_H_
#define _PHNXPSPILIB_H_

#include <phNxpEse_Api.h>
#include <phNxpSpiLog.h>

/********************* Definitions and structures *****************************/

typedef enum {
  ESE_STATUS_CLOSE = 0x00,
  ESE_STATUS_BUSY,
  ESE_STATUS_RECOVERY,
  ESE_STATUS_IDLE,
  ESE_STATUS_OPEN,
} phNxpEse_LibStatus;

typedef enum {
  PN67T_POWER_SCHEME = 0x01,
  PN80T_LEGACY_SCHEME,
  PN80T_EXT_PMU_SCHEME,
} phNxpEse_PowerScheme;

typedef enum {
  END_POINT_ESE = 0, /*!< eSE services */
  END_POINT_EUICC,   /*!< UICC services*/
  MAX_END_POINTS
} phNxpEse_EndPoint;

/* Macros definition */
#define MAX_DATA_LEN 780
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
#define ESE_JCOP_OS_DWNLD_RETRY_CNT \
  10 /* Maximum retry count for ESE JCOP OS Dwonload*/
#endif
#if (NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
#define ESE_FW_DWNLD_RETRY_CNT 10 /* Maximum retry count for FW Dwonload*/
#endif

typedef enum nadInfoTx {
  ESE_NAD_TX = 0x5A,  /*!< R-frame Acknowledgement frame indicator */
  EUICC_NAD_TX = 0x4B /*!< R-frame Negative-Acknowledgement frame indicator */
} nadInfoTx_t;

/*!
 * \brief R-Frame types used in 7816-3 protocol stack
 */
typedef enum nadInfoRx {
  ESE_NAD_RX = 0xA5,  /*!< R-frame Acknowledgement frame indicator */
  EUICC_NAD_RX = 0xB4 /*!< R-frame Negative-Acknowledgement frame indicator */
} nadInfoRx_t;

typedef struct phNxpEseNadInfo {
  nadInfoTx_t nadTx;
  nadInfoRx_t nadRx;
} phNxpEseNadInfo_t;

#define MAX_RETRY_COUNT 3
#define ESE_OPEN_SLEEP_TIME 4000
#define PROP_NOT_FOUND ("_not_found")

/* SPI Control structure */
typedef struct phNxpEse_Context {
  void *pDevHandle;
  long nadPollingRetryTime;
  long invalidFrame_Rnack_Delay;
  phNxpEse_LibStatus EseLibStatus; /* Indicate if Ese Lib is open or closed */
  phNxpEse_initParams initParams;
  phNxpEseNadInfo_t nadInfo;
  uint8_t p_read_buff[MAX_DATA_LEN];
  uint8_t p_cmd_data[MAX_DATA_LEN];
  uint16_t cmd_len;
  uint8_t pwr_scheme;
  uint8_t endPointInfo;
  bool_t rnack_sent;
  bool_t spm_power_state;
} phNxpEse_Context_t;

/* Timeout value to wait for response from
   Note: Timeout value updated from 1000 to 2000 to fix the JCOP delay (WTX)*/
#define HAL_EXTNS_WRITE_RSP_TIMEOUT (2000)

#define SPILIB_CMD_CODE_LEN_BYTE_OFFSET (2U)
#define SPILIB_CMD_CODE_BYTE_LEN (3U)

static nadInfoTx_t nadInfoTx_ptr[MAX_END_POINTS] = {ESE_NAD_TX, EUICC_NAD_TX};

static nadInfoRx_t nadInfoRx_ptr[MAX_END_POINTS] = {ESE_NAD_RX, EUICC_NAD_RX};
ESESTATUS phNxpEse_WriteFrame(uint32_t data_len, uint8_t *p_data);
ESESTATUS phNxpEse_read(uint32_t *data_len, uint8_t **pp_data);

#endif /* _PHNXPSPILIB_H_ */
