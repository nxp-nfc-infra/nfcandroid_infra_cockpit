/*
 * Copyright 2023 NXP.
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only be
 * used strictly in accordance with the applicable license terms. By expressly
 * accepting such terms or by downloading, installing, activating and/or
 * otherwise using the software, you are agreeing that you have read, and that
 * you agree to comply with and are bound by, such license terms. If you do not
 * agree to be bound by the applicable license terms, then you may not
 * retain,install, activate or otherwise use the software.
 */

#include <phDal4Nfc_messageQueueLib.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <phNxpNciHal_utils.h>
#include <scr_common.h>
#include <stdio.h>
#include <string.h>

extern HalCmd_Handler_t gCmd_Handler;
extern scrModes modeType;

void *spiHandlerThread(void *arg);

/*******************************************************************************
**
** Function:       spiEventHandlerThread
**
** Description:    thread to trigger on SPI event
**
** Returns:        None .
**
*******************************************************************************/
void *spiHandlerThread(void *pParam) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  UNUSED(pParam);

  phNxpEse_data pCmdTrans;
  phNxpEse_data pRspTrans;
  phLibNfc_Message_t msg;
  NXPLOG_NCIHAL_D("P7X - SPI Thread Started................\n");

  int8_t timestamp_buf[LEN_TIME_BUF];
  /* Writer thread loop shall be running till shutdown is invoked */
  while (gCmd_Handler.bThreadRunning) {
    if (phDal4Nfc_msgrcv(gCmd_Handler.nClientId, &msg, 0, 0) == -1) {
      NXPLOG_NCIHAL_E("SPI client received bad message");
      continue;
    }

    if (gCmd_Handler.bThreadRunning) {
      switch (msg.eMsgType) {
        case SPI_OPEN: {
          CONCURRENCY_LOCK_SCR();
          ESESTATUS ret = ESESTATUS_DEFAULT;
          phNxpEse_initParams initParams;
          memset(&initParams, 0x00, sizeof(phNxpEse_initParams));
          initParams.initMode = ESE_MODE_NORMAL;
          printf("\nTurn on eSE-SPI\n\n");
          ret = phNxpEse_open(initParams);
          CONCURRENCY_UNLOCK_SCR();

          break;
        }
        case SPI_CLOSE: {
          CONCURRENCY_LOCK_SCR();
          ESESTATUS ret = ESESTATUS_DEFAULT;
          printf("\nTurn off eSE-SPI\n\n");
          ret = phNxpEse_close();
          CONCURRENCY_UNLOCK_SCR();

          break;
        }
        case SPI_PWR_ON: {
          CONCURRENCY_LOCK_SCR();
          ESESTATUS ret = ESESTATUS_DEFAULT;
          phNxpEse_initParams initParams;
          memset(&initParams, 0x00, sizeof(phNxpEse_initParams));
          initParams.initMode = ESE_MODE_NORMAL;
          ret = phNxpEse_open(initParams);
          CONCURRENCY_UNLOCK_SCR();

          break;
        }
        case SPI_PWR_OFF: {
          CONCURRENCY_LOCK_SCR();
          ESESTATUS ret = ESESTATUS_DEFAULT;
          printf("\nTurn off eSE-SPI\n\n");
          ret = phNxpEse_close();
          CONCURRENCY_UNLOCK_SCR();

          break;
        }

        case SPI_TRANSCIEVE: {
          CONCURRENCY_LOCK_SCR();
          memset(&pCmdTrans, 0x00, sizeof(pCmdTrans));
          memset(&pRspTrans, 0x00, sizeof(pRspTrans));
          pCmdTrans.len = msg.Size;
          pCmdTrans.p_data = malloc(pCmdTrans.len);
          memcpy(pCmdTrans.p_data, msg.pMsgData, pCmdTrans.len);

          uint32_t i;
          printf(
              "%s ----------------------------------------SEND_SPI      : %3d "
              "> ",
              GKI_get_time_stamp(timestamp_buf, LEN_TIME_BUF), pCmdTrans.len);
          for (i = 0; i < pCmdTrans.len; i++)
            printf("%02X", *(pCmdTrans.p_data + i));
          printf("\n");

          status = phNxpEse_Transceive(&pCmdTrans, &pRspTrans);
          if (ESESTATUS_SUCCESS == status) {
            uint32_t i;
            printf(
                "%s ----------------------------------------RECEIVE_SPI   : "
                "%3d < ",
                GKI_get_time_stamp(timestamp_buf, LEN_TIME_BUF), pRspTrans.len);
            for (i = 0; i < pRspTrans.len; i++)
              printf("%02X", *(pRspTrans.p_data + i));
            printf("\n");
          } else {
            printf("             Transceive failed !!\n");
          }
          CONCURRENCY_UNLOCK_SCR();

          break;
        }
      }
    } else {
      NXPLOG_NCIHAL_D("P7X - exit thread.....\n");
      break;
    }

  } /* End of While loop */
  if (pCmdTrans.p_data != NULL) free(pCmdTrans.p_data);
  if (msg.pMsgData != NULL) free(msg.pMsgData);
  pthread_exit(NULL);
  return NULL;
}

