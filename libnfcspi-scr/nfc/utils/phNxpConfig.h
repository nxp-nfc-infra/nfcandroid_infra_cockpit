/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright 2013-2014,2022-2023 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#ifndef __CONFIG_H
#define __CONFIG_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NONE,
  RESET,
  SEND,
  TRIGGER,
  SLEEP,
  LOOPS,
  END,
  INTERVAL,
  RESETSPI,
  PWRREQ,
  SENDSPI,
  STOP,
  EOS
} scr_cmd;

typedef enum {
  SPI_OPEN = 0x500,
  SPI_CLOSE,
  SPI_TRANSCIEVE,
  SPI_PWR_ON,
  SPI_PWR_OFF
} spi_cmd_type;

typedef enum { NCI_WRITE = 0x600 } write_cmd_type;

#define NS_EXPORT
// extern "C" {
NS_EXPORT __attribute__((weak)) void* __dso_handle;
//}

void resetNxpConfig(void);
void closeScrfd(void);
int readScrline(const char* name, char* data_buf, scr_cmd* cmd);
bool pullBackScrline();
bool pullBackScrstart();

void set_loop_count(unsigned long count);

#ifdef __cplusplus
};
#endif

#endif
