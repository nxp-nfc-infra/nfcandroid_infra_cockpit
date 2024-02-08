/*
 * Copyright 2022-2024 NXP.
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only be
 * used strictly in accordance with the applicable license terms. By expressly
 * accepting such terms or by downloading, installing, activating and/or
 * otherwise using the software, you are agreeing that you have read, and that
 * you agree to comply with and are bound by, such license terms. If you do not
 * agree to be bound by the applicable license terms, then you may not
 * retain,install, activate or otherwise use the software.
 */

#include <getopt.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phNfcStatus.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <scr_common.h>
#include <stdio.h>

#define PORT_NUMBER 8059
extern char *serverVersion;

#undef FLUSH_BUFFER
HalCmd_Handler_t gCmd_Handler;
extern phPalEse_Config_t gPalConfig;
scrModes modeType = 0;

bool ifDebug = false;

struct option scr_options[] = {
    {"type", required_argument, 0, 't'},
    {"dev", required_argument, 0, 'd'},
    {"file", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0},
};

/******************************************************************************
 * Function         phNxpPn54x_ScrUsage
 *
 * Description      This function give error message in case of wrong argument
 *                  option.
 *
 * Parameters       err_msg - predefined/custom error message
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpPn54x_ScrUsage(char *err_msg) { fprintf(stderr, "%s", err_msg); }

/******************************************************************************
 * Function         phNxpPn54x_GetScrArg
 *
 * Description      This function store arguments in char array based on
 *                  argument options.
 *
 * Parameters       argc - Argument count
 *                  argv - Arguments passed to binary in execution command.
 *                  driver_name - pointer to store driver name.
 *                  file_name - pointer to store script file name.
 *                  type_of_test - execution mode of PNSCR
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpPn54x_GetScrArg(int argc, char **argv, char *driver_name,
                          char *file_name, long *type_of_test) {
  int option, option_index = 0;
  unsigned long temp = 0;
  char dName[LEN_DRIVER_NAME] = "nxpnfc";
  char fName[LEN_FILE_NAME] = "/data/nfc/pn54x.scr";
  unsigned char check_all_arg = 0x00;

  // memset(ptr_arg_data, 0x00, sizeof(scr_arg_data_t));
  strcpy(driver_name, "nxpnfc");

  memset(&gPalConfig, 0x00, sizeof(gPalConfig));
  gPalConfig.pDevName = (int8_t *)"/dev/p61";
  strcpy(file_name, "/data/nfc/pn54x.scr");

  while ((option = getopt_long(argc, argv, "t:d:f:h:", scr_options,
                               &option_index)) != -1) {
    switch (option) {
      case 't':
        temp = 0;
        sscanf(optarg, "%ld", &temp);
        *type_of_test = temp;
        check_all_arg = check_all_arg | 0x01;
        break;
      case 'd':
        memset(dName, 0x00, sizeof(dName));
        sscanf(optarg, "%s", dName);
        strcpy(driver_name, dName);
        if (!strncmp(driver_name, DEVICE_PN553, 5))
          gPalConfig.pDevName = (int8_t *)DEVICE_P73;
        if (!strncmp(driver_name, DEVICE_NXPNCI, 7))
          gPalConfig.pDevName = (int8_t *)DEVICE_P73;
        check_all_arg = check_all_arg | 0x02;
        break;
      case 'f':
        sscanf(optarg, "%s", fName);
        strcpy(file_name, fName);
        check_all_arg = check_all_arg | 0x04;
        break;
      case 'h':
        phNxpPn54x_ScrUsage(ARG_ERR);
        exit(EXIT_FAILURE);
        break;
      default:
        printf("Wrong argument\n");
        break;
    }
  }
}

/******************************************************************************
 * Function         get_mode
 *
 * Description      This function get IOCTL reset value from input script.
 *
 * Parameters       file_name - input script file.
 *
 * Returns          -1 If erro while opening script file,
 *                  otherwise reset value between 0 to 6.
 *
 ******************************************************************************/
int get_mode(char *file_name) {
  char *strData;
  char *cmdArr;
  scr_cmd cmd_type = NONE;
  int cmdArr_len;

  unsigned long reset_val = 0;
  cmdArr = malloc(LEN_HEX_COMMAND);
  strData = malloc(LEN_STR_COMMAND);

  cmdArr_len = readScrline(file_name, strData, &cmd_type);

  if (cmdArr_len == -1) {
    printf("Error in opening script\n");
    exit(EXIT_FAILURE);
  }

  if (cmdArr_len > 0 || (cmd_type == END)) {
    if (cmd_type == RESET) {
      memset(cmdArr, 0x00, LEN_HEX_COMMAND);
      if (str2hex(cmdArr, strData, strlen(strData)) == 0) {
        reset_val = get_val(cmdArr_len, cmdArr);
      }
    } else if ((RESET < cmd_type) && (cmd_type < EOS))
      reset_val = 0;
  }
  pullBackScrstart();

  closeScrfd();

  NXPLOG_EXTNS_D("get_mode, mode : %d", (int)reset_val);
  if (strData != NULL) {
    free(strData);
  }
  if (cmdArr != NULL) {
    free(cmdArr);
  }
  return reset_val;
}