/******************************************************************************
 * Function         createSPIThread
 *
 * Description      Thread creation for eSE SPI communication.
 *
 * Returns          TRUE on successful thread creation.
 *                  FALSE if thread creation unsuccessful
 *
 ******************************************************************************/
bool_t createSPIThread() {
  pthread_t spiThread;
  bool_t stat = TRUE;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  gCmd_Handler.bThreadRunning = 1;

  if (pthread_create(&spiThread, &attr, spiHandlerThread, NULL) != 0) {
    NXPLOG_NCIHAL_D("Unable to create the thread");
    stat = FALSE;
  }
  pthread_attr_destroy(&attr);
  return stat;
}

/******************************************************************************
 * Function         releaseSPIThread
 *
 * Description      releasing thread by setting bThreadRunning to 0.
 *
 * Returns          void
 *
 ******************************************************************************/
void releaseSPIThread() { gCmd_Handler.bThreadRunning = 0; }

void p_hal_cback(nfc_event_t event, nfc_status_t event_status) {
  (void)event;
  (void)event_status;
}

/******************************************************************************
 * Function         p_hal_data_callback
 *
 * Description      callback function for parsing NCI responses.
 *
 * Parameters       data_len - response data len
 *                  p_data - response data
 *
 * Returns          void
 *
 ******************************************************************************/
void p_hal_data_callback(uint16_t data_len, uint8_t *p_data) {
  char *result;
  int i = 0;

    if(modeType == SOCKET_MODE) {
    /* Send the response to the client */
      send_resp(data_len,p_data);
      return;
    }

  // stop trigger
  while (gCmd_Handler.stopping_buf[i].len != 0) {
    char *rspstr = malloc(sizeof(char) * ((data_len + 1) * 2));
    ToHexStr(p_data, data_len, rspstr, (data_len + 1) * 2);
    printf("stopping_buf[%d].data : %s\n", i,
           gCmd_Handler.stopping_buf[i].data);
    NXPLOG_EXTNS_E("stopping_buf[%d].data : %s", i,
                   gCmd_Handler.stopping_buf[i].data);

    result = strcasestr(rspstr, gCmd_Handler.stopping_buf[i].data);
    free(rspstr);
    if (result) {
      printf("Stopping data recieved !! Exit\n");
      NXPLOG_EXTNS_E("Stopping data recieved !! Exit");
      exit(0);
    }
    i++;
  }

  i = 0;
  // wait and resume
  while (gCmd_Handler.waiting_buf[i].len != 0) {
    char *rspstr = malloc(sizeof(char) * ((data_len + 1) * 2));
    ToHexStr(p_data, data_len, rspstr, (data_len + 1) * 2);
    printf("waiting_buf[%d].data : %s\n", i, gCmd_Handler.waiting_buf[i].data);
    NXPLOG_EXTNS_E("waiting_buf[%d].data : %s", i,
                   gCmd_Handler.waiting_buf[i].data);

    result = strcasestr(rspstr, gCmd_Handler.waiting_buf[i].data);
    free(rspstr);
    if (result) {
      printf("Waiting data recieved !! Resume\n");
      NXPLOG_EXTNS_E("Waiting data recieved !! Resume");
      memset(gCmd_Handler.waiting_buf, 0x00, sizeof(gCmd_Handler.waiting_buf));
      gCmd_Handler.bNeed_wait = FALSE;
      sem_post(&gCmd_Handler.wait_sem);
      break;
    }
    i++;
  }
}

