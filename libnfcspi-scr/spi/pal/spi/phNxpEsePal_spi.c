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
 * DAL spi port implementation for linux
 *
 * Project: Trusted ESE Linux
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <phEseStatus.h>
#include <phNxpEsePal.h>
#include <phNxpEsePal_spi.h>
#include <phNxpSpiLog.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_RETRY_CNT 10

extern phPalEse_Config_t gPalConfig;

static void hex_print_tag(char *tag, uint8_t *data, uint32_t len) {
  uint32_t tmpLen = 0;
  if (NULL == data || len < 1 || tag == NULL) {
    NXPLOG_PAL_I("Invalid parameters.");
  } else {
    uint32_t i = 0, write_size = 0, total_size = len;
    char print_buffer[200];
    char temp[3];
    NXPLOG_PAL_I("%s data size : %u", tag, len);
    if (len > 199) {
      tmpLen = 199;
      total_size = 199;
    } else {
      tmpLen = len;
    }

    while (total_size > write_size) {
      memset(print_buffer, 0, 200);
      memset(temp, 0, 3);
      for (i = write_size; i < tmpLen && i - write_size < 36; i++) {
        //            for (i = write_size; i < len && i-write_size < 36 ; i++) {
        snprintf(temp, 3, "%02X", data[i]);
        strcat(print_buffer, temp);
      }
      NXPLOG_PAL_I("0x%s", print_buffer);
      write_size = i;
    }
  }
}

/*******************************************************************************
**
** Function         phPalEse_spi_close
**
** Description      Closes PN547 device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phPalEse_spi_close(void *pDevHandle) {
  if (NULL != pDevHandle) {
    close((intptr_t)pDevHandle);
  }

  return;
}

/*******************************************************************************
**
** Function         phPalEse_spi_open_and_configure
**
** Description      Open and configure pn547 device
**
** Parameters       pConfig     - hardware information
**                  pLinkHandle - device handle
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - open_and_configure operation
*success
**                  ESESTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_spi_open_and_configure(pphPalEse_Config_t pConfig) {
  int nHandle;
  int retryCnt = 0;

  NXPLOG_PAL_D("Opening port=%s\n", pConfig->pDevName);
  /* open port */

retry:
  nHandle = open((char const *)pConfig->pDevName, O_RDWR);
  if (nHandle < 0) {
    NXPLOG_PAL_E("%s : failed errno = 0x%x", __FUNCTION__, errno);
    if (errno == -EBUSY || errno == EBUSY) {
      retryCnt++;
      NXPLOG_PAL_E("Retry open eSE driver, retry cnt : %d", retryCnt);
      if (retryCnt < MAX_RETRY_CNT) {
        phPalEse_sleep(500000);
        goto retry;
      }

      return ESESTATUS_BUSY;
    }
    NXPLOG_PAL_E("_spi_open() Failed: retval %x", nHandle);
    pConfig->pDevHandle = NULL;
    if (errno == -ENOENT || errno == ENOENT) {
      return ESESTATUS_FEATURE_NOT_SUPPORTED;
    } else {
      return ESESTATUS_INVALID_DEVICE;
    }

    return ESESTATUS_INVALID_DEVICE;
  }
  NXPLOG_PAL_I("eSE driver opened :: fd = [%d]", nHandle);
  pConfig->pDevHandle = (void *)((intptr_t)nHandle);
  return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phPalEse_spi_read
**
** Description      Reads requested number of bytes from pn547 device into given
*buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
int phPalEse_spi_read(void *pDevHandle, uint8_t *pBuffer, int nNbBytesToRead) {
  int ret = -1;
  struct ese_ioctl_transfer ptx_data;

  /* prepare the receive buffer */
  ptx_data.tx_buffer = NULL;
  ptx_data.rx_buffer = &pBuffer[0];
  ptx_data.len = nNbBytesToRead;
  NXPLOG_PAL_D("%s Read Requested %d bytes", __FUNCTION__, nNbBytesToRead);
  if (!strncmp((const char *)gPalConfig.pDevName, "/dev/p61", 8))
    ret = phPalEse_spi_ioctl(phPalEse_e_ExchangeSpiData, pDevHandle,
                             (long)&ptx_data);
  else
    ret = read((intptr_t)pDevHandle, (void *)pBuffer, (nNbBytesToRead));

  if (ret < 0) {
    NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
  }
  if (NXPLOG_DEFAULT_LOGLEVEL != NXPLOG_LOG_DEBUG_LOGLEVEL) {
    if (nNbBytesToRead == 1) {
      // Do not print log
    } else if (nNbBytesToRead < 10) {
      hex_print_tag((char *)"[RECV]", (uint8_t *)pBuffer, nNbBytesToRead);
    } else {
      hex_print_tag((char *)"[RECV]", (uint8_t *)pBuffer, 10);
    }
  } else {
    hex_print_tag((char *)"[RECV]", (uint8_t *)pBuffer, nNbBytesToRead);
  }

  NXPLOG_PAL_D(" read returned= %d", ret);
  return ret;
}

