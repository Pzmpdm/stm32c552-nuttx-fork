/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/src/stm32_adc.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <nuttx/analog/adc.h>

#include "stm32_adc.h"
#include "stm32_gpio.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int stm32_adc_setup(void)
{
  static const uint8_t channels[] =
  {
    8
  };

  struct adc_dev_s *adc;
  int ret;

  ret = stm32_configgpio(GPIO_ADC1_IN8_1);
  if (ret < 0)
    {
      return ret;
    }

  adc = stm32_adc_initialize(1, channels, 1);
  return adc == NULL ? -ENODEV : adc_register("/dev/adc0", adc);
}
