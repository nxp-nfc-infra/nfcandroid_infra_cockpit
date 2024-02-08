/*
 * Copyright 2012-2014,2022-2023 NXP
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

#include <NXP_ESE_FEATURES.h>
#include <phNxpEsePal.h>
#include <phNxpEsePal_spi.h>
#include <phNxpEseProto7816_3.h>
#include <phNxpEse_Apdu_Api.h>
#include <phNxpEse_Internal.h>
#include <phNxpSpiLog.h>
/* Macro to enable SPM Module */
#define SPM_INTEGRATED
// #undef SPM_INTEGRATED
#ifdef SPM_INTEGRATED
#include "../spm/phNxpEse_Spm.h"
#endif

// factory test [[
#include <string.h>
#include <sys/time.h>
#include <time.h>
// #include <log.h>

bool_t bOpenResult = FALSE;
// ]]

#define RECIEVE_PACKET_SOF 0xA5
#define CHAINED_PACKET_WITHSEQN 0x60
#define CHAINED_PACKET_WITHOUTSEQN 0x20
static int phNxpEse_readPacket(void *pDevHandle, uint8_t *pBuffer,
                               int nNbBytesToRead);
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
static ESESTATUS phNxpEse_checkJcopDwnldState(void);
static ESESTATUS phNxpEse_setJcopDwnldState(phNxpEse_JcopDwnldState state);
static ESESTATUS phNxpEse_checkFWDwnldStatus(void);
#endif
static __inline bool_t phNxpEse_isColdResetRequired(phNxpEse_initMode mode,
                                                    ESESTATUS status);
static int poll_sof_chained_delay = 0;
/*********************** Global Variables *************************************/

/* ESE Context structure */
phNxpEse_Context_t nxpese_ctxt;
uint8_t chipType = SN100X;
phPalEse_Config_t gPalConfig;

/******************************************************************************
 * Function         phNxpEse_SetEndPoint_Cntxt
 *
 * Description      This function is called set the SE endpoint
 *
 * Returns          None
 *
 ******************************************************************************/

