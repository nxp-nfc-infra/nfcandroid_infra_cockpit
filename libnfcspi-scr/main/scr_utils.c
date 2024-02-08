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

#include <math.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <scr_common.h>
#include <stdio.h>

/******************************************************************************
 * Function         get_val
 *
 * Description      This function give decimal value based on input size for
 *                  events like STOP,TRIGGER,STOP,RESET,RESETSPI etc.
 *
 * Parameters       buf_len - length of buf
 *                  buf - pointer to input buf
 *
 * Returns          val - int value for event.
 *
 ******************************************************************************/
unsigned long get_val(int buf_len, char *buf) {
  unsigned long val;
  unsigned long temp_val;
  switch (buf_len) {
    case sizeof(unsigned long):
      revert_order((unsigned char *)&temp_val, (unsigned char *)buf, buf_len);
      val = convert_to_decimal((unsigned char *)&temp_val, buf_len);
      break;
    case sizeof(unsigned short):
      revert_order((unsigned char *)&temp_val, (unsigned char *)buf, buf_len);
      val = convert_to_decimal((unsigned char *)&temp_val, buf_len);
      break;
    case sizeof(unsigned char):
      val = convert_to_decimal((unsigned char *)buf, buf_len);
      break;
    case 0:
      val = 0;
      break;
    default:
      if (buf_len > (int)sizeof(unsigned long)) buf_len = sizeof(unsigned long);
      revert_order((unsigned char *)&temp_val, (unsigned char *)buf, buf_len);
      val = convert_to_decimal((unsigned char *)&temp_val, buf_len);
      break;
  }
  return val;
}

/*******************************************************************************
**
** Function:    isDigit()
**
** Description: determine if 'c' is numeral digit
**
** Returns:     true, if numerical digit
**
*******************************************************************************/
bool_t isDigit(char c, int base) {
  if ('0' <= c && c <= '9') return TRUE;
  if (base == 16) {
    if (('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')) return TRUE;
  }
  return FALSE;
}

/******************************************************************************
 * Function         revert_order
 *
 * Description      This function reverse the input command.
 *
 * Parameters       reseult - pointer to store reversed command.
 *                  input - pointer to command.
 *                  len - length of input
 *
 * Returns          void
 *
 ******************************************************************************/
void revert_order(unsigned char *result, unsigned char *input, int len) {
  int i;
  for (i = 0; i < len; i++) {
    result[i] = input[len - 1 - i];
  }
}

/******************************************************************************
 * Function         str2hex
 *
 * Description      This function convert string command to hex.
 *
 * Parameters       data - pointer to hex converted command.
 *                  hexstring - pointer to string command.
 *                  len - length of hexstring
 *
 * Returns          0 on success
 *                  -1 if hexstring is empty
 *
 ******************************************************************************/
int str2hex(char *data, const char *hexstring, unsigned int len) {
  char pos[512];
  char *endptr;
  size_t count = 0;

  if ((hexstring[0] == '\0')) {
    // hexstring contains no data
    // or hexstring has an odd length
    return -1;
  }

  if (len % 2) {
    *pos = '0';
    memcpy(pos + 1, hexstring, len);
    len++;
  } else
    memcpy(pos, hexstring, len);

  for (count = 0; count < len; count++) {
    char buf[5] = {'0', 'x', *(pos + 2 * count), *(pos + 2 * count + 1), 0};
    data[count] = strtol(buf, &endptr, 0);
  }
  return 0;
}

/******************************************************************************
 * Function         convert_to_decimal
 *
 * Description      This function convert hex to decimal value.
 *
 * Parameters       input - pointer to hex value.
 *                  len - length of input.
 *
 * Returns          decimal value.
 *
 ******************************************************************************/
unsigned long convert_to_decimal(unsigned char *input, int len) {
  int i;
  unsigned long value = 0;
  for (i = 0; i < len; i++) {
    value += ((((input[i] >> 4) & 0x0F) * 10) + (input[i] & 0x0F)) *
             pow(10, (i * 2));
  }
  return value;
}

/******************************************************************************
 * Function         ToHexStr
 *
 * Description      This function convert array data to hex string.
 *
 * Parameters       data - pointer to array data.
 *                  len - length of array data.
 *                  hexString - pointer to store hex string.
 *                  hexStringSize - size of hexstring.
 *
 * Returns          void
 *
 ******************************************************************************/
void ToHexStr(const uint8_t *data, uint16_t len, char *hexString,
              uint16_t hexStringSize) {
  int i = 0, j = 0;
  const char *sTable = "0123456789abcdef";
  for (i = 0, j = 0; i < len && j < hexStringSize - 3; i++) {
    hexString[j++] = sTable[(*data >> 4) & 0xf];
    hexString[j++] = sTable[*data & 0xf];
    data++;
  }
  hexString[j] = '\0';
}
