/****************************************************************************
 * arch/arm/src/stm32c5/stm32_adc.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32C5_STM32_ADC_H
#define __ARCH_ARM_SRC_STM32C5_STM32_ADC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <nuttx/analog/adc.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* ADC1, one external channel (0..11), 12-bit software-triggered sampling.
 * GPIO analog configuration is owned by the board.  Internal channels and
 * ADC2 are intentionally rejected until their setup is implemented.
 */

struct adc_dev_s *stm32_adc_initialize(int intf, const uint8_t *chanlist,
                                     int nchannels);

#endif /* __ARCH_ARM_SRC_STM32C5_STM32_ADC_H */
