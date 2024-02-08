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

/* Name        :   cockpit_Client_sample.cpp
 * Discription :   This is sample code to connect with NFC MW Server and
 *                 Perform the basic testing. Client can send the valid NCI packet
 *                 and wait for response from the NFCC.
 */

#include <stdio.h>
#include <winsock.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#define PORT 8050
#define NCI_HEADER_LEN 0x03
#define HEADER_LEN 0x06
#define MAX_PKT_SIZE 1024

bool str2Hex(char* inputString, uint8_t* hexArr, uint16_t hexArrLen, uint16_t * len);
bool isValidHexChar(char c);
void print_pkt(char *tag, char ch,uint8_t *arr, uint16_t len );

const uint8_t appMajVer[] = {0x0E, 0x11, 0x00, 0x00, 0x00, 0x00};
const uint8_t appMinVer[] = {0x0E, 0x12, 0x00, 0x00, 0x00, 0x00};
const uint8_t appDevVer[] = {0x0E, 0x13, 0x00, 0x00, 0x00, 0x00};
const uint8_t appVerStr[] = {0x0E, 0x14, 0x00, 0x00, 0x00, 0x00};
const uint8_t appBuildDateTime[] = {0x0E, 0x15, 0x00, 0x00, 0x00, 0x00};
const uint8_t boardNameStr[] = {0x0E, 0x30, 0x00, 0x00, 0x00, 0x00};
const uint8_t fwMajVer[] = {0x0E, 0x31, 0x00, 0x00, 0x00, 0x00};
const uint8_t fwMinVer[] = {0x0E, 0x32, 0x00, 0x00, 0x00, 0x00};
const uint8_t fwHwVer[] = {0x0E, 0x35, 0x00, 0x00, 0x00, 0x00};
const uint8_t fwModelVer[] = {0x0E, 0x36, 0x00, 0x00, 0x00, 0x00};
const uint8_t fwRomVer[] = {0x0E, 0x37, 0x00, 0x00, 0x00, 0x00};
const uint8_t tansmit[] = {0x01, 0x05, 0x00, 0x00, 0x06, 0x00,
                           0x20, 0x03, 0x03, 0x01, 0xA2, 0x00};
const uint8_t irqValue[] = {0x10, 0x0E, 0x02, 0x00, 0x00, 0x00};
const uint8_t receive_heard[] = {0x01, 0x0E, 0x03, 0x00, 0x00, 0x00};
uint8_t receive_payload[] = {0x01, 0x0E, 0x0D, 0x00, 0x00, 0x00};