ESESTATUS phNxpEse_SetEndPoint_Cntxt(uint8_t uEndPoint) {
  ESESTATUS status = phNxpEseProto7816_SetEndPoint(uEndPoint);
  if (status == ESESTATUS_SUCCESS) {
    nxpese_ctxt.nadInfo.nadRx = nadInfoRx_ptr[uEndPoint];
    nxpese_ctxt.nadInfo.nadTx = nadInfoTx_ptr[uEndPoint];
    nxpese_ctxt.endPointInfo = uEndPoint;
  }
  NXPLOG_ESELIB_D("%s Mr Robot in control, you are not !: fSociety: Enpoint=%d",
                  __FUNCTION__, uEndPoint);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_ResetEndPoint_Cntxt
 *
 * Description      This function is called to reset the SE endpoint
 *
 * Returns          None
 *
 ******************************************************************************/
ESESTATUS phNxpEse_ResetEndPoint_Cntxt(uint8_t uEndPoint) {
  ESESTATUS status = phNxpEseProto7816_ResetEndPoint(uEndPoint);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_init
 *
 * Description      This function is called by Jni/phNxpEse_open during the
 *                  initialization of the ESE. It initializes protocol stack
 *instance variable
 *
 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_init(phNxpEse_initParams initParams) {
  NXPLOG_ESELIB_D("%s Enter \n", __FUNCTION__);
  ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
  uint8_t retry = 0;
  phNxpEseProto7816InitParam_t protoInitParam;

  wConfigStatus = phNxpEse_SetEndPoint_Cntxt(0);
  if (wConfigStatus != ESESTATUS_SUCCESS) {
    return wConfigStatus;
  }

  phNxpEse_memset(&protoInitParam, 0x00, sizeof(phNxpEseProto7816InitParam_t));
  /* STATUS_OPEN */
  nxpese_ctxt.EseLibStatus = ESE_STATUS_OPEN;

  if (chipType == SN100X) nxpese_ctxt.nadPollingRetryTime = 2;

  protoInitParam.wtx_counter_limit = PH_PROTO_WTX_DEFAULT_COUNT;
  protoInitParam.rnack_retry_limit = MAX_RNACK_RETRY_LIMIT;

  if (ESE_MODE_NORMAL ==
      initParams.initMode) /* TZ/Normal wired mode should come here*/
  {
    protoInitParam.interfaceReset = TRUE;
  } else /* OSU mode, no interface reset is required */
  {
    protoInitParam.interfaceReset = FALSE;
  }
  /* Delay before the first transceive */
  phPalEse_sleep(3000);

  if (chipType == SN100X) {
    if (/*ESE_PROTOCOL_MEDIA_SPI_APDU_GATE == initParams.mediaType &&*/
        ESE_MODE_NORMAL == initParams.initMode) {
      NXPLOG_ESELIB_D("Issue dummy apdu before>>>>>");
      phNxpEse_AcquireEseLock(TRUE);
    }
  }

  do {
    /* T=1 Protocol layer open */
    wConfigStatus = phNxpEseProto7816_Open(protoInitParam);
    if (phNxpEse_isColdResetRequired(initParams.initMode, wConfigStatus))
      phNxpEse_SPM_ConfigPwr(SPM_RECOVERY_RESET);
  } while (phNxpEse_isColdResetRequired(initParams.initMode, wConfigStatus) &&
           retry++ < 1);
  if (ESESTATUS_TRANSCEIVE_FAILED == wConfigStatus ||
      ESESTATUS_FAILED == wConfigStatus) {
    nxpese_ctxt.EseLibStatus = ESE_STATUS_RECOVERY;
  } else if (ESESTATUS_SUCCESS == wConfigStatus && chipType == SN100X) {
    if (ESE_MODE_NORMAL == initParams.initMode) {
      NXPLOG_ESELIB_D("Issue dummy apdu after>>>>>");
      wConfigStatus = phNxpEse_AcquireEseLock(FALSE);
    }
  } else {
    NXPLOG_ESELIB_D("phNxpEseProto7816_Open failed >>>>>");
  }
  if (ESESTATUS_FAILED == wConfigStatus) {
    wConfigStatus = ESESTATUS_FAILED;
    NXPLOG_ESELIB_E("phNxpEseProto7816_Open failed");
    return wConfigStatus;
  }

  phNxpEse_setIfs(
      phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLenIFSC);
  wConfigStatus = phNxpEse_ResetEndPoint_Cntxt(0);

  NXPLOG_ESELIB_D("%s Exit \n", __FUNCTION__);
  return wConfigStatus;
}

/******************************************************************************
 * Function         phNxpEse_open
 *
 * Description      This function is called by Jni during the
 *                  initialization of the ESE. It opens the physical connection
 *                  with ESE and creates required client thread for
 *                  operation.
 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_open(phNxpEse_initParams initParams) {
  //    phPalEse_Config_t tPalConfig;
  ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
#ifdef SPM_INTEGRATED
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;
  spm_state_t current_spm_state = SPM_STATE_INVALID;
#endif

  /* initialize trace level */
  phNxpLog_SPIInitializeLogLevel();

  NXPLOG_ESELIB_D("phNxpEse_open executed.");

  /*When spi channel is already opened return status as FAILED*/
  if (nxpese_ctxt.EseLibStatus != ESE_STATUS_CLOSE) {
    NXPLOG_ESELIB_I("eSE driver already opened :: fd = [%d]",
                    (int)(intptr_t)nxpese_ctxt.pDevHandle);
    return ESESTATUS_BUSY;
  }

  phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
  NXPLOG_ESELIB_D("MW SEAccessKit Version");
  NXPLOG_ESELIB_D("Android Version:0x%x", NXP_ANDROID_VER);
  NXPLOG_ESELIB_D("Major Version:0x%x", ESELIB_MW_VERSION_MAJ);
  NXPLOG_ESELIB_D("Minor Version:0x%x", ESELIB_MW_VERSION_MIN);

  nxpese_ctxt.pwr_scheme = PN67T_POWER_SCHEME;

  //    tPalConfig.pDevName = (int8_t *) "/dev/p61";

  /* Initialize PAL layer */
  wConfigStatus = phPalEse_open_and_configure(&gPalConfig);
  if (wConfigStatus == ESESTATUS_BUSY) {
    return wConfigStatus;
  } else if (wConfigStatus == ESESTATUS_FEATURE_NOT_SUPPORTED) {
    phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
    return wConfigStatus;
  } else if (wConfigStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phPalEse_Init Failed");
    goto clean_and_return_2;
  }

  /* Copying device handle to ESE Lib context*/
  nxpese_ctxt.pDevHandle = gPalConfig.pDevHandle;
#ifdef SPM_INTEGRATED
  /* Get the Access of ESE*/
  wSpmStatus = phNxpEse_SPM_Init(nxpese_ctxt.pDevHandle);
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_SPM_Init Failed");
    wConfigStatus = ESESTATUS_FAILED;
    goto clean_and_return_2;
  }

#if (NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
  if (chipType != SN100X) {
    wConfigStatus = phNxpEse_checkFWDwnldStatus();
    if (wConfigStatus != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E(
          "Failed to open SPI due to VEN pin used by FW download \n");
      wConfigStatus = ESESTATUS_FAILED;
      goto clean_and_return_1;
    }
  }
#endif
  wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E(" %s : phNxpEse_SPM_GetPwrState Failed", __FUNCTION__);
    wConfigStatus = ESESTATUS_FAILED;
    goto clean_and_return_1;
  } else {
    NXPLOG_ESELIB_D(" %s : current_spm_state = 0x%X", __FUNCTION__,
                    current_spm_state);
    if ((current_spm_state & SPM_STATE_SPI) |
        (current_spm_state & SPM_STATE_SPI_PRIO)) {
      NXPLOG_ESELIB_E(
          " %s : SPI is already opened...second instance not allowed",
          __FUNCTION__);
      wConfigStatus = ESESTATUS_FAILED;
      goto clean_and_return_1;
    }
  }
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
  if (current_spm_state & SPM_STATE_JCOP_DWNLD) {
    NXPLOG_ESELIB_E(" %s : Denying to open JCOP Download in progress",
                    __FUNCTION__);
    wConfigStatus = ESESTATUS_FAILED;
    goto clean_and_return_1;
  }
#endif
  phNxpEse_memcpy(&nxpese_ctxt.initParams, &initParams,
                  sizeof(phNxpEse_initParams));
  /* Updating ESE power state based on the init mode */
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
  if (ESE_MODE_OSU == nxpese_ctxt.initParams.initMode) {
    wConfigStatus = phNxpEse_checkJcopDwnldState();
    if (wConfigStatus != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("phNxpEse_checkJcopDwnldState failed");
      goto clean_and_return_1;
    }
  }
#endif
  if (chipType == SN100X)
    wSpmStatus = ESESTATUS_SUCCESS;
  else
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_ENABLE);
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: enabling power Failed");
    if (wSpmStatus == ESESTATUS_BUSY) {
      wConfigStatus = ESESTATUS_BUSY;
    } else if (wSpmStatus == ESESTATUS_DWNLD_BUSY) {
      wConfigStatus = ESESTATUS_DWNLD_BUSY;
    } else {
      wConfigStatus = ESESTATUS_FAILED;
    }
    goto clean_and_return;
  } else {
    NXPLOG_ESELIB_D("nxpese_ctxt.spm_power_state TRUE");
    nxpese_ctxt.spm_power_state = TRUE;
  }
#endif

#ifndef SPM_INTEGRATED
  if (chipType == SN100X)
    wConfigStatus = ESESTATUS_SUCCESS;
  else
    wConfigStatus =
        phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
  if (wConfigStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
    goto clean_and_return;
  }
