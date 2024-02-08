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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <phNxpEse_Api.h>
#include <phNxpNciHal_Adaptation.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <phNxpNciHal_utils.h>
#include <scr_common.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>

#define ARG_ERR                                                            \
  "usage: \n  -t <type of test> \n        0 : Line input\n        1 : "    \
  "execute script\n        2 : execute script + line input\n  -d <kernel " \
  "driver name>\n  -f <script file full path and name>\n"
#define DEVICE_PN553 "pn553"
#define DEVICE_NXPNCI "nxp-nci"
#define DEVICE_P73 "/dev/p73"
#define LEN_DRIVER_NAME 16
#define LEN_FILE_NAME 256
#define LEN_STR_COMMAND 1024
#define LEN_HEX_COMMAND 512
#define LEN_BUFFER 16
#define HEX_BASE 16
#define MICRO_TO_MILLI_SEC 1000
#define LEN_TIME_BUF 32
#define CORE_SET_CONFIG      0x0001
#define CORE_GET_CONFIG      0x0002
#define NCI_HEADER_LEN       0x03
#define NCI_LEN_FIELD        (NCI_HEADER_LEN - 0x01)
#define MAX_RETRY            0x01
#define CORE_SET_CONFIG_RSP  0x4002
#define CORE_GET_CONFIG_RSP  0x4003
#define NCI_NTF              0x06
#define HEADER_LEN           0x06

/* Class */
#define CLS_VERSION               0x0E
#define CLS_TRANSRECEIVE          0x01
#define CLS_GPIO                  0x10

/* Instruction */
#define CMD_TRANSMIT              0x05
#define CMD_RECEIVE               0x0E
#define CMD_GET_V                 0x0E

#define CMD_UC_MAJOR              0x11
#define CMD_UC_MINOR              0x12
#define CMD_UC_DEV                0x13
#define CMD_UC_STRING             0x14
#define CMD_UC_DATE_TIME          0x15
#define CMD_BOARD                 0x30
#define CMD_READER_IC_MAJOR       0x31
#define CMD_READER_IC_MINOR       0x32
#define CMD_READER_IC_HW_VER      0x35
#define CMD_READER_IC_MODEL_ID    0x36
#define CMD_READER_IC_ROM_VER     0x37

/* Param 1 */
#define P1_GPIO_IRQ               0x02


typedef enum { SOCKET_MODE = 0, LINE_MODE, SCRIPT_MODE, HYBRID_MODE} scrModes;

typedef enum {
  LEN_STR_CMD_INVALID = 3,
  LEN_STR_CMD_EXIT,
  LEN_STR_CMD_RESET
} scr_LenStrCmd;

typedef enum {
  RESET_TYPE_NFC_OFF = 0,
  RESET_TYPE_NFC_ON,
  RESET_TYPE_NFC_DL,
  RESET_TYPE_RFU,
  RESET_TYPE_NFC_SN1XX_DL,
  RESET_TYPE_NFC_VEN,
  RESET_TYPE_NFC_SN1XX
} scrNfc_Reset_val;

typedef enum { RESET_TYPE_SPI_OFF = 0, RESET_TYPE_SPI_ON } scrSPI_Reset_val;

typedef struct trigger_q {
  char data[255];
  int len;
} trigger_q_t;

typedef struct halCmd_Handler {
  bool_t bHal_opened;
  trigger_q_t waiting_buf[LEN_BUFFER];
  trigger_q_t stopping_buf[LEN_BUFFER];
  sem_t wait_sem;
  bool_t bNeed_wait;
  uintptr_t nClientId;
  uint8_t bThreadRunning; /*Flag to decide whether to run or abort the thread */
} HalCmd_Handler_t;

extern int8_t *GKI_get_time_stamp(int8_t *tbuf, int len);
bool_t isDigit(char c, int base);
void revert_order(unsigned char *result, unsigned char *input, int len);
int str2hex(char *data, const char *hexstring, unsigned int len);
unsigned long convert_to_decimal(unsigned char *input, int len);
void ToHexStr(const uint8_t *data, uint16_t len, char *hexString,
              uint16_t hexStringSize);
void execute_line();
void execute_script(char *file_name);
void p_hal_cback(nfc_event_t event, nfc_status_t event_status);
void p_hal_data_callback(uint16_t data_len, uint8_t *p_data);
bool_t createSPIThread();
void releaseSPIThread();
unsigned long get_val(int buf_len, char *buf);
void proc_server(uint32_t Port_number);
bool send_raw_cmd(const uint8_t *buff, uint16_t len);
void send_resp(uint16_t data_len, uint8_t *p_data);
bool isValidNCIPacket(const uint8_t* packet, uint16_t packetLength);
#endif