/******************************************************************************
 * Function         parseTrigger_q
 *
 * Description      This function is used to store TRIGGER or STOP command in
 *                  structure to compare with response from previous command.
 *
 * Parameters       buf - input command
 *                  len - length of buf
 *                  trigger_buf - structure variable
 *
 * Returns          -1 If non-HEX coammnd passed as buf
 *                  0 on success
 *
 ******************************************************************************/
int parseTrigger_q(const char *buf, int len, trigger_q_t *trigger_buf) {
  const char *pos = buf;
  char entry_buf[LEN_FILE_NAME] = {0};
  int entry_offset = 0, entry_count = 0;
  int c = 0;

  memset(trigger_buf, 0x00, sizeof(trigger_q_t));

  if (pos[0] == '\0' || pos[0] == '|') {
    // hexstring contains no data
    // or wrong data
    return -1;
  }

  for (c = 0; c <= len; c++) {
    if (isDigit(*(pos + c), HEX_BASE) && c < len) {
      entry_buf[entry_offset] = *(pos + c);
      entry_offset++;
    } else if (*(pos + c) == '|' || *(pos + c) == '\n' || *(pos + c) == '\r' ||
               c == len) {
      memcpy(trigger_buf[entry_count].data, entry_buf, entry_offset);
      trigger_buf[entry_count].data[entry_offset] = '\0';
      trigger_buf[entry_count].len = entry_offset + 1;
      memset(entry_buf, 0x00, sizeof(entry_buf));
      entry_offset = 0;
      entry_count++;
    }
  }

  return 0;
}

/******************************************************************************
 * Function         execute_line
 *
 * Description      This function is used to
 *                    1. Read command which passed as parameter to binary.
 *                    2. Get command type for each command.
 *                    3. send command over i2c.
 *
 * Returns          void.
 *
 ******************************************************************************/
void execute_line() {
  int i;
  char *str_command = malloc(LEN_STR_COMMAND);
  char *hex_command = malloc(LEN_HEX_COMMAND);
  int hex_command_len = 0;
  printf("\nMode : Line input mode\n\nInput command\n");
  if (gCmd_Handler.bHal_opened == FALSE) {
    if (phNxpNciHal_open(p_hal_cback, p_hal_data_callback) !=
        NFCSTATUS_SUCCESS) {
      printf("Error in phNxpNciHal_open\n");
      exit(EXIT_FAILURE);
    }
  }

  gCmd_Handler.bHal_opened = TRUE;

  do {
    int nonDigit = 0;
    fgets(str_command, LEN_STR_COMMAND, stdin);

    if (!strncmp(str_command, "exit", LEN_STR_CMD_EXIT)) {
      phNxpNciHal_shutdown();
      //                phNxpNciHal_ioctl(0);
      exit(EXIT_FAILURE);
    }
    if (!strncmp(str_command, "reset", LEN_STR_CMD_RESET)) {
      unsigned long reset_val =
          (unsigned long)(str_command[strlen(str_command) - 2] - '0');
      if ((reset_val == RESET_TYPE_NFC_OFF) ||
          (reset_val == RESET_TYPE_NFC_ON) ||
          (reset_val == RESET_TYPE_NFC_DL) ||
          (reset_val == RESET_TYPE_NFC_SN1XX_DL) ||
          (reset_val == RESET_TYPE_NFC_VEN) ||
          (reset_val == RESET_TYPE_NFC_SN1XX)) {
        phNxpNciHal_ioctl(reset_val);
      }
      memset(str_command, 0x00, LEN_STR_COMMAND);
      continue;

    } else {
      for (i = 0; i < (int)strlen(str_command) - 1; i++) {
        if (!isDigit(str_command[i], HEX_BASE)) {
          nonDigit = 1;
        }
      }

      if (strlen(str_command) < LEN_STR_CMD_INVALID) {
        printf("Invalid data\n");
        continue;
      }
      if (nonDigit == 1) {
        printf("Please input data in HEX\n");
        continue;
      }

      if ((strlen(str_command) - 1) % 2) {
        printf("Please input EVEN number\n");
        continue;
      }
      hex_command_len = (strlen(str_command) - 1) / 2;

      memset(hex_command, 0x00, LEN_HEX_COMMAND);
      str2hex(hex_command, str_command, strlen(str_command) - 1);
      if (phNxpNciHal_write(hex_command_len, (uint8_t *)hex_command) <= 0)
        printf("SEND failed !!\n");
    }
  } while (1);
  if (str_command != NULL) free(str_command);
  if (hex_command != NULL) free(hex_command);
}