#endif

  NXPLOG_ESELIB_D("wConfigStatus %x", wConfigStatus);

  if (wConfigStatus == ESESTATUS_SUCCESS) {
    if (chipType == SN100X) {
      usleep(ESE_OPEN_SLEEP_TIME);
      bOpenResult =
          (phNxpEse_init(initParams) == ESESTATUS_SUCCESS) ? TRUE : FALSE;
      if (!bOpenResult) {
        NXPLOG_ESELIB_E("phNxpEse_init failed");
        wConfigStatus = ESESTATUS_FAILED;
        phNxpEse_coldReset();
        goto clean_and_return;
      }
    }
  } else {
    NXPLOG_ESELIB_E("wConfigStatus %x", wConfigStatus);
  }
  nxpese_ctxt.EseLibStatus = ESE_STATUS_OPEN;
  return wConfigStatus;

clean_and_return:
#ifdef SPM_INTEGRATED
  wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_DISABLE);
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: disabling power Failed");
  }
clean_and_return_1:
  phNxpEse_SPM_DeInit();
clean_and_return_2:
#endif
  if (NULL != nxpese_ctxt.pDevHandle) {
    phPalEse_close(nxpese_ctxt.pDevHandle);
    phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
  }

  nxpese_ctxt.EseLibStatus = ESE_STATUS_CLOSE;
  nxpese_ctxt.spm_power_state = FALSE;
  return ESESTATUS_FAILED;
}
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
/******************************************************************************
 * Function         phNxpEse_setJcopDwnldState
 *
 * Description      This function is  used to check whether JCOP OS
 *                  download can be started or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_FAILED
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_setJcopDwnldState(phNxpEse_JcopDwnldState state) {
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;
  ESESTATUS wConfigStatus = ESESTATUS_FAILED;

  wSpmStatus = phNxpEse_SPM_SetJcopDwnldState(state);
  if (wSpmStatus == ESESTATUS_SUCCESS) {
    wConfigStatus = ESESTATUS_SUCCESS;
  }

  return wConfigStatus;
}

/******************************************************************************
 * Function         phNxpEse_checkJcopDwnldState
 *
 * Description      This function is  used to check whether JCOP OS
 *                  download can be started or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_BUSY
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_checkJcopDwnldState(void) {
  NXPLOG_ESELIB_D("phNxpEse_checkJcopDwnld Enter");
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;
  spm_state_t current_spm_state = SPM_STATE_INVALID;
  uint8_t ese_dwnld_retry = 0x00;
  ESESTATUS status = ESESTATUS_FAILED;

  wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
  if (wSpmStatus == ESESTATUS_SUCCESS) {
    /* Check current_spm_state and update config/Spm status*/
    if ((current_spm_state & SPM_STATE_JCOP_DWNLD) ||
        (current_spm_state & SPM_STATE_WIRED))
      return ESESTATUS_BUSY;

    status = phNxpEse_setJcopDwnldState(JCP_DWNLD_INIT);
    if (status == ESESTATUS_SUCCESS) {
      while (ese_dwnld_retry < ESE_JCOP_OS_DWNLD_RETRY_CNT) {
        NXPLOG_ESELIB_E("ESE_JCOP_OS_DWNLD_RETRY_CNT retry count : %d",
                        ese_dwnld_retry);
        wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
        if (wSpmStatus == ESESTATUS_SUCCESS) {
          if ((current_spm_state & SPM_STATE_JCOP_DWNLD)) {
            status = ESESTATUS_SUCCESS;
            break;
          }
        } else {
          status = ESESTATUS_FAILED;
          break;
        }
        phNxpEse_Sleep(
            100000); /*sleep for 100 ms checking for jcop dwnld status*/
        ese_dwnld_retry++;
      }
    }
  }

  NXPLOG_ESELIB_D("phNxpEse_checkJcopDwnldState status %x", status);
  return status;
}
#endif
/******************************************************************************
 * Function         phNxpEse_Transceive
 *
 * Description      This function update the len and provided buffer
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEse_Transceive(phNxpEse_data *pCmd, phNxpEse_data *pRsp) {
  ESESTATUS status = ESESTATUS_FAILED;

  if ((NULL == pCmd) || (NULL == pRsp)) return ESESTATUS_INVALID_PARAMETER;

  if ((pCmd->len == 0) || pCmd->p_data == NULL) {
    NXPLOG_ESELIB_E(" phNxpEse_Transceive - Invalid Parameter no data");
    return ESESTATUS_INVALID_PARAMETER;
  } else if ((ESE_STATUS_CLOSE == nxpese_ctxt.EseLibStatus)) {
    NXPLOG_ESELIB_E(" %s ESE Not Initialized", __FUNCTION__);
    return ESESTATUS_NOT_INITIALISED;
  } else if ((ESE_STATUS_BUSY == nxpese_ctxt.EseLibStatus)) {
    NXPLOG_ESELIB_E(" %s ESE - BUSY", __FUNCTION__);
    return ESESTATUS_BUSY;
  } else if ((ESE_STATUS_RECOVERY == nxpese_ctxt.EseLibStatus)) {
    NXPLOG_ESELIB_E(" %s ESE - RECOVERY \n", __FUNCTION__);
    return ESESTATUS_RECOVERY_STARTED;
  } else {
    nxpese_ctxt.EseLibStatus = ESE_STATUS_BUSY;
    status = phNxpEseProto7816_Transceive((phNxpEse_data *)pCmd,
                                          (phNxpEse_data *)pRsp);

    if (ESESTATUS_SUCCESS != status) {
      NXPLOG_ESELIB_E(" %s phNxpEseProto7816_Transceive- Failed", __FUNCTION__);
      if (ESESTATUS_TRANSCEIVE_FAILED == status) {
        /*MAX WTX reached*/
        nxpese_ctxt.EseLibStatus = ESE_STATUS_RECOVERY;
      } else {
        /*Timeout/ No response*/
        nxpese_ctxt.EseLibStatus = ESE_STATUS_IDLE;
      }
    } else {
      nxpese_ctxt.EseLibStatus = ESE_STATUS_IDLE;
    }

    NXPLOG_ESELIB_D(" %s Exit status 0x%x \n", __FUNCTION__, status);
    return status;
  }
}

