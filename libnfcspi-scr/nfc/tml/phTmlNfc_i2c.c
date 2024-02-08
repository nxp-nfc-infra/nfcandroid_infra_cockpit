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

/*
 * DAL I2C port implementation for linux
 *
 * Project: Trusted NFC Linux
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <hardware/nfc.h>
#include <phNfcStatus.h>
#include <phNxpLog.h>
#include <phTmlNfc_i2c.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define CRC_LEN 2
#define NORMAL_MODE_HEADER_LEN 3
#define FW_DNLD_HEADER_LEN 2
#define FW_DNLD_LEN_OFFSET 1
#define NORMAL_MODE_LEN_OFFSET 2
#define FRAGMENTSIZE_MAX PHNFC_I2C_FRAGMENT_SIZE
#define MAX_I2C_PACKET_SIZE 256
static bool_t bFwDnldFlag = FALSE;
phTmlNfc_i2cfragmentation_t fragmentation_enabled = I2C_FRAGMENATATION_DISABLED;

/*******************************************************************************
**
** Function         phTmlNfc_i2c_close
**
** Description      Closes PN548AD device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phTmlNfc_i2c_close(void *pDevHandle) {
  if (NULL != pDevHandle) {
    close((intptr_t)pDevHandle);
  }

  return;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_open_and_configure
**
** Description      Open and configure PN548AD device
**
** Parameters       pConfig     - hardware information
**                  pLinkHandle - device handle
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS            - open_and_configure operation
*success
**                  NFCSTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
NFCSTATUS phTmlNfc_i2c_open_and_configure(pphTmlNfc_Config_t pConfig,
                                          void **pLinkHandle) {
  int nHandle;

  NXPLOG_TML_D("Opening port=%s\n", pConfig->pDevName);
  /* open port */
  nHandle = open((const char *)pConfig->pDevName, O_RDWR);
  if (nHandle < 0) {
    NXPLOG_TML_E("_i2c_open() Failed: retval %x", nHandle);
    *pLinkHandle = NULL;
    return NFCSTATUS_INVALID_DEVICE;
  }

  *pLinkHandle = (void *)((intptr_t)nHandle);
  NXPLOG_TML_D("_i2c_open() : retval %x", nHandle);
  return NFCSTATUS_SUCCESS;
}

