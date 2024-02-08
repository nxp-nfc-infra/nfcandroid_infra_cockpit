/*
 * Copyright 2022 NXP
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

#ifndef __LOG_H__
#define __LOG_H__

#include <android/log.h>

#define _TAG "NXP_P61"

#define DEBUG
#define MW_LOG_DEBUG    ANDROID_LOG_DEBUG
#define MW_LOG_WARN     ANDROID_LOG_WARN
#define MW_LOG_ERROR    ANDROID_LOG_ERROR
#define MW_LOG_VERBOSE  ANDROID_LOG_VERBOSE
#define MW_LOG_INFO     ANDROID_LOG_INFO
#define mw_log_print    __android_log_print

#define ALOGV(...) mw_log_print(MW_LOG_VERBOSE, _TAG, __VA_ARGS__)
#define ALOGI(...) mw_log_print(MW_LOG_INFO, _TAG, __VA_ARGS__)
#define ALOGE(...) mw_log_print(MW_LOG_ERROR, _TAG, __VA_ARGS__)

#ifdef DEBUG
#define ALOGD(...) mw_log_print(MW_LOG_DEBUG, _TAG, __VA_ARGS__)
#define ALOGW(...) mw_log_print(MW_LOG_WARN, _TAG, __VA_ARGS__)
#else
#define ALOGD(...)
#define ALOGW(...)
#endif  // DEBUG

#endif /* __LOG_H__ */