/******************************************************************************
 * Function         phNxpEse_AcquireEseLock
 *
 * Description      This function avoids eSE to enter into standby mode
 *                  until end of apdu command is sent
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEse_AcquireEseLock(bool_t isFirstCommand) {
  ESESTATUS status = ESESTATUS_FAILED;

  uint8_t pDummyApdu[] = {0, 0, 0, 0};

  phNxpEse_data pCmd;
  phNxpEse_data pRsp;

  phNxpEse_memset(&pCmd, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&pRsp, 0x00, sizeof(phNxpEse_data));

  pCmd.len = sizeof(pDummyApdu);
  pCmd.p_data = (uint8_t *)phNxpEse_memalloc(pCmd.len);

  if (pCmd.p_data != NULL) {
    phNxpEse_memcpy(pCmd.p_data, pDummyApdu, pCmd.len);
  } else {
    ALOGE(" %s Resource allocation failed \n", __FUNCTION__);
    return status;
  }

  if ((ESE_STATUS_CLOSE == nxpese_ctxt.EseLibStatus)) {
    ALOGE(" %s ESE Not Initialized \n", __FUNCTION__);
    return ESESTATUS_NOT_INITIALISED;
  } else if ((ESE_STATUS_BUSY == nxpese_ctxt.EseLibStatus)) {
    ALOGE(" %s ESE - BUSY \n", __FUNCTION__);
    return ESESTATUS_BUSY;
  } else {
    nxpese_ctxt.EseLibStatus = ESE_STATUS_BUSY;
    status = phNxpEseProto7816_AcquireLock(
        (phNxpEse_data *)&pCmd, (phNxpEse_data *)&pRsp, isFirstCommand);
    if (ESESTATUS_SUCCESS != status) {
      ALOGE(" %s phNxpEse_AcquireEseLock- Failed \n", __FUNCTION__);
    }
    nxpese_ctxt.EseLibStatus = ESE_STATUS_IDLE;
    nxpese_ctxt.rnack_sent = FALSE;

    phNxpEse_free(pCmd.p_data);
    phNxpEse_free(pRsp.p_data);

    NXPLOG_ESELIB_D(" %s Exit status 0x%x \n", __FUNCTION__, status);
    return status;
  }
}

/******************************************************************************
 * Function         phNxpEse_coldReset
 *
 * Description      This function power cycles the ESE
 *                  (cold reset by prop. FW command) interface by
 *                  talking to NFC HAL
 *
 *                  Note:
 *                  After cold reset, phNxpEse_init need to be called to
 *                  reset the host AP T=1 stack parameters
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is
 *successful else
 *                  ESESTATUS_FAILED(1)
 ******************************************************************************/
ESESTATUS phNxpEse_coldReset(void) {
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;

  NXPLOG_ESELIB_D(" %s Enter \n", __FUNCTION__);

  if (chipType == SN100X) {
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_RECOVERY_RESET);
  } else {
    wSpmStatus = ESESTATUS_FAILED;
    NXPLOG_ESELIB_E(" %s Function not supported \n", __FUNCTION__);
  }

  NXPLOG_ESELIB_D(" %s Exit status 0x%x \n", __FUNCTION__, wSpmStatus);

  return wSpmStatus;
}

ESESTATUS phNxpEse_wtxNotice(uint8_t flag) {
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;

  NXPLOG_ESELIB_D(" %s Enter \n", __FUNCTION__);

  if (chipType == SN100X) {
    (void)flag;
  } else {
    wSpmStatus = ESESTATUS_FAILED;
    NXPLOG_ESELIB_E(" %s Function not supported \n", __FUNCTION__);
  }

  NXPLOG_ESELIB_D(" %s Exit status 0x%x \n", __FUNCTION__, wSpmStatus);

  return wSpmStatus;
}

/******************************************************************************
 * Function         phNxpEse_reset
 *
 * Description      This function reset the ESE interface and free all
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is
 *successful else ESESTATUS_FAILED(1)
 ******************************************************************************/
ESESTATUS phNxpEse_reset(void) {
  ESESTATUS status = ESESTATUS_SUCCESS;

#ifdef SPM_INTEGRATED
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;
#endif

  /* TBD : Call the ioctl to reset the ESE */
  NXPLOG_ESELIB_D(" %s Enter \n", __FUNCTION__);
  /* Do an interface reset, don't wait to see if JCOP went through a full power
   * cycle or not */
  status = phNxpEseProto7816_IntfReset();
  if (status) {
    NXPLOG_ESELIB_E("%s Ese status Failed", __FUNCTION__);
    status = ESESTATUS_FAILED;
  }
#ifdef SPM_INTEGRATED
  if ((nxpese_ctxt.pwr_scheme == PN67T_POWER_SCHEME) ||
      (nxpese_ctxt.pwr_scheme == PN80T_LEGACY_SCHEME)) {
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_RESET);
    if (wSpmStatus != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: reset Failed");
    }
  }
#else
  /* if arg ==2 (hard reset)
   * if arg ==1 (soft reset)
   */
  status = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
  if (status != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_reset Failed");
  }
