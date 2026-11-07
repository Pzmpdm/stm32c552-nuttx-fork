/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/src/stm32_spi.c
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
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>
#include <arch/board/board.h>

#include "stm32_gpio.h"
#include "stm32_spi.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void stm32_spi1select(struct spi_dev_s *dev, uint32_t devid, bool selected)
{
  stm32_gpiowrite(GPIO_SPI1_CS, !(selected && devid == SPIDEV_USER(0)));
}

uint8_t stm32_spi1status(struct spi_dev_s *dev, uint32_t devid)
{
  return 0;
}

void stm32_spi1_initialize(void)
{
  /* Configure inactive CS before enabling the SPI alternate functions. */

  stm32_configgpio(GPIO_SPI1_CS);
}

#ifdef CONFIG_SPI_DRIVER
int stm32_spi1_register(void)
{
  struct spi_dev_s *spi;

  spi = stm32_spibus_initialize(1);
  return spi == NULL ? -ENODEV : spi_register(spi, 1);
}

#endif /* CONFIG_SPI_DRIVER */
