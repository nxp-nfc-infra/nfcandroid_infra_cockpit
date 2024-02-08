/*
 * Copyright 2023-2024 NXP.
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only be
 * used strictly in accordance with the applicable license terms. By expressly
 * accepting such terms or by downloading, installing, activating and/or
 * otherwise using the software, you are agreeing that you have read, and that
 * you agree to comply with and are bound by, such license terms. If you do not
 * agree to be bound by the applicable license terms, then you may not
 * retain,install, activate or otherwise use the software.
 */

#include<stdio.h>
#include <unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<string.h>
#include<netdb.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<strings.h>
#include <stdlib.h>
#include <scr_common.h>
#include <phNxpLog.h>
#include <semaphore.h>


#define MAX_NCI_PKT 1024
#define MAX_SEVER_RSP_SIZE 1024
#define SA struct sockaddr

#define NCI_MT_TYPE_CMD 0x02
#define NCI_MT_TYPE_RSP 0x04
#define NCI_MT_TYPE_NTF 0x06

#define NCI_MT_TYPE_MASK 0x07

#define PH_SUCCESS                      0x0000 /**< Returned in case of no error. */
#define PH_ERR_READ_WRITE_ERROR         0x0008 /**< A Read or Write error occurred in RAM/ROM or Flash. */
#define PH_ERR_INVALID_PARAMETER        0x0021 /**< Invalid parameter supplied. */

// Define a binary semaphore
sem_t semaphore;
int newsockfd;
extern HalCmd_Handler_t gCmd_Handler;

char *serverVersion   = "NNC_uC_ADBSocket_04.00.00\0";
char *compileDateTime = __DATE__ " " __TIME__"\0";
char *boardName       = "General_RB3\0";

const uint8_t core_reset_cmd[] = {0x20, 0x00, 0x01, 0x00};
const uint8_t core_init_cmd[]  = {0x20, 0x01, 0x02, 0x00, 0x00};
const uint8_t core_set_pwr_cmd[] = {0x2F, 0x00, 0x01, 0x00}; // 0x00 : Standby disable
bool isInitDone = false;

uint8_t ReaderIC_ModelID = 0xFF;
uint8_t ReaderIC_HwVer   = 0xFF;
uint8_t ReaderIC_RomVer  = 0xFF;
uint8_t ReaderIC_Major   = 0xFF;
uint8_t ReaderIC_Minor   = 0xFF;

uint8_t rspBuff[MAX_NCI_PKT];
uint8_t * pUpdatedIndex = NULL;
uint16_t  remDataLen = 0x00;

extern bool ifDebug;

void send_resp(uint16_t data_len, uint8_t *p_data) {
    uint8_t mtType = (p_data[0] >> 4) & NCI_MT_TYPE_MASK;

    if((p_data[0] == 0x60) && (p_data[1] == 0x00) && (data_len == 13)){
        ReaderIC_ModelID = p_data[8];
        ReaderIC_HwVer   = p_data[9];
        ReaderIC_RomVer  = p_data[10];
        ReaderIC_Major   = p_data[11];
        ReaderIC_Minor   = p_data[12];
    }

    sem_post(&semaphore);

    if(isInitDone && (mtType == NCI_MT_TYPE_RSP)){
       bzero(rspBuff , sizeof(rspBuff));
       memcpy(rspBuff, p_data, data_len);
       pUpdatedIndex = rspBuff;
       remDataLen = data_len;
    }
    return;
}

/******************************************************************************
 * Function         short_init
 *
 * Description      Perform short init
 *
 * Parameters       void
 *
 * Returns          true for success else false.
 *
 ******************************************************************************/