#endif

  phNxpEse_setIfs(
      phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLenIFSC);
  NXPLOG_ESELIB_D(" %s Exit \n", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_resetJcopUpdate
 *
 * Description      This function reset the ESE interface during JCOP Update
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is
 *successful else ESESTATUS_FAILED(1)
 ******************************************************************************/
ESESTATUS phNxpEse_resetJcopUpdate(void) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  uint8_t retry = 0;

  /* TBD : Call the ioctl to reset the  */
  NXPLOG_ESELIB_D(" %s Enter", __FUNCTION__);

  /* Reset interface after every reset irrespective of
  whether JCOP did a full power cycle or not. */
  do {
    status = phNxpEseProto7816_Reset();
    if (status != ESESTATUS_SUCCESS) phNxpEse_SPM_ConfigPwr(SPM_RECOVERY_RESET);
  } while (status != ESESTATUS_SUCCESS && retry++ < 1);
#ifdef SPM_INTEGRATED
  NXPLOG_ESELIB_D(" %s Call Config Pwr Reset \n", __FUNCTION__);
  if (chipType == SN100X) {
    status = phNxpEse_chipReset();
    if (status != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate: chip reset Failed");
      status = ESESTATUS_FAILED;
    }
  } else {
    status = phNxpEse_SPM_ConfigPwr(SPM_POWER_RESET);
    if (status != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate: reset Failed");
      status = ESESTATUS_FAILED;
    }
  }
#else
  /* if arg ==2 (hard reset)
   * if arg ==1 (soft reset)
   */
  status = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
  if (status != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate Failed");
  }
#endif

  NXPLOG_ESELIB_D(" %s Exit \n", __FUNCTION__);
  return status;
}
/******************************************************************************
 * Function         phNxpEse_EndOfApdu
 *
 * Description      This function is used to send S-frame to indicate
 *END_OF_APDU
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is
 *successful else ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_EndOfApdu(void) {
  ESESTATUS status = ESESTATUS_SUCCESS;
#if (NXP_ESE_END_OF_SESSION == TRUE)
  status = phNxpEseProto7816_Close();
#endif
  return status;
}

/******************************************************************************
 * Function         phNxpEse_chipReset
 *
 * Description      This function is used to reset the ESE.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_chipReset(void) {
  ESESTATUS status = ESESTATUS_FAILED;
  ESESTATUS bStatus = ESESTATUS_FAILED;
  if (nxpese_ctxt.pwr_scheme == PN80T_EXT_PMU_SCHEME) {
    bStatus = phNxpEseProto7816_Reset();
    if (bStatus != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E(
          "Inside phNxpEse_chipReset, phNxpEseProto7816_Reset Failed");
    }
    status = phPalEse_ioctl(phPalEse_e_ChipRst, nxpese_ctxt.pDevHandle, 6);
    if (status != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("phNxpEse_chipReset  Failed");
    }
  } else {
    NXPLOG_ESELIB_E(
        "phNxpEse_chipReset is not supported in legacy power scheme");
    status = ESESTATUS_FAILED;
  }
  return status;
}

/******************************************************************************
 * Function         phNxpEse_GetOsMode
 *
 * Description      This function is used to get OS mode(JCOP/OSU)
 *
 * Returns          0x01 : JCOP_MODE
 *                  0x02 : OSU_MODE
 *
 ******************************************************************************/
phNxpEseProto7816_OsType_t phNxpEse_GetOsMode(void) {
  return phNxpEseProto7816_GetOsMode();
}

/******************************************************************************
 * Function         phNxpEse_isColdResetRequired
 *
 * Description      This function determines whether hard reset recovery is
 *                  required or not on protocol recovery failure.
 * Returns          TRUE(required)/FALSE(not required).
 *
 ******************************************************************************/
static __inline bool_t phNxpEse_isColdResetRequired(phNxpEse_initMode mode,
                                                    ESESTATUS status) {
  return (mode == ESE_MODE_OSU && status != ESESTATUS_SUCCESS);
}

/******************************************************************************
 * Function         phNxpEse_deInit
 *
 * Description      This function de-initializes all the ESE protocol params
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_deInit(void) {
  ESESTATUS status = ESESTATUS_SUCCESS;

  if (ESE_STATUS_RECOVERY == nxpese_ctxt.EseLibStatus) {
    return status;
  }

  status = phNxpEseProto7816_Close();
  nxpese_ctxt.EseLibStatus = ESE_STATUS_CLOSE;
  return status;
}

/******************************************************************************
 * Function         phNxpEse_close
 *
 * Description      This function close the ESE interface and free all
 *                  resources.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_close(void) {
  ESESTATUS status = ESESTATUS_SUCCESS;

  if ((ESE_STATUS_CLOSE == nxpese_ctxt.EseLibStatus)) {
    NXPLOG_ESELIB_E(" %s ESE Not Initialized \n", __FUNCTION__);
    return ESESTATUS_NOT_INITIALISED;
  }

#ifdef SPM_INTEGRATED
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;

  if (chipType == SN100X) {
    if (bOpenResult == TRUE) {
      phNxpEse_deInit();
      NXPLOG_ESELIB_I("phNxpEse_deInit is done in factory Bin!");
    }
  }

  /* Release the Access of  */
  wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_DISABLE);
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: disabling power Failed");
  } else {
    nxpese_ctxt.spm_power_state = FALSE;
  }
#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
  if (ESE_MODE_OSU == nxpese_ctxt.initParams.initMode) {
    status = phNxpEse_setJcopDwnldState(JCP_SPI_DWNLD_COMPLETE);
    if (status != ESESTATUS_SUCCESS) {
      NXPLOG_ESELIB_E("%s: phNxpEse_setJcopDwnldState failed", __FUNCTION__);
    }
  }
#endif
  if (chipType == SN100X) {
    if (nxpese_ctxt.EseLibStatus == ESE_STATUS_RECOVERY ||
        (ESESTATUS_SUCCESS != phNxpEseProto7816_CloseAllSessions())) {
      NXPLOG_ESELIB_D("eSE not responding perform hard reset");
      //            phNxpEse_SPM_ConfigPwr(SPM_RECOVERY_RESET);
      phNxpEse_coldReset();
    }
  }

  wSpmStatus = phNxpEse_SPM_DeInit();
  if (wSpmStatus != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_SPM_DeInit Failed");
  }
#endif
  if (NULL != nxpese_ctxt.pDevHandle) {
    phPalEse_close(nxpese_ctxt.pDevHandle);
    phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
    NXPLOG_ESELIB_D("phNxpEse_close - ESE Context deinit completed");
  }
  nxpese_ctxt.EseLibStatus = ESE_STATUS_CLOSE;

  NXPLOG_ESELIB_I("eSE driver closed");

  /* Return success always */
  return status;
}

