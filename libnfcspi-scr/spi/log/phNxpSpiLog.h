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

#if !defined(NXPLOG__H_INCLUDED)
#define NXPLOG__H_INCLUDED
#include <stdint.h>
#include <string.h>

#include "NXP_ESE_FEATURES.h"
#include "log.h"
#include "phEseTypes.h"

typedef struct spi_log_level {
  uint8_t global_log_level;
  uint8_t eselib_log_level;
  uint8_t pal_log_level;
  uint8_t spix_log_level;
  uint8_t spir_log_level;
} spi_log_level_t;

/* global log level Ref */
extern spi_log_level_t gSPILog_level;

#define MAX_LOG_LEVEL 0x03
/* define log module included when compile */
#define ENABLE_LIB_TRACES TRUE
#define ENABLE_PAL_TRACES TRUE
#define ENABLE_SPIX_TRACES TRUE
#define ENABLE_SPIR_TRACES TRUE

#define NXPLOG_LOG_SILENT_LOGLEVEL 0x00
#define NXPLOG_LOG_ERROR_LOGLEVEL 0x01
#define NXPLOG_LOG_WARN_LOGLEVEL 0x02
#define NXPLOG_LOG_DEBUG_LOGLEVEL 0x03

/* The Default log level for all the modules. */
#define NXPLOG_DEFAULT_LOGLEVEL NXPLOG_LOG_ERROR_LOGLEVEL

extern const char* NXPLOG_ITEM_ESELIB; /* Android logging tag for NxpEseLib */
extern const char* NXPLOG_ITEM_SPIX;   /* Android logging tag for NxpSpiX   */
extern const char* NXPLOG_ITEM_SPIR;   /* Android logging tag for NxpSpiR   */
extern const char* NXPLOG_ITEM_PAL;    /* Android logging tag for NxpPal    */

/* Logging APIs used by NxpEseLib module */
#if (ENABLE_LIB_TRACES == TRUE)
#define NXPLOG_ESELIB_I(...)                                         \
  {                                                                  \
    if (gSPILog_level.eselib_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_ESELIB, __VA_ARGS__);   \
  }
#define NXPLOG_ESELIB_D(...)                                         \
  {                                                                  \
    if (gSPILog_level.eselib_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_ESELIB, __VA_ARGS__);   \
  }
#define NXPLOG_ESELIB_W(...)                                        \
  {                                                                 \
    if (gSPILog_level.eselib_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) \
      mw_log_print(MW_LOG_WARN, NXPLOG_ITEM_ESELIB, __VA_ARGS__);   \
  }
#define NXPLOG_ESELIB_E(...)                                         \
  {                                                                  \
    if (gSPILog_level.eselib_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) \
      mw_log_print(MW_LOG_ERROR, NXPLOG_ITEM_ESELIB, __VA_ARGS__);   \
  }
#else
#define NXPLOG_ESELIB_D(...)
#define NXPLOG_ESELIB_W(...)
#define NXPLOG_ESELIB_E(...)
#endif /* Logging APIs used by LIB module */

/* Logging APIs used by NxpSpiX module */
#if (ENABLE_SPIX_TRACES == TRUE)
#define NXPLOG_SPIX_D(...)                                         \
  {                                                                \
    if (gSPILog_level.spix_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_SPIX, __VA_ARGS__);   \
  }
#define NXPLOG_SPIX_W(...)                                        \
  {                                                               \
    if (gSPILog_level.spix_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) \
      mw_log_print(MW_LOG_WARN, NXPLOG_ITEM_SPIX, __VA_ARGS__);   \
  }
#define NXPLOG_SPIX_E(...)                                         \
  {                                                                \
    if (gSPILog_level.spix_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) \
      mw_log_print(MW_LOG_ERROR, NXPLOG_ITEM_SPIX, __VA_ARGS__);   \
  }
#else
#define NXPLOG_SPIX_D(...)
#define NXPLOG_SPIX_W(...)
#define NXPLOG_SPIX_E(...)
#endif /* Logging APIs used by SPIx module */

/* Logging APIs used by NxpSpiR module */
#if (ENABLE_SPIR_TRACES == TRUE)
#define NXPLOG_SPIR_D(...)                                         \
  {                                                                \
    if (gSPILog_level.spir_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_SPIR, __VA_ARGS__);   \
  }
#define NXPLOG_SPIR_W(...)                                        \
  {                                                               \
    if (gSPILog_level.spir_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) \
      mw_log_print(MW_LOG_WARN, NXPLOG_ITEM_SPIR, __VA_ARGS__);   \
  }
#define NXPLOG_SPIR_E(...)                                         \
  {                                                                \
    if (gSPILog_level.spir_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) \
      mw_log_print(MW_LOG_ERROR, NXPLOG_ITEM_SPIR, __VA_ARGS__);   \
  }
#else
#define NXPLOG_SPIR_D(...)
#define NXPLOG_SPIR_W(...)
#define NXPLOG_SPIR_E(...)
#endif /* Logging APIs used by SPIR module */

/* Logging APIs used by NxpPal module */
#if (ENABLE_PAL_TRACES == TRUE)
#define NXPLOG_PAL_I(...)                                         \
  {                                                               \
    if (gSPILog_level.pal_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_PAL, __VA_ARGS__);   \
  }
#define NXPLOG_PAL_D(...)                                         \
  {                                                               \
    if (gSPILog_level.pal_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) \
      mw_log_print(MW_LOG_DEBUG, NXPLOG_ITEM_PAL, __VA_ARGS__);   \
  }
#define NXPLOG_PAL_W(...)                                        \
  {                                                              \
    if (gSPILog_level.pal_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) \
      mw_log_print(MW_LOG_WARN, NXPLOG_ITEM_PAL, __VA_ARGS__);   \
  }
#define NXPLOG_PAL_E(...)                                         \
  {                                                               \
    if (gSPILog_level.pal_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) \
      mw_log_print(MW_LOG_ERROR, NXPLOG_ITEM_PAL, __VA_ARGS__);   \
  }
#else
#define NXPLOG_PAL_D(...)
#define NXPLOG_PAL_W(...)
#define NXPLOG_PAL_E(...)
#endif /* Logging APIs used by NxpPal module */

void phNxpLog_SPIInitializeLogLevel(void);

#endif /* NXPLOG__H_INCLUDED */