/******************************************************************************
 * Function         flush_buffer
 *
 * Description      This function will
 *                    1. turn on NFCC.
 *                    2. send CORE RESET.
 *                    3. turn of NFCC.
 *
 * Returns          void
 *
 ******************************************************************************/
void flush_buffer(void) {
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  phNxpNciHal_setflush(1);
  if (phNxpNciHal_open(p_hal_cback, p_hal_data_callback) != NFCSTATUS_SUCCESS) {
    printf("Error in phNxpNciHal_open\n");
    exit(EXIT_FAILURE);
  }
  phNxpNciHal_ioctl(RESET_TYPE_NFC_ON);
  usleep(30 * MICRO_TO_MILLI_SEC);
  phNxpNciHal_write(sizeof(cmd_reset_nci), cmd_reset_nci);
  usleep(30 * MICRO_TO_MILLI_SEC);

  phNxpNciHal_close();
  phNxpNciHal_ioctl(RESET_TYPE_NFC_OFF);
  usleep(30 * MICRO_TO_MILLI_SEC);
  phNxpNciHal_setflush(0);
}

/******************************************************************************
 * Function         close_scr
 *
 * Description      This function will be called after executing all commands.
 *                  This function will execute following operations.
 *                    1. release SPI thread.
 *                    2. Ncihal_close if NCI hal is opened.
 *                    3. release client thread.
 *
 * Returns          void
 *
 ******************************************************************************/
int close_scr() {
  releaseSPIThread();
  if (gCmd_Handler.bHal_opened == TRUE) {
    phNxpNciHal_close();
  }
  phDal4Nfc_msgrelease(gCmd_Handler.nClientId);
  return 0;
}

/******************************************************************************
 * Function         main
 *
 * Description      main function gets all arguments from execution cammand.
 *                  This function call different function based on execution
 *                  mode.
 *
 * Returns          0 on success
 *
 ******************************************************************************/
int main(int argc, char *argv[]) {
  char *driver_name = malloc(LEN_DRIVER_NAME);
  char *file_name = malloc(LEN_FILE_NAME);
  long type_of_test;

  int mode = 0;

  if (argc < 2) {
      printf("Usage: %s <true|false>\n", argv[0]);
      return 1;
  }

  ifDebug = (strcmp(argv[1], "true") == 0);

  bool_t spi_thread_status = FALSE;
  memset(&gCmd_Handler, 0x00, sizeof(gCmd_Handler));
  gCmd_Handler.bHal_opened = FALSE;
  gCmd_Handler.bNeed_wait = FALSE;
  phNxpPn54x_GetScrArg(argc, argv, driver_name, file_name, &type_of_test);

  if (0 != sem_init(&gCmd_Handler.wait_sem, 0, 0)) {
    printf("FAILED in wait_sem sem_init\n");
    free(driver_name);
    free(file_name);
    return close_scr();
  }

  printf("******************* NXP Semiconductor ****************\n");
  printf("                                                      \n");
  printf("                      %s                  \n", serverVersion);
  printf("                                                      \n");
  printf("******************************************************\n");
  phNxpLog_InitializeLogLevel();

  phNxpNciHal_setdevname(driver_name);

  if (type_of_test != LINE_MODE && type_of_test != SOCKET_MODE) {
    mode = get_mode(file_name);

    if (mode == RESET_TYPE_NFC_DL)
      phNxpNciHal_set_dnld_flag(1);
    else
      phNxpNciHal_set_dnld_flag(0);
  } else
    phNxpNciHal_set_dnld_flag(0);

#ifdef FLUSH_BUFFER
  flush_buffer();
#endif
  modeType = type_of_test;
  gCmd_Handler.nClientId = phDal4Nfc_msgget(0, 0600);
  spi_thread_status = createSPIThread();

  if (type_of_test == LINE_MODE) {
    execute_line(&gCmd_Handler);
  } else if ((type_of_test == SCRIPT_MODE) || (type_of_test == HYBRID_MODE)) {
    execute_script(file_name);
    if (type_of_test == HYBRID_MODE) {
      printf("\nMode : Hybrid mode\n");
      type_of_test = 0;
      execute_line();
    }
  }
  else if(type_of_test == SOCKET_MODE) {
    proc_server((uint32_t)PORT_NUMBER);
  }
  free(driver_name);
  free(file_name);
  return close_scr();
}