bool short_init(void)
{
    struct timespec rspTs;
    struct timespec ntfTs;
    sem_init(&semaphore, 0, 0);

    gCmd_Handler.bHal_opened = TRUE;

    if(send_raw_cmd(core_reset_cmd,sizeof(core_reset_cmd)) == false) {
    ifDebug ? printf("core_reset_cmd Failed\n") : 0;
    return false;
    }

    // Set a timeout of 2 seconds
    clock_gettime(CLOCK_REALTIME, &rspTs);
    rspTs.tv_sec += 2;
    // Wait for the semaphore with a timeout
    int result = sem_timedwait(&semaphore, &rspTs);
    if (result != 0x00) {
        ifDebug ? printf("Timeout occurred. No resp for core_reset_cmd\n") : 0;
        return false;
    }

    // Set a timeout of 2 seconds
    clock_gettime(CLOCK_REALTIME, &ntfTs);
    ntfTs.tv_sec += 2;
    // Wait for the semaphore with a timeout
    result = sem_timedwait(&semaphore, &ntfTs);
    if (result != 0x00) {
        ifDebug ? printf("Timeout occurred. No ntf for core_reset_cmd\n") : 0;
        return false;
    }

    if(send_raw_cmd(core_init_cmd,sizeof(core_init_cmd)) == false) {
        ifDebug ? printf("core_init_cmd Failed\n") : 0;
        return false;
    }

    // Set a timeout of 2 seconds
    clock_gettime(CLOCK_REALTIME, &rspTs);
    rspTs.tv_sec += 2;
    // Wait for the semaphore with a timeout
    result = sem_timedwait(&semaphore, &rspTs);
    if (result != 0x00) {
        ifDebug ? printf("Timeout occurred. No resp for core_init_cmd\n") : 0;
        return false;
    }

    if(send_raw_cmd(core_set_pwr_cmd,sizeof(core_set_pwr_cmd)) == false) {
        ifDebug ? printf("core_set_pwr_cmd Failed\n") : 0;
        return false;
    }

    // Set a timeout of 2 seconds
    clock_gettime(CLOCK_REALTIME, &rspTs);
    rspTs.tv_sec += 2;
    // Wait for the semaphore with a timeout
    result = sem_timedwait(&semaphore, &rspTs);
    if (result != 0x00) {
        ifDebug ? printf("Timeout occurred. No resp for core_set_pwr_cmd\n") : 0;
        return false;
    }
    isInitDone = true;
    return true;
}

/******************************************************************************
 * Function         proc_server
 *
 * Description      Run the TCP server and process the client request.
 *
 * Parameters       Client Port number
 *
 * Returns          void.
 *
 ******************************************************************************/
void proc_server(uint32_t client_port_number)
{
    int sockfd;
    socklen_t  len;
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buff[MAX_NCI_PKT];
    uint16_t payloadLen = 0;
    uint8_t *pBuff = NULL;
    uint8_t retry = 0;
    uint8_t byteRead = 0;
    uint8_t class = 0;
    uint8_t cmd = 0;
    uint8_t p1 = 0x00;
    uint8_t p2 = 0x00;
    uint16_t stringLen = 0;
    uint8_t verRsp[MAX_SEVER_RSP_SIZE]={0};
    uint8_t ucMajor = serverVersion[18] - '0';
    uint8_t ucMinor = serverVersion[21] - '0';
    uint8_t ucDev   = serverVersion[24] - '0';

    if (gCmd_Handler.bHal_opened == FALSE) {
        if (phNxpNciHal_open(p_hal_cback, p_hal_data_callback) != NFCSTATUS_SUCCESS) {
            ifDebug ? printf("Error in phNxpNciHal_open\n") : 0;
            return;
        }
    }

    if(short_init() == false) {
        ifDebug ? printf("Init failed\n") : 0;
        isInitDone = false;
        return;
    }

rebind:
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        ifDebug ? printf("socket creation failed...\n") : 0;
    }
    else
        printf("Socket successfully created..\n");
        printf("Listening at port number %d\n",client_port_number);
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    servaddr.sin_port = htons(client_port_number);

    // Binding newly created socket to given IP
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        ifDebug ? printf("socket bind failed. Retrying\n") : 0;
        if(retry < MAX_RETRY)
        {
            close(sockfd);
            retry++;
            goto rebind;
        }
    }
    else
        printf("Socket successfully binded \n");