void phTmlNfc_i2c_set_dnld_flag(int value) {
  bFwDnldFlag = (bool_t)value;
  NXPLOG_TML_D("phTmlNfc_i2c_set_dnld_flag, value[%d], bFwDnldFlag[%d]", value,
               bFwDnldFlag);
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_read
**
** Description      Reads requested number of bytes from PN548AD device into
*given buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
static int read_count = 0;
int phTmlNfc_i2c_read(void *pDevHandle, uint8_t *pBuffer, int nNbBytesToRead) {
  int ret_Read;
  int ret_Select;
  int numRead = 0;
  struct timeval tv;
  fd_set rfds;
  uint16_t totalBtyesToRead = 0;
  uint16_t bytesToRead = 0;
  bool_t lbDnldFlag = false;
  UNUSED(nNbBytesToRead);
  if (NULL == pDevHandle) {
    return -1;
  }

  NXPLOG_TML_D("phTmlNfc_i2c_read, enter bFwDnldFlag : %d", bFwDnldFlag);

  lbDnldFlag = bFwDnldFlag;

  if (FALSE == bFwDnldFlag) {
    totalBtyesToRead = NORMAL_MODE_HEADER_LEN;
  } else {
    totalBtyesToRead = FW_DNLD_HEADER_LEN;
  }

  /* Read with 2 second timeout, so that the read thread can be aborted
     when the PN548AD does not respond and we need to switch to FW download
     mode. This should be done via a control socket instead. */
  FD_ZERO(&rfds);
  FD_SET((intptr_t)pDevHandle, &rfds);
  tv.tv_sec = 2;
  tv.tv_usec = 1;

  ret_Select =
      select((int)((intptr_t)pDevHandle + (int)1), &rfds, NULL, NULL, &tv);
  if (ret_Select < 0) {
    NXPLOG_TML_E("i2c select() errno : %x", errno);
    return -1;
  } else if (ret_Select == 0) {
    NXPLOG_TML_E("i2c select() Timeout");
    return -1;
  } else {
  retry_read:
    read_count++;
    ret_Read = read((intptr_t)pDevHandle, pBuffer, totalBtyesToRead - numRead);
    read_count--;
    NXPLOG_TML_D("phTmlNfc_i2c_read, reading [%d]bytes", ret_Read);

    if ((pBuffer[0] == 0) && (pBuffer[1] == 0) &&
        (totalBtyesToRead ==
         2))  //    ((totalBtyesToRead == 2) && (ret_Read<100)))
    {
      NXPLOG_TML_E("invalid read");
      goto retry_read;
    }
    if (ret_Read > 0) {
      numRead += ret_Read;
    } else if (ret_Read == 0) {
      NXPLOG_TML_E("_i2c_read() [hdr]EOF");
      return -1;
    } else {
      NXPLOG_TML_E("_i2c_read() [hdr] errno : %x", errno);
      return -1;
    }

    if (FALSE == lbDnldFlag) {
      totalBtyesToRead = NORMAL_MODE_HEADER_LEN;
    } else {
      totalBtyesToRead = FW_DNLD_HEADER_LEN;
    }

    if (numRead < totalBtyesToRead) {
      ret_Read =
          read((intptr_t)pDevHandle, pBuffer, totalBtyesToRead - numRead);
      if (ret_Read != totalBtyesToRead - numRead) {
        NXPLOG_TML_E("_i2c_read() [hdr] errno : %x", errno);
        return -1;
      } else {
        numRead += ret_Read;
      }
    }
    if (TRUE == lbDnldFlag) {
      totalBtyesToRead = (pBuffer[FW_DNLD_LEN_OFFSET - 1] * 0x100 +
                          pBuffer[FW_DNLD_LEN_OFFSET]) +
                         FW_DNLD_HEADER_LEN + CRC_LEN;
    } else {
      totalBtyesToRead =
          pBuffer[NORMAL_MODE_LEN_OFFSET] + NORMAL_MODE_HEADER_LEN;
    }
    NXPLOG_TML_D("totalBtyesToRead : %d (0x%02x)", totalBtyesToRead,
                 totalBtyesToRead);

    while (totalBtyesToRead > numRead) {
      if (totalBtyesToRead - numRead > MAX_I2C_PACKET_SIZE)
        bytesToRead = MAX_I2C_PACKET_SIZE;
      else
        bytesToRead = totalBtyesToRead - numRead;

      ret_Read = read((intptr_t)pDevHandle, (pBuffer + numRead), bytesToRead);
      if (ret_Read > 0) {
        numRead += ret_Read;
      } else if (ret_Read == 0) {
        NXPLOG_TML_E("_i2c_read() [pyld] EOF");
        return -1;
      } else {
        NXPLOG_TML_E("_i2c_read() [pyld] errno : %x", errno);
        if (errno == EINTR || errno == EAGAIN) {
          return -1;
        }
      }
      NXPLOG_TML_D("Read %d bytes, total Bytes : %d", numRead,
                   totalBtyesToRead);
    }
  }
  NXPLOG_TML_D("numRead : %d (0x%02x)", numRead, numRead);
  NXPLOG_TML_D("I2C Read Exit, lbDnldFlag : %d", lbDnldFlag);

  return numRead;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_write
**
** Description      Writes requested number of bytes from given buffer into
*PN548AD device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phTmlNfc_i2c_write(void *pDevHandle, uint8_t *pBuffer,
                       int nNbBytesToWrite) {
  int ret;
  int numWrote = 0;
  int numBytes = nNbBytesToWrite;
  if (NULL == pDevHandle) {
    return -1;
  }
  if (fragmentation_enabled == I2C_FRAGMENATATION_DISABLED &&
      nNbBytesToWrite > FRAGMENTSIZE_MAX) {
    NXPLOG_TML_E(
        "i2c_write() data larger than maximum I2C  size,enable I2C "
        "fragmentation");
    return -1;
  }
  while (numWrote < nNbBytesToWrite) {
    if (fragmentation_enabled == I2C_FRAGMENTATION_ENABLED &&
        nNbBytesToWrite > FRAGMENTSIZE_MAX) {
      if (nNbBytesToWrite - numWrote > FRAGMENTSIZE_MAX) {
        numBytes = numWrote + FRAGMENTSIZE_MAX;
      } else {
        numBytes = nNbBytesToWrite;
      }
    }
    ret = write((intptr_t)pDevHandle, pBuffer + numWrote, numBytes - numWrote);
    if (ret > 0) {
      numWrote += ret;
      if (fragmentation_enabled == I2C_FRAGMENTATION_ENABLED &&
          numWrote < nNbBytesToWrite) {
        usleep(500);
      }
    } else if (ret == 0) {
      NXPLOG_TML_E("_i2c_write() EOF");
      return -1;
    } else {
      NXPLOG_TML_E("_i2c_write() errno : %x", errno);
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      return -1;
    }
  }

  return numWrote;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_reset
**
** Description      Reset PN548AD device, using VEN pin
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - reset operation success
**                  -1   - reset operation failure
**
*******************************************************************************/
int phTmlNfc_i2c_reset(void *pDevHandle, long level) {
  int ret;
  NXPLOG_TML_D("phTmlNfc_i2c_reset(), VEN level %ld", level);

  if (NULL == pDevHandle) {
    NXPLOG_TML_E("phTmlNfc_i2c_reset(), Error : pDevHandle is NULL");
    return -1;
  }

  ret = ioctl((intptr_t)pDevHandle, PN544_SET_PWR, level);
  if (ret < 0) NXPLOG_TML_E("%s :failed errno = 0x%x", __func__, errno);
  //        ret = ioctl((intptr_t)pDevHandle, PN544_SET_PWR_N, level);
  if ((level == 2 || level == 4) && ret == 0) {
    bFwDnldFlag = TRUE;
  } else {
    bFwDnldFlag = FALSE;
  }
  return ret;
}

/*******************************************************************************
**
** Function         getDownloadFlag
**
** Description      Returns the current mode
**
** Parameters       none
**
** Returns           Current mode download/NCI
*******************************************************************************/
bool_t getDownloadFlag(void) { return bFwDnldFlag; }