int main()
{
  // Initiate the Socket environment
  WSADATA w;
  sockaddr_in srv;
  int nRet = 0;
  char sBuff[MAX_PKT_SIZE] = {0};
  uint8_t rBuff[MAX_PKT_SIZE] = {0};
  uint8_t sHexBuff[MAX_PKT_SIZE] = {0};
  uint16_t rsplen = 0;
  uint8_t pyloadlen = 0;
  uint16_t cmdLen = 0;
  int16_t readByte = 0;
  uint8_t option = 0x00;

  nRet = WSAStartup(MAKEWORD(2, 2), &w);
  if (nRet < 0) {
    printf("Cannot Initialize socket lib\n");
    return -1;
  }

  // Open a socket - listener
  SOCKET nSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (nSocket < 0) {
    // errno is a system global variable which gets updated
    // with the last API call return value/result.
    printf("Cannot Initialize listener socket:%d\n", errno);
    ;
    return -1;
  }

reconnect :
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = inet_addr("127.0.0.1");
  srv.sin_port = htons(PORT);
  memset(&(srv.sin_zero), 0, 8);
  printf("Check port number:%d\n", srv.sin_port);
  ;
  nRet = connect(nSocket, (struct sockaddr *)&srv, sizeof(srv));
  if (nRet < 0) {
    printf("Server is not available :%d\n", nRet);
    return -1;
  } else {
    printf("Server Connection Successful\n");
  }

  while (1) {
    /* Reset the buffers & legth variable */
    cmdLen = 0;
    rsplen = 0;
    option = 0x00;
    memset(sBuff, 0, sizeof(sBuff));
    memset(rBuff, 0, sizeof(rBuff));
    memset(sHexBuff, 0, sizeof(sBuff));

    printf("Select the option : \n");
    printf("1. Major version of Socket Application\n");
    printf("2. Minor version of Socket Application\n");
    printf("3. Dev version of Socket Application\n");
    printf("4. Version string of Socket Application\n");
    printf("5. Socket app compile time & Date\n");
    printf("6. Board name \n");
    printf("7. Major version of Pallas IC \n");
    printf("8. Minor version of Pallas IC \n");
    printf("9. Hardware version of Pallas IC \n");
    printf("10 Model ID of Pallas IC \n");
    printf("11 ROM version of Pallas IC \n");
    printf("12 Send Sample NCI pkt to NFCC \n");
    printf("13 Get the IRQ status \n");
    printf("14 Read Sample NCI pkt response header from server \n");
    printf("15 Read Sample NCI pkt response payload from server \n");
    printf("16 Enter command direct \n");
    scanf("%d", &option);

    switch (option) {
    case 1: {
      memcpy(sHexBuff, &appMajVer[0], sizeof(appMajVer) / sizeof(appMajVer[0]));
      cmdLen = sizeof(appMajVer) / sizeof(appMajVer[0]);
    } break;
    case 2: {
      memcpy(sHexBuff, &appMinVer[0], sizeof(appMinVer) / sizeof(appMinVer[0]));
      cmdLen = sizeof(appMinVer) / sizeof(appMinVer[0]);
    } break;
    case 3: {
      memcpy(sHexBuff, &appDevVer[0], sizeof(appDevVer) / sizeof(appDevVer[0]));
      cmdLen = sizeof(appDevVer) / sizeof(appDevVer[0]);
    } break;
    case 4: {
      memcpy(sHexBuff, &appVerStr[0], sizeof(appVerStr) / sizeof(appVerStr[0]));
      cmdLen = sizeof(appVerStr) / sizeof(appVerStr[0]);
    } break;
    case 5: {
      memcpy(sHexBuff, &appBuildDateTime[0],
             sizeof(appBuildDateTime) / sizeof(appBuildDateTime[0]));
      cmdLen = sizeof(appBuildDateTime) / sizeof(appBuildDateTime[0]);
    } break;
    case 6: {
      memcpy(sHexBuff, &boardNameStr[0],
             sizeof(boardNameStr) / sizeof(boardNameStr[0]));
      cmdLen = sizeof(boardNameStr) / sizeof(boardNameStr[0]);
    } break;
    case 7: {
      memcpy(sHexBuff, &fwMajVer[0], sizeof(fwMajVer) / sizeof(fwMajVer[0]));
      cmdLen = sizeof(fwMajVer) / sizeof(fwMajVer[0]);
    } break;
    case 8: {
      memcpy(sHexBuff, &fwMinVer[0], sizeof(fwMinVer) / sizeof(fwMinVer[0]));
      cmdLen = sizeof(fwMinVer) / sizeof(fwMinVer[0]);
    } break;
    case 9: {
      memcpy(sHexBuff, &fwHwVer[0], sizeof(fwHwVer) / sizeof(fwHwVer[0]));
      cmdLen = sizeof(fwHwVer) / sizeof(fwHwVer[0]);
    } break;
    case 10: {
      memcpy(sHexBuff, &fwModelVer[0],
             sizeof(fwModelVer) / sizeof(fwModelVer[0]));
      cmdLen = sizeof(fwModelVer) / sizeof(fwModelVer[0]);
    } break;
    case 11: {
      memcpy(sHexBuff, &fwRomVer[0], sizeof(fwRomVer) / sizeof(fwRomVer[0]));
      cmdLen = sizeof(fwRomVer) / sizeof(fwRomVer[0]);
    } break;
    case 12: {
      memcpy(sHexBuff, &tansmit[0], sizeof(tansmit) / sizeof(tansmit[0]));
      cmdLen = sizeof(tansmit) / sizeof(tansmit[0]);
    } break;
    case 13: {
      memcpy(sHexBuff, &irqValue[0], sizeof(irqValue) / sizeof(irqValue[0]));
      cmdLen = sizeof(irqValue) / sizeof(irqValue[0]);
    } break;
    case 14: {
      memcpy(sHexBuff, &receive_heard[0],
             sizeof(receive_heard) / sizeof(receive_heard[0]));
      cmdLen = sizeof(receive_heard) / sizeof(receive_heard[0]);
    } break;
    case 15: {
      receive_payload[2] = pyloadlen;
      memcpy(sHexBuff, &receive_payload[0],
             sizeof(receive_payload) / sizeof(receive_payload[0]));
      cmdLen = sizeof(receive_payload) / sizeof(receive_payload[0]);
    } break;
    case 16: {
      fflush(stdin);
      printf("Enter Valid TVL packet to send to NFC MW Server : ");
      fgets(sBuff, 1024, stdin);

      /* Conver the given input into HEX bytes */
      if (str2Hex(sBuff, sHexBuff, sizeof(sHexBuff), &cmdLen) == false) {
        printf("Invalid data formate, Please provide HEX character only. \n");
        continue;
      }
    } break;
    default: {
      printf("Invalid option. retry ....\n");
      continue;
    } break;
    }

    /* Print the send packet */
    print_pkt((char *)"SND", '>', sHexBuff, cmdLen);

    /* Send the TLV to NFC MW Server */
    send(nSocket, (char *)sHexBuff, cmdLen, 0);

  read_rsp:
    /* Read the TYPE & LENGTH to extract the payload length */
    readByte = recv(nSocket, (char *)rBuff, HEADER_LEN, 0);
    if (readByte == -1) {
      printf("Server Disconnected...\n");
      goto reconnect;
    }

    /* Extract the payload length from Header byte */
    rsplen = (((rBuff[5] << 8) & 0xFF00) | ((rBuff[4]) & 0x00FF));

    /* Read the Payload */
    if (rsplen > 0) {
      readByte = recv(nSocket, (char *)&rBuff[HEADER_LEN], rsplen, 0);
      if (readByte == -1) {
        printf("Server Disconnected...\n");
        goto reconnect;
      }
    }

    if(rsplen == 0x03) {
        pyloadlen = rBuff[8]; /* Get the NCI payload length */
    }
    /*Print the received NCI Response */
    print_pkt((char *)"RCV", '<', rBuff, (rsplen + HEADER_LEN));

    /* If notification read the response */
    if (rBuff[1] == 0xFF)
      goto read_rsp;

    printf("\n");
    sleep(1);
  }
}

