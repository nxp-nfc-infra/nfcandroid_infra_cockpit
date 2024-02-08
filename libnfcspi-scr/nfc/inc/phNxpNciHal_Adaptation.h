/*
 * Copyright 2012-2014,2023 NXP
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

#ifndef _PHNXPNCIHAL_ADAPTATION_H_
#define _PHNXPNCIHAL_ADAPTATION_H_

#include <hardware/hardware.h>
#include <hardware/nfc.h>

typedef struct {
  struct nfc_nci_device nci_device;

  /* Local definitions */
} pn547_dev_t;

/* NXP HAL functions */

int phNxpNciHal_open(nfc_stack_callback_t *p_cback,
                     nfc_stack_data_callback_t *p_data_cback);
int phNxpNciHal_write(uint16_t data_len, const uint8_t *p_data);
int phNxpNciHal_close(void);
void phNxpNciHal_shutdown(void);
void phNxpNciHal_setdevname(char *name);
void phNxpNciHal_ioctl(int value);
void phNxpNciHal_setflush(uint8_t value);
void phNxpNciHal_set_dnld_flag(int value);

#endif /* _PHNXPNCIHAL_ADAPTATION_H_ */