/*******************************************************************************
**
** Function         phPalEse_spi_write
**
** Description      Writes requested number of bytes from given buffer into
*pn547 device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phPalEse_spi_write(void *pDevHandle, uint8_t *pBuffer,
                       int nNbBytesToWrite) {
  int ret = -1;
  int numWrote = 0;
  unsigned long int retryCount = 0;
  if (NULL == pDevHandle) {
    NXPLOG_PAL_E("phPalEse_spi_write: received pDevHandle=NULL");
    return -1;
  }

  while (numWrote < nNbBytesToWrite) {
    // usleep(5000);
    if (!strncmp((const char *)gPalConfig.pDevName, "/dev/p61", 8)) {
      struct ese_ioctl_transfer ptx_data;
      /* prepare the transmit buffer */
      ptx_data.rx_buffer = NULL;
      ptx_data.tx_buffer = pBuffer + numWrote;
      ptx_data.len = (unsigned int)(nNbBytesToWrite - numWrote);
      if (NXPLOG_DEFAULT_LOGLEVEL != NXPLOG_LOG_DEBUG_LOGLEVEL) {
        if (nNbBytesToWrite == 1) {
          // Do not print log
        } else if (nNbBytesToWrite < 10) {
          hex_print_tag((char *)"[SEND]", (uint8_t *)pBuffer, nNbBytesToWrite);
        } else {
          hex_print_tag((char *)"[SEND]", (uint8_t *)pBuffer, 10);
        }
      } else {
        hex_print_tag((char *)"[SEND]", (uint8_t *)pBuffer, nNbBytesToWrite);
      }
      ret = phPalEse_spi_ioctl(phPalEse_e_ExchangeSpiData, pDevHandle,
                               (long)&ptx_data);
    } else {
      ret = write((intptr_t)pDevHandle, pBuffer, nNbBytesToWrite);
    }

    if (ret < 0) {
      NXPLOG_PAL_E("_spi_write() errno : %x", errno);
      if ((errno == EINTR || errno == EAGAIN) && (retryCount < MAX_RETRY_CNT)) {
        /*Configure retry count or timeout here,now its configured for 2*10
         * secs*/
        retryCount++;
        /* 5ms delay to give ESE wake up delay */
        phPalEse_sleep(1000 * WAKE_UP_DELAY);
        NXPLOG_PAL_E(
            "phPalEse_spi_ioctl() failed. Going to retry, counter:%ld !",
            retryCount);
        continue;
      }
      return -1;
    }
    numWrote = nNbBytesToWrite;
  }
  return numWrote;
}

/*******************************************************************************
**
** Function         phPalEse_spi_ioctl
**
** Description      Exposed ioctl by p61 spi driver
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - ioctl operation success
**                  -1   - ioctl operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_spi_ioctl(phPalEse_ControlCode_t eControlCode,
                             void *pDevHandle, long level) {
  ESESTATUS ret = ESESTATUS_IOCTL_FAILED;
  NXPLOG_PAL_D("phPalEse_spi_ioctl(), ioctl %x , level %lx", eControlCode,
               level);

  if (NULL == pDevHandle) {
    return (ESESTATUS)-1;
  }
  switch (eControlCode) {
    case phPalEse_e_ResetDevice:
      if (chipType == SN100X)
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_PWR, level);
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_PWR_PN80T, level);
      break;

    case phPalEse_e_EnableLog:
      if (chipType == SN100X)
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_DBG, level);
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_DBG_PN80T, level);
      break;

    case phPalEse_e_EnablePollMode:
      if (chipType == SN100X)
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_POLL, level);
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_POLL_PN80T, level);
      break;

    case phPalEse_e_ChipRst:
      if (chipType == SN100X) {
        if (level == 5) {
          if (!(ioctl((intptr_t)pDevHandle, ESE_PERFORM_COLD_RESET, level)))
            ret = ESESTATUS_SUCCESS;
        } else {
          ret = ESESTATUS_SUCCESS;
        }
      } else {
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_SPM_PWR_PN80T,
                               level);
      }
      break;

    case phPalEse_e_GetSPMStatus:
      if (chipType == SN100X)
        ret = ESESTATUS_SUCCESS;
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_GET_SPM_STATUS_PN80T,
                               level);
      break;

    case phPalEse_e_GetEseAccess:
      if (chipType == SN100X)
        ret = ESESTATUS_SUCCESS;
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_GET_ESE_ACCESS_PN80T,
                               level);
      break;

    case phPalEse_e_ExchangeSpiData:
      /* Transmit data to the device and retrieve data from it simultaneously.*/
      if (chipType == SN100X)
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_RW_SPI_DATA, level);
      else
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_RW_SPI_DATA_PN80T,
                               level);
      break;

#if (NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
    case phPalEse_e_SetClientUpdateState:
      if (chipType != SN100X)
        ret = (ESESTATUS)ioctl((intptr_t)pDevHandle, P61_SET_DWNLD_STATUS_PN80T,
                               level);
      break;
#endif

    default:
      ret = ESESTATUS_IOCTL_FAILED;
      break;
  }
  NXPLOG_PAL_D("Exit  phPalEse_spi_ioctl : ret = %d errno = %d", ret, errno);
  return ret;
}