void print_pkt(char *tag, char ch,uint8_t *arr, uint16_t len ) {
  printf("%s : len =  %d %c ", tag, len, ch);
  for (int i = 0; i < len; i++) {
    printf("%02X", (arr[i] & 0xFF));
  }
  printf("\n");
}

bool str2Hex(char* inputString, uint8_t* hexArr, uint16_t hexArrLen, uint16_t * len) {
  uint16_t inputLength = strlen(inputString) - 1;
  uint16_t i, j;
  *len = 0;

  if ((inputLength & 1) != 0x00) {
    printf("No of char of string must be even no.\n");
    return false;
  }

  // Ensure the output byte array is large enough to hold the converted bytes
  if (hexArrLen < (inputLength / 2)) {
    return false;
  }

  // Convert each pair of characters in the input string into a hex byte
  for (i = 0, j = 0; i < inputLength; i += 2, j++) {
    char highNibble = inputString[i];
    char lowNibble = inputString[i + 1];

    // Validate the characters as valid hex digits
    if (!isValidHexChar(highNibble) || !isValidHexChar(lowNibble)) {
      // Handle invalid character error
      printf("Invalid character\n");
      return false;
    }

    // Convert the high and low nibbles to their corresponding hex values
    uint8_t highByte =
        isdigit(highNibble) ? highNibble - '0' : toupper(highNibble) - 'A' + 10;
    uint8_t lowByte =
        isdigit(lowNibble) ? lowNibble - '0' : toupper(lowNibble) - 'A' + 10;

    // Combine the high and low bytes into a single hex byte
    hexArr[j] = (highByte << 4) | lowByte;

    // Count complete packet length
    (*len)++;
  }
  return true;
}

bool isValidHexChar(char c) {
  // Check if the character is a valid hexadecimal digit ('0' to '9' or 'A' to
  // 'F' or 'a' to 'f')
  return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
          (c >= 'a' && c <= 'f'));
}
