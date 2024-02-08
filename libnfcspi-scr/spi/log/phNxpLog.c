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

/* ############################################### Header Includes
 * ################################################ */
#if !defined(NXPLOG__H_INCLUDED)
#include <phNxpSpiLog.h>
#endif

const char* NXPLOG_ITEM_ESELIB = "NxpEseLib";
const char* NXPLOG_ITEM_SPIX = "NxpEseDataX";
const char* NXPLOG_ITEM_SPIR = "NxpEseDataR";
const char* NXPLOG_ITEM_PAL = "NxpEsePal";

/* global log level structure */
spi_log_level_t gSPILog_level;

void phNxpLog_SPIInitializeLogLevel(void) {
  gSPILog_level.global_log_level = NXPLOG_DEFAULT_LOGLEVEL;
  gSPILog_level.eselib_log_level = NXPLOG_DEFAULT_LOGLEVEL;
  gSPILog_level.pal_log_level = NXPLOG_DEFAULT_LOGLEVEL;
  gSPILog_level.spir_log_level = NXPLOG_LOG_DEBUG_LOGLEVEL;
  gSPILog_level.spix_log_level = NXPLOG_LOG_DEBUG_LOGLEVEL;

  ALOGD("%s: global =%u, lib =%u, pal =%u, spir =%u, spix =%u", __FUNCTION__,
        gSPILog_level.global_log_level, gSPILog_level.eselib_log_level,
        gSPILog_level.pal_log_level, gSPILog_level.spir_log_level,
        gSPILog_level.spix_log_level);
}