/******************************************************************************
 * Function         phNxpEse_read
 *
 * Description      This function write the data to ESE through physical
 *                  interface (e.g. I2C) using the  driver interface.
 *                  Before sending the data to ESE, phNxpEse_write_ext
 *                  is called to check if there is any extension processing
 *                  is required for the SPI packet being sent out.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if read successful else
 *                  ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_read(uint32_t *data_len, uint8_t **pp_data) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  int ret = -1;

  NXPLOG_ESELIB_D("%s Enter ..", __FUNCTION__);

  ret = phNxpEse_readPacket(nxpese_ctxt.pDevHandle, nxpese_ctxt.p_read_buff,
                            MAX_DATA_LEN);
  if (ret < 0) {
    NXPLOG_ESELIB_E("PAL Read status error status = %x", status);
    *data_len = 2;
    *pp_data = nxpese_ctxt.p_read_buff;
    status = ESESTATUS_FAILED;
  } else {
    phPalEse_print_packet("RECV", nxpese_ctxt.p_read_buff, ret);
    *data_len = ret;
    *pp_data = nxpese_ctxt.p_read_buff;
    status = ESESTATUS_SUCCESS;
  }

  NXPLOG_ESELIB_D("%s Exit", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_readPacket
 *
 * Description      This function Reads requested number of bytes from
 *                  pn547 device into given buffer.
 *
 * Returns          nNbBytesToRead- number of successfully read bytes
 *                  -1        - read operation failure
 *
 ******************************************************************************/