waitForClient :
    // Now server is ready to listen
    if ((listen(sockfd, 5)) != 0) {
        ifDebug ? printf("Listen failed...\n") : 0;
        return;
    }
    else
        printf("Server listening..\n");
    len = sizeof(cliaddr);

    // Accept the data packet from client
    newsockfd = accept(sockfd, (SA*)&cliaddr, &len);
    if (newsockfd < 0) {
        ifDebug ? printf("server accept failed...\n") : 0;
        return;
    }  else {
        printf("Server accepted client request !!!\n");
    }

    while(true)
    {
        bzero(verRsp, MAX_SEVER_RSP_SIZE);
        bzero(buff , MAX_NCI_PKT);
        stringLen = 0;
        byteRead = read(newsockfd, buff, HEADER_LEN);
        if(byteRead == 0x00) {
            printf("Client Disconnected...\n");
            goto waitForClient;
        }

        payloadLen = ((buff[0x05] << 8) & 0xFF00) | (buff[0x04]);

        if(payloadLen > 0) {
            byteRead = read(newsockfd, &buff[HEADER_LEN], payloadLen);
            if(byteRead == 0x00) {
                printf("Client Disconnected...\n");
                goto waitForClient;
            }

            pBuff = (uint8_t *)malloc(payloadLen);
            if(pBuff == NULL) {
                ifDebug ? printf("Payload memory allocation failed\n") : 0;
                continue;
            }
            memcpy(pBuff, &buff[HEADER_LEN], payloadLen);
        }

        class = buff[0];
        cmd = buff[1];
        p1 = buff[2];
        p2 = buff[3];
        ifDebug ? printf("Class [0x%X] Command [0x%X] reaceived parm1 [0x%X] parm2 [0x%X]\n",class,cmd,p1,p2) : 0;

          switch(class) {
            case CLS_VERSION : {
              switch (cmd) {
                case CMD_UC_MAJOR : {
                    ifDebug ? printf("App Major Verion : 0x%X\n",ucMajor) : 0;
                    memcpy(verRsp, &buff[0], 0x04);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ucMajor;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_UC_MINOR : {
                    ifDebug ? printf("App Minor Verion : 0x%X\n",ucMinor) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ucMinor;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_UC_DEV : {
                    ifDebug ? printf("App Dev Verion : 0x%X\n",ucDev) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ucDev;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_UC_STRING : {
                    ifDebug ? printf("App Verion : %s\n",serverVersion) : 0;
                    stringLen = strlen(serverVersion) + 1; // for NULL terminator
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = stringLen & 0x00FF;
                    verRsp[5] = (stringLen >> 8) & 0x00FF;
                    memcpy(&verRsp[6], serverVersion, HEADER_LEN + verRsp[4]);
                    write(newsockfd, verRsp, HEADER_LEN + verRsp[4]);
                }
                break;
                case CMD_UC_DATE_TIME : {
                    ifDebug ? printf("Build Date & Time : %s\n", compileDateTime) : 0;
                    stringLen = strlen(compileDateTime) + 1; // for NULL terminator
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = stringLen & 0x00FF;
                    verRsp[5] = (stringLen >> 8) & 0x00FF;
                    memcpy(&verRsp[6], compileDateTime, HEADER_LEN + verRsp[4]);
                    write(newsockfd, verRsp, HEADER_LEN + verRsp[4]);
                }
                break;
                case CMD_BOARD: {
                    ifDebug ? printf("Board Name : %s\n",boardName) : 0;
                    stringLen = strlen(boardName) + 1; // for NULL terminator
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = stringLen & 0x00FF;
                    verRsp[5] = (stringLen >> 8) & 0x00FF;
                    memcpy(&verRsp[6], boardName, HEADER_LEN + verRsp[4]);
                    write(newsockfd, verRsp, HEADER_LEN + verRsp[4]);
                }
                break;
                case CMD_READER_IC_MAJOR : {
                    ifDebug ? printf("FW Manjor Version : 0x%X\n",ReaderIC_Major) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ReaderIC_Major;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_READER_IC_MINOR : {
                    ifDebug ? printf("FW Minor version : 0x%X\n",ReaderIC_Minor) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ReaderIC_Minor;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_READER_IC_HW_VER : {
                    ifDebug ? printf("IC HW Version : 0x%X\n",ReaderIC_HwVer) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ReaderIC_HwVer;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_READER_IC_MODEL_ID : {
                    ifDebug ? printf("IC Model ID : 0x%X\n",ReaderIC_ModelID) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ReaderIC_ModelID;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                case CMD_READER_IC_ROM_VER : {
                    ifDebug ? printf("IC ROM Version : 0x%X\n",ReaderIC_RomVer) : 0;
                    memcpy(verRsp, &buff[0], 4);
                    verRsp[4] = 0x01;
                    verRsp[5] = 0x00;
                    verRsp[6] = ReaderIC_RomVer;
                    write(newsockfd, verRsp, 0x07);
                }
                break;
                default:{
                    ifDebug ? printf("Class [0x%X] Command [0x%X] is not supported\n",class,cmd) : 0;
                    memcpy(verRsp, &buff[0], 2);
                    verRsp[2] = (uint8_t)(PH_ERR_INVALID_PARAMETER & 0x00FF);
                    verRsp[3] = (uint8_t)((PH_ERR_INVALID_PARAMETER >> 8 ) & 0x00FF);
                    verRsp[4] = 0x00;
                    verRsp[5] = 0x00;
                    write(newsockfd, verRsp, HEADER_LEN);
                }
                break;
              }
            }
            break;

            case CLS_TRANSRECEIVE : {
                switch (cmd) {
                case CMD_TRANSMIT : {
                        memcpy(verRsp, &buff[0], 0x02);
                        verRsp[4] = 0x00; /* No Payload */
                        verRsp[5] = 0x00; /* No Payload */

                        if(send_raw_cmd(pBuff,payloadLen) == false) {
                            ifDebug ? printf("cmd write failed\n") : 0;
                            verRsp[2] = (uint8_t)(PH_ERR_READ_WRITE_ERROR & 0x00FF);
                            verRsp[3] = (uint8_t)((PH_ERR_READ_WRITE_ERROR >> 8 ) & 0x00FF);
                        } else {
                            verRsp[2] = (uint8_t)(PH_SUCCESS & 0x00FF);
                            verRsp[3] = (uint8_t)((PH_SUCCESS >> 8) & 0x00FF);
                        }
                        write(newsockfd, verRsp, HEADER_LEN);
                        if(pBuff != NULL){
                            free(pBuff);
                            pBuff = NULL;
                        }
                    }
                    break;
                case CMD_RECEIVE : {
                        uint8_t readBytes = p1;
                        if((remDataLen != 0x00) && (readBytes <= remDataLen)) {
                            memcpy(verRsp, &buff[0], 0x02);
                            verRsp[4] = readBytes; /* Num of byte read */
                            verRsp[5] = 0x00; /* No Payload */
                            verRsp[2] = (uint8_t)(PH_SUCCESS & 0x00FF);
                            verRsp[3] = (uint8_t)((PH_SUCCESS >> 8) & 0x00FF);
                            memcpy(&verRsp[6], pUpdatedIndex, readBytes);

                            pUpdatedIndex += (readBytes);
                            remDataLen -= (readBytes);

                            write(newsockfd, verRsp, HEADER_LEN + readBytes);
                        } else {
                            ifDebug ? printf("Invalid readBytes %d\nAvailable resp data %d\n", readBytes,remDataLen) : 0;
                            memcpy(verRsp, &buff[0], 2);
                            verRsp[2] = (uint8_t)(PH_ERR_INVALID_PARAMETER & 0x00FF);
                            verRsp[3] = (uint8_t)((PH_ERR_INVALID_PARAMETER >> 8 ) & 0x00FF);
                            verRsp[4] = 0x00;
                            verRsp[5] = 0x00;
                            write(newsockfd, verRsp, HEADER_LEN);
                        }
                    }
                    break;
                }
            }
            break;

            case CLS_GPIO : {
                switch (cmd) {
                    case CMD_GET_V : {
                        if(p1 == P1_GPIO_IRQ ){
                            memcpy(verRsp, &buff[0], 2);
                            verRsp[2] = 0x00;
                            verRsp[3] = 0x00;
                            verRsp[4] = 0x01;
                            verRsp[5] = 0x00;
                            verRsp[6] = (remDataLen == 0x00) ? 0x00 : 0x01;
                            write(newsockfd, verRsp, 0x07);
                        } else {
                            ifDebug ? printf("Class [0x%X] Command [0x%X] P1 [0x%X] is not supported\n",class,cmd,p1) : 0;
                            memcpy(verRsp, &buff[0], 2);
                            verRsp[2] = (uint8_t)(PH_ERR_INVALID_PARAMETER & 0x00FF);
                            verRsp[3] = (uint8_t)((PH_ERR_INVALID_PARAMETER >> 8 ) & 0x00FF);
                            verRsp[4] = 0x00;
                            verRsp[5] = 0x00;
                            write(newsockfd, verRsp, HEADER_LEN);
                        }
                    }
                    break;
                    default : {
                        ifDebug ? printf("Class [0x%X] Command [0x%X] is not supported\n",class,cmd) : 0;
                        memcpy(verRsp, &buff[0], 2);
                        verRsp[2] = (uint8_t)(PH_ERR_INVALID_PARAMETER & 0x00FF);
                        verRsp[3] = (uint8_t)((PH_ERR_INVALID_PARAMETER >> 8 ) & 0x00FF);
                        verRsp[4] = 0x00;
                        verRsp[5] = 0x00;
                        write(newsockfd, verRsp, HEADER_LEN);
                    }
                    break;
                }
            }
            break;
            default : {
                ifDebug ? printf("Class [0x%X] is not supported\n",class) : 0;
                memcpy(verRsp, &buff[0], 2);
                verRsp[2] = (uint8_t)(PH_ERR_INVALID_PARAMETER & 0x00FF);
                verRsp[3] = (uint8_t)((PH_ERR_INVALID_PARAMETER >> 8 ) & 0x00FF);
                verRsp[4] = 0x00;
                verRsp[5] = 0x00;
                write(newsockfd, verRsp, HEADER_LEN);
            }
          }
    }

    phNxpNciHal_shutdown();
    close(newsockfd);
    close(sockfd);
    sem_destroy(&semaphore);
    return;
}

bool isValidNCIPacket(const uint8_t* packet, uint16_t packetLength) {
    // Ensure the packet length is at least 3 bytes (minimum valid NCI packet length)
    if (packetLength < 3) {
        ifDebug ? printf("Error :  invalid pkt len 0x%X\n",packetLength) : 0;
        return false;
    }
    /* This tool don't expecting empty NCI packet */
    if (packetLength == 3) {
        ifDebug ? printf("Empty packet, Might be invalid packet\n") : 0;
        return false;
    }

    // Check the length byte against the actual packet length
    uint8_t length = packet[2];
    if (packetLength != (length + 3)) {
        ifDebug ? printf("Error :  invalid payload len 0x%X\n",packetLength) : 0;
        return false;
    }

    // Return true if the packet is valid
    return true;
}