bool  send_raw_cmd(const uint8_t *buff, uint16_t len) {

  if(isValidNCIPacket(buff,len) == false) {
      printf("invalid NCI packet\n");
      return false;
  }
  if (phNxpNciHal_write(len,buff) <= 0) {
    printf("SEND failed !!\n");
    return false;
  }
  return true;
}

/******************************************************************************
 * Function         execute_script
 *
 * Description      This function is used to
 *                    1. Read each line from input file in loop
 *                    2. Get event for each command.
 *                    3. send command over i2c or spi.
 *
 * Parameters       file_name - Input script file name
 *
 * Returns          void.
 *
 ******************************************************************************/
void execute_script(char *file_name) {
  printf("\nMode : Script mode\n\n");
  int8_t timestamp_buf[LEN_TIME_BUF];
  int interval_ms = 0;
  scr_cmd cmd_type;
  char *str_command = malloc(LEN_STR_COMMAND);
  char *hex_command = malloc(LEN_HEX_COMMAND);
  int hex_command_len = 0;
  do {
    memset(str_command, 0x00, LEN_STR_COMMAND);
    hex_command_len = readScrline(file_name, str_command, &cmd_type);
    if (hex_command_len == -1) {
      printf("Error in opening script\n");
      exit(EXIT_FAILURE);
    }

    if (hex_command_len > 0 || (cmd_type == END)) {
      memset(hex_command, 0x00, LEN_HEX_COMMAND);
      if (str2hex(hex_command, str_command, strlen(str_command)) == 0) {
        switch (cmd_type) {
          case SEND: {
            NXPLOG_EXTNS_W("SEND");
            if (interval_ms > 0) usleep(interval_ms * MICRO_TO_MILLI_SEC);
            {
              int l_buf_len;
              char l_buf[LEN_HEX_COMMAND];
              memset(str_command, 0x00, LEN_STR_COMMAND);
              do {
                l_buf_len = readScrline(file_name, str_command, &cmd_type);
                if (l_buf_len == -1) {
                  printf("Error in opening script\n");
                  exit(EXIT_FAILURE);
                }
              } while (cmd_type == NONE);

              if (cmd_type == TRIGGER) {
                memset(l_buf, 0x00, sizeof(l_buf));
                if (str2hex(l_buf, str_command, strlen(str_command)) == 0) {
                  if (parseTrigger_q(str_command, strlen(str_command),
                                     gCmd_Handler.waiting_buf) != -1) {
                    int i = 0;
                    while (gCmd_Handler.waiting_buf[i].len != 0) {
                      NXPLOG_EXTNS_E("Waiting index[%d]", i);
                      NXPLOG_EXTNS_E(" len : %d, data : %s\n",
                                     (gCmd_Handler.waiting_buf[i].len - 1) / 2,
                                     gCmd_Handler.waiting_buf[i].data);
                      printf("Waiting index[%d]", i);
                      printf(" len : %d, data : %s\n",
                             (gCmd_Handler.waiting_buf[i].len - 1) / 2,
                             gCmd_Handler.waiting_buf[i].data);
                      i++;
                    }
                    gCmd_Handler.bNeed_wait = TRUE;
                  }
                }
              } else
                pullBackScrline();
            }

            if (gCmd_Handler.bThreadRunning == FALSE) {
              if (phNxpNciHal_open(p_hal_cback, p_hal_data_callback) !=
                  NFCSTATUS_SUCCESS) {
                printf("Error in phNxpNciHal_open\n");
                exit(EXIT_FAILURE);
              }
              gCmd_Handler.bHal_opened = TRUE;
            }

            if (phNxpNciHal_write(hex_command_len, (uint8_t *)hex_command) <= 0)
              printf("%s SEND   : failed !!\n",
                     GKI_get_time_stamp(timestamp_buf, LEN_TIME_BUF));

            if (gCmd_Handler.bNeed_wait == TRUE) {
              sem_wait(&gCmd_Handler.wait_sem);
            }
          } break;

          case TRIGGER: {
            NXPLOG_EXTNS_W("TRIGGER");
            if (parseTrigger_q(str_command, strlen(str_command),
                               gCmd_Handler.waiting_buf) != -1) {
              int i = 0;
              while (gCmd_Handler.waiting_buf[i].len != 0) {
                NXPLOG_EXTNS_W("Waiting index[%d]", i);
                NXPLOG_EXTNS_W(" len : %d, data : %s",
                               (gCmd_Handler.waiting_buf[i].len - 1) / 2,
                               gCmd_Handler.waiting_buf[i].data);
                printf("Waiting index[%d]", i);
                printf(" len : %d, data : %s\n",
                       (gCmd_Handler.waiting_buf[i].len - 1) / 2,
                       gCmd_Handler.waiting_buf[i].data);
                i++;
              }
              sem_wait(&gCmd_Handler.wait_sem);
            }
          } break;
          case SLEEP: {
            unsigned long wait_ms;
            wait_ms = get_val(hex_command_len, hex_command);
            printf("SLEEP  :       %d ms\n", (int)wait_ms);
            NXPLOG_EXTNS_W("SLEEP %dms", (int)wait_ms);
            usleep(wait_ms * MICRO_TO_MILLI_SEC);
          } break;

          case INTERVAL: {
            interval_ms = get_val(hex_command_len, hex_command);
            NXPLOG_EXTNS_W("INTERVAL %dms", interval_ms);
            printf("INTERVAL  :    %d ms\n", interval_ms);
          } break;

          case LOOPS: {
            unsigned long loop_count;
            loop_count = get_val(hex_command_len, hex_command);
            if (loop_count > 0) {
              set_loop_count(loop_count);
            }
            NXPLOG_EXTNS_W("LOOP   :       Start , Loop count : %d\n",
                           (int)loop_count);
            printf("LOOP   :       Start , Loop count : %d\n", (int)loop_count);
          } break;

          case RESET: {
            unsigned long reset_val;
            printf("RESET\n");
            reset_val = get_val(hex_command_len, hex_command);
            if (reset_val == RESET_TYPE_NFC_OFF)
              printf("\nTurn off NFCC\n\n");
            else if (reset_val == RESET_TYPE_NFC_ON)
              printf("\nTurn on NFCC\n\n");
            else if (reset_val == RESET_TYPE_NFC_DL)
              printf("\nDOWNLOAD Mode\n\n");
            else if (reset_val == RESET_TYPE_NFC_SN1XX_DL)
              printf("\nDOWNLOAD Mode for SN100x\n\n");
            else if (reset_val == RESET_TYPE_NFC_VEN)
              printf("\nVEN reset !!\n\n");
            else if (reset_val == RESET_TYPE_NFC_SN1XX)
              printf("\nNormal Mode for SN100x\n\n");

            NXPLOG_EXTNS_W("RESET %d", (int)reset_val);

            if ((reset_val == RESET_TYPE_NFC_OFF) ||
                (reset_val == RESET_TYPE_NFC_ON) ||
                (reset_val == RESET_TYPE_NFC_DL) ||
                (reset_val == RESET_TYPE_NFC_SN1XX_DL) ||
                (reset_val == RESET_TYPE_NFC_VEN) ||
                (reset_val == RESET_TYPE_NFC_SN1XX)) {
              if (reset_val == RESET_TYPE_NFC_DL ||
                  reset_val == RESET_TYPE_NFC_SN1XX_DL)
                phNxpNciHal_set_dnld_flag(1);
              else
                phNxpNciHal_set_dnld_flag(0);

              if (gCmd_Handler.bHal_opened == FALSE) {
                if (phNxpNciHal_open(p_hal_cback, p_hal_data_callback) !=
                    NFCSTATUS_SUCCESS) {
                  printf("Error in phNxpNciHal_open\n");
                  exit(EXIT_FAILURE);
                }
                gCmd_Handler.bHal_opened = TRUE;
              }
              phNxpNciHal_ioctl(reset_val);
            }
          } break;

          case RESETSPI: {
            unsigned long reset_val;
            reset_val = get_val(hex_command_len, hex_command);
            phLibNfc_Message_t pMsg;

            NXPLOG_EXTNS_W("RESETSPI %d", (int)reset_val);

            if (reset_val == RESET_TYPE_SPI_ON) {
              pMsg.eMsgType = SPI_OPEN;
            } else if (reset_val == RESET_TYPE_SPI_OFF) {
              pMsg.eMsgType = SPI_CLOSE;
            }

            (void)phDal4Nfc_msgsnd(gCmd_Handler.nClientId, &pMsg, 0);
          } break;

          case PWRREQ: {
            unsigned long reset_val;
            reset_val = get_val(hex_command_len, hex_command);
            if (reset_val == 0)
              printf("\nTurn off PWR-REQ\n\n");
            else if (reset_val == 1)
              printf("\nTurn on PWR-REQ\n\n");

            phLibNfc_Message_t pMsg;

            if (reset_val == RESET_TYPE_SPI_ON) {
              pMsg.eMsgType = SPI_PWR_ON;
            } else if (reset_val == RESET_TYPE_SPI_OFF) {
              pMsg.eMsgType = SPI_PWR_OFF;
            }

            (void)phDal4Nfc_msgsnd(gCmd_Handler.nClientId, &pMsg, 0);
          } break;

          case SENDSPI: {
            NXPLOG_EXTNS_W("SENDSPI");
            if (interval_ms > 0) usleep(interval_ms * MICRO_TO_MILLI_SEC);

            phLibNfc_Message_t pMsg;

            pMsg.eMsgType = SPI_TRANSCIEVE;
            pMsg.Size = hex_command_len;
            pMsg.pMsgData = malloc(hex_command_len);
            memcpy((uint8_t *)pMsg.pMsgData, hex_command, hex_command_len);

            (void)phDal4Nfc_msgsnd(gCmd_Handler.nClientId, &pMsg, 0);

          } break;

          case END: {
            NXPLOG_EXTNS_W("LOOP   :       End");
            printf("LOOP   :       End\n");
          } break;

          case STOP: {
            printf("STOP\n");
            NXPLOG_EXTNS_W("STOP");
            if (parseTrigger_q(str_command, strlen(str_command),
                               gCmd_Handler.stopping_buf) != -1) {
              int i = 0;
              while (gCmd_Handler.stopping_buf[i].len != 0) {
                NXPLOG_EXTNS_W("Stopping index[%d]", i);
                NXPLOG_EXTNS_W(" len : %d, data : %s\n",
                               (gCmd_Handler.stopping_buf[i].len - 1) / 2,
                               gCmd_Handler.stopping_buf[i].data);
                printf("Stopping index[%d]", i);
                printf(" len : %d, data : %s\n",
                       (gCmd_Handler.stopping_buf[i].len - 1) / 2,
                       gCmd_Handler.stopping_buf[i].data);
                i++;
              }
            }
          } break;

          default: {
            printf("Wrong command\n");
            continue;
          } break;
        }
      }
    }
  } while (cmd_type != EOS);
  if (str_command != NULL) free(str_command);
  if (hex_command != NULL) free(hex_command);
}