static int phNxpEse_readPacket(void *pDevHandle, uint8_t *pBuffer,
                               int nNbBytesToRead) {
  if (chipType == SN100X) {
    int ret = -1;
    int sof_counter = 0; /* one read may take 1 ms*/
    int total_count = 0, numBytesToRead = 0, headerIndex = 0;
    int max_sof_counter = 0;

    NXPLOG_ESELIB_D("%s Enter", __FUNCTION__);
    /*Max retry to get SOF in case of chaining*/
    if (poll_sof_chained_delay == 1) {
      /*Wait Max for 1.3 sec before retry/recvoery*/
      /*(max_sof_counter(1300) * 10 us) = 1.3 sec */
      max_sof_counter = ESE_POLL_TIMEOUT * 10;
    }
    /*Max retry to get SOF in case of Non-chaining*/
    else {
      /*wait based on config option */
      /*(nadPollingRetryTime * WAKE_UP_DELAY * NAD_POLLING_SCALER)*/
      max_sof_counter =
          ((ESE_POLL_TIMEOUT * 1000) / (nxpese_ctxt.nadPollingRetryTime *
                                        WAKE_UP_DELAY * NAD_POLLING_SCALER));
    }
    if (nxpese_ctxt.rnack_sent) {
      phPalEse_sleep(nxpese_ctxt.invalidFrame_Rnack_Delay);
    }

    do {
      ret = -1;
      ret = phPalEse_read(pDevHandle, pBuffer, 2);
      if (ret < 0) {
        /*Polling for read on spi, hence Debug log*/
        NXPLOG_ESELIB_D("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
      }
      if ((pBuffer[0] == nxpese_ctxt.nadInfo.nadRx) ||
          (pBuffer[0] == RECIEVE_PACKET_SOF)) {
        /* Read the HEADR of one byte*/
        NXPLOG_ESELIB_D("%s Read HDR SOF + PCB", __FUNCTION__);
        numBytesToRead = 1; /*Read only INF LEN*/
        headerIndex = 1;
        break;
      } else if ((pBuffer[1] == nxpese_ctxt.nadInfo.nadRx) ||
                 (pBuffer[1] == RECIEVE_PACKET_SOF)) {
        /* Read the HEADR of Two bytes*/
        NXPLOG_ESELIB_D("%s Read HDR only SOF", __FUNCTION__);
        pBuffer[0] = pBuffer[1];
        numBytesToRead = 2; /*Read PCB + INF LEN*/
        headerIndex = 0;
        break;
      }

      /*If it is Chained packet wait for 100 usec*/
      if (poll_sof_chained_delay == 1) {
        NXPLOG_ESELIB_D("%s Chained Pkt, delay read %dus", __FUNCTION__,
                        WAKE_UP_DELAY * CHAINED_PKT_SCALER);
        phPalEse_sleep(WAKE_UP_DELAY * CHAINED_PKT_SCALER);
      } else {
        /*DLOG_IF(INFO, ese_debug_enabled)
          << StringPrintf("%s Normal Pkt, delay read %dus", __FUNCTION__,
          WAKE_UP_DELAY * NAD_POLLING_SCALER);*/
        phPalEse_sleep(nxpese_ctxt.nadPollingRetryTime * WAKE_UP_DELAY *
                       NAD_POLLING_SCALER);
      }
      sof_counter++;
    } while (sof_counter < max_sof_counter);

    /*SOF Read timeout happened, go for frame retransmission*/
    if (sof_counter == max_sof_counter) {
      ret = -1;
    }
    if ((pBuffer[0] == nxpese_ctxt.nadInfo.nadRx) ||
        (pBuffer[0] == RECIEVE_PACKET_SOF)) {
      NXPLOG_ESELIB_D("%s SOF FOUND", __FUNCTION__);
      /* Read the HEADR of one/Two bytes based on how two bytes read A5 PCB or
       * 00 A5*/
      ret =
          phPalEse_read(pDevHandle, &pBuffer[1 + headerIndex], numBytesToRead);
      if (ret < 0) {
        NXPLOG_ESELIB_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
      }
      if ((pBuffer[1] == CHAINED_PACKET_WITHOUTSEQN) ||
          (pBuffer[1] == CHAINED_PACKET_WITHSEQN)) {
        poll_sof_chained_delay = 1;
        NXPLOG_ESELIB_D("poll_sof_chained_delay value is %d ",
                        poll_sof_chained_delay);
      } else {
        poll_sof_chained_delay = 0;
        NXPLOG_ESELIB_D("poll_sof_chained_delay value is %d ",
                        poll_sof_chained_delay);
      }
      total_count = 3;
      uint8_t pcb;
      phNxpEseProto7816_PCB_bits_t pcb_bits;
      pcb = pBuffer[PH_PROPTO_7816_PCB_OFFSET];

      phNxpEse_memset(&pcb_bits, 0x00, sizeof(phNxpEseProto7816_PCB_bits_t));
      phNxpEse_memcpy(&pcb_bits, &pcb, sizeof(uint8_t));

      /*For I-Frame Only*/
      if (0 == pcb_bits.msb) {
        if (pBuffer[2] != EXTENDED_FRAME_MARKER) {
          nNbBytesToRead = pBuffer[2];
          headerIndex = 3;
        } else {
          ret = phPalEse_read(pDevHandle, &pBuffer[3], 2);
          if (ret < 0) {
            NXPLOG_ESELIB_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
          }
          NXPLOG_ESELIB_E("_spi_read() [HDR]errno 4444444 %d", ret);
          nNbBytesToRead = (pBuffer[3] << 8);
          nNbBytesToRead = nNbBytesToRead | pBuffer[4];
          total_count += 2;
          headerIndex = 5;
        }
      }
      /*For Non-IFrame*/
      else {
        nNbBytesToRead = pBuffer[2];
        headerIndex = 3;
      }
      /* Read the Complete data + one byte CRC*/
      ret = phPalEse_read(pDevHandle, &pBuffer[headerIndex],
                          (nNbBytesToRead + 1));
      if (ret < 0) {
        NXPLOG_ESELIB_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
        ret = -1;
      } else {
        ret = (total_count + (nNbBytesToRead + 1));
        /*If I-Frame received with invalid length respond with RNACK*/
        if ((0 == pcb_bits.msb) &&
            ((nNbBytesToRead == 0) ||
             (nNbBytesToRead > phNxpEseProto7816_GetIfs()))) {
          NXPLOG_ESELIB_E("I-Frame with invalid len == %d", nNbBytesToRead);
          pBuffer[0] = 0x90;
          pBuffer[1] = RECIEVE_PACKET_SOF;
          ret = 0x02;
        }
      }
      nxpese_ctxt.rnack_sent = FALSE;
    } else if (ret < 0) {
      /*In case of IO Error*/
      ret = -2;
      pBuffer[0] = 0x64;
      pBuffer[1] = 0xFF;
    } else { /* Received corrupted frame:
                Flushing out data in the Rx buffer so that Card can switch the
                mode */
      uint16_t ifsd_size = phNxpEseProto7816_GetIfs();
      uint32_t total_frame_size = 0;
      NXPLOG_ESELIB_E("_spi_read() corrupted, IFSD size=%d flushing it out!!",
                      ifsd_size);
      /* If a non-zero byte is received while polling for NAD byte and the byte
         is not a valid NAD byte (0xA5 or 0xB4): 1)  Read & discard (without
         de-asserting SPI CS line) : a.  Max IFSD size + 5 (remaining four
         prologue + one LRC bytes) bytes from eSE  if max IFS size is greater
         than 254 bytes OR b.  Max IFSD size + 3 (remaining two prologue + one
         LRC bytes) bytes from eSE  if max IFS size is less than 255 bytes.

          2) Send R-NACK to request eSE to re-transmit the frame*/

      if (ifsd_size > IFSC_SIZE_SEND) {
        total_frame_size = ifsd_size + 4;
      } else {
        total_frame_size = ifsd_size + 2;
      }
      nxpese_ctxt.rnack_sent = TRUE;
      phPalEse_sleep(nxpese_ctxt.invalidFrame_Rnack_Delay);
      ret = phPalEse_read(pDevHandle, &pBuffer[2], total_frame_size);
      if (ret < 0) {
        NXPLOG_ESELIB_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
      } else { /* LRC fail expected for this frame to send R-NACK*/
        ret = total_frame_size + 2;
        NXPLOG_ESELIB_E(
            "_spi_read() SUCCESS  ret : %X LRC fail excpected for this frame",
            ret);
        phPalEse_print_packet("RECV", pBuffer, ret);
      }
      pBuffer[0] = 0x90;
      pBuffer[1] = RECIEVE_PACKET_SOF;
      ret = 0x02;
    }

    NXPLOG_ESELIB_D("%s Exit ret = %d", __FUNCTION__, ret);
    return ret;
  } else {
    int ret = -1;
    int sof_counter = 0; /* one read may take 1 ms*/
    int total_count = 0;

    NXPLOG_ESELIB_D("%s Enter", __FUNCTION__);
    do {
      sof_counter++;
      ret = -1;
      ret = phPalEse_read(pDevHandle, pBuffer, 1);
      if (ret < 0) {
        NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
      }
      if (pBuffer[0] != RECIEVE_PACKET_SOF)
        phPalEse_sleep(WAKE_UP_DELAY_PN80T); /*sleep for 1ms*/
    } while ((pBuffer[0] != RECIEVE_PACKET_SOF) &&
             (sof_counter < ESE_POLL_TIMEOUT));

    if (pBuffer[0] == RECIEVE_PACKET_SOF) {
      NXPLOG_ESELIB_D("SOF: 0x%x", pBuffer[0]);
      total_count = 1;
      /* Read the HEADR of Two bytes*/
      ret = phPalEse_read(pDevHandle, &pBuffer[1], 2);
      if (ret < 0) {
        NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
      } else {
        total_count += 2;
        /* Get the data length */
        nNbBytesToRead = pBuffer[2];
        NXPLOG_ESELIB_D("lenght 0x%x", nNbBytesToRead);

        ret = phPalEse_read(pDevHandle, &pBuffer[3], (nNbBytesToRead + 1));
        if (ret < 0) {
          NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
        } else {
          ret = (total_count + (nNbBytesToRead + 1));
        }
      }
    } else {
      NXPLOG_PAL_E("RESPONSE TIME OUT");
      ret = -1;
    }
    NXPLOG_ESELIB_D("%s Exit ret = %d", __FUNCTION__, ret);
    return ret;
  }
}

/******************************************************************************
 * Function         phNxpEse_WriteFrame
 *
 * Description      This is the actual function which is being called by
 *                  phNxpEse_write. This function writes the data to ESE.
 *                  It waits till write callback provide the result of write
 *                  process.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if write successful else
 *                  ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_WriteFrame(uint32_t data_len, uint8_t *p_data) {
  ESESTATUS status = ESESTATUS_INVALID_PARAMETER;
  int32_t dwNoBytesWrRd = 0;
  NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);

  p_data[0] = nxpese_ctxt.nadInfo.nadTx;

  /* Create local copy of cmd_data */
  phNxpEse_memcpy(nxpese_ctxt.p_cmd_data, p_data, data_len);
  nxpese_ctxt.cmd_len = data_len;

  dwNoBytesWrRd = phPalEse_write(nxpese_ctxt.pDevHandle, nxpese_ctxt.p_cmd_data,
                                 nxpese_ctxt.cmd_len);
  if (-1 == dwNoBytesWrRd) {
    NXPLOG_PAL_E(" - Error in SPI Write.....%d\n", errno);
    status = ESESTATUS_FAILED;
  } else {
    status = ESESTATUS_SUCCESS;
    phPalEse_print_packet("SEND", nxpese_ctxt.p_cmd_data, nxpese_ctxt.cmd_len);
  }

  NXPLOG_ESELIB_D("Exit %s status %x\n", __FUNCTION__, status);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_getAtr
 *
 * Description      This function retrieves ATR bytes from 7816-3 layer
 *Update.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if write successful else
 *                  ESESTATUS_FAILED(1
 *
 ******************************************************************************/
ESESTATUS phNxpEse_getAtr(phNxpEse_data *pATR) {
  ESESTATUS status = ESESTATUS_FAILED;
  status = phNxpEseProto7816_getAtr(pATR);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_atr
 *
 * Description      This function retrieves ATR bytes from 7816-3 layer
 *Update.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if write successful else
 *                  ESESTATUS_FAILED(1
 *
 ******************************************************************************/
ESESTATUS phNxpEse_atr(void) {
  ESESTATUS status = ESESTATUS_FAILED;

  status = phNxpEseProto7816_Reset();
  if (status != ESESTATUS_SUCCESS) {
    NXPLOG_ESELIB_E("phNxpEse_atr Failed");
    status = ESESTATUS_FAILED;
  }
  phNxpEse_setIfs(
      phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLenIFSC);
  return status;
}

/******************************************************************************
 * Function         phNxpEse_setIfs
 *
 * Description      This function sets the IFS size to 240/254 support JCOP OS
 *Update.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_setIfs(uint16_t IFS_Size) {
  if (chipType == SN100X) {
    if (IFS_Size > ESE_IFSD_VALUE) IFS_Size = ESE_IFSD_VALUE;
    phNxpEseProto7816_SetIfs(IFS_Size);
  }
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_setIfsc
 *
 * Description      This function sets the IFSC size to 240/254 support JCOP OS
 *Update.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_setIfsc(uint16_t IFSC_Size) {
  /*SET the IFSC size to 240 bytes*/
  phNxpEseProto7816_SetIfscSize(IFSC_Size);
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_Sleep
 *
 * Description      This function  suspends execution of the calling thread for
 *           (at least) usec microseconds
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_Sleep(uint32_t usec) {
  phPalEse_sleep(usec);
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_memset
 *
 * Description      This function updates destination buffer with val
 *                  data in len size
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
void *phNxpEse_memset(void *buff, int val, size_t len) {
  return phPalEse_memset(buff, val, len);
}

/******************************************************************************
 * Function         phNxpEse_memcpy
 *
 * Description      This function copies source buffer to  destination buffer
 *                  data in len size
 *
 * Returns          Return pointer to allocated memory location.
 *
 ******************************************************************************/
void *phNxpEse_memcpy(void *dest, const void *src, size_t len) {
  return phPalEse_memcpy(dest, src, len);
}

/******************************************************************************
 * Function         phNxpEse_Memalloc
 *
 * Description      This function allocation memory
 *
 * Returns          Return pointer to allocated memory or NULL.
 *
 ******************************************************************************/
void *phNxpEse_memalloc(uint32_t size) {
  return phPalEse_memalloc(size);
  ;
}

/******************************************************************************
 * Function         phNxpEse_calloc
 *
 * Description      This is utility function for runtime heap memory allocation
 *
 * Returns          Return pointer to allocated memory or NULL.
 *
 ******************************************************************************/
void *phNxpEse_calloc(size_t datatype, size_t size) {
  return phPalEse_calloc(datatype, size);
}

/******************************************************************************
 * Function         phNxpEse_free
 *
 * Description      This function de-allocation memory
 *
 * Returns         void.
 *
 ******************************************************************************/
void phNxpEse_free(void *ptr) { return phPalEse_free(ptr); }

#if (NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
/******************************************************************************
 * Function         phNxpEse_checkFWDwnldStatus
 *
 * Description      This function is  used to  check whether FW download
 *                  is completed or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_BUSY
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_checkFWDwnldStatus(void) {
  NXPLOG_ESELIB_D("phNxpEse_checkFWDwnldStatus Enter");
  ESESTATUS wSpmStatus = ESESTATUS_SUCCESS;
  spm_state_t current_spm_state = SPM_STATE_INVALID;
  uint8_t ese_dwnld_retry = 0x00;
  ESESTATUS status = ESESTATUS_FAILED;

  wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
  if (wSpmStatus == ESESTATUS_SUCCESS) {
    /* Check current_spm_state and update config/Spm status*/
    while (ese_dwnld_retry < ESE_FW_DWNLD_RETRY_CNT) {
      NXPLOG_ESELIB_D("ESE_FW_DWNLD_RETRY_CNT retry count : %d",
                      ese_dwnld_retry);
      wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
      if (wSpmStatus == ESESTATUS_SUCCESS) {
        if ((current_spm_state & SPM_STATE_DWNLD)) {
          status = ESESTATUS_FAILED;
        } else {
          NXPLOG_ESELIB_D("Exit polling no FW Download ..");
          status = ESESTATUS_SUCCESS;
          break;
        }
      } else {
        status = ESESTATUS_FAILED;
        break;
      }
      phNxpEse_Sleep(500000); /*sleep for 500 ms checking for fw dwnld status*/
      ese_dwnld_retry++;
    }
  }

  NXPLOG_ESELIB_D("phNxpEse_checkFWDwnldStatus status %x", status);
  return status;
}
#endif
