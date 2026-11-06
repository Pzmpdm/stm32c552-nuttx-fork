/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/src/stm32_boardgpio.c
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
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/board.h>

#include <arch/board/board.h>

#include "stm32_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GPIO_LED_USER (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_2MHZ | \
                       GPIO_OUTPUT_SET | GPIO_PORTH | GPIO_PIN3)
#define GPIO_BTN_USER (GPIO_INPUT | GPIO_PULLUP | GPIO_EXTI | \
                       GPIO_PORTC | GPIO_PIN13)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_initialize(void)
{
  stm32_configgpio(GPIO_LED_USER);
}

void board_autoled_on(int led)
{
  if (led == LED_STACKCREATED || led == LED_PANIC)
    {
      stm32_gpiowrite(GPIO_LED_USER, false);
    }
}

void board_autoled_off(int led)
{
  if (led == LED_PANIC)
    {
      stm32_gpiowrite(GPIO_LED_USER, true);
    }
}
#else
uint32_t board_userled_initialize(void)
{
  stm32_configgpio(GPIO_LED_USER);
  return BOARD_NLEDS;
}

void board_userled(int led, bool ledon)
{
  if (led == BOARD_LED_USER)
    {
      stm32_gpiowrite(GPIO_LED_USER, !ledon);
    }
}

void board_userled_all(uint32_t ledset)
{
  board_userled(BOARD_LED_USER, (ledset & BOARD_LED_USER_BIT) != 0);
}

#ifdef CONFIG_USERLED_LOWER_READSTATE
void board_userled_getall(uint32_t *ledset)
{
  *ledset = stm32_gpioread(GPIO_LED_USER) ? 0 : BOARD_LED_USER_BIT;
}
#endif
#endif

#ifdef CONFIG_ARCH_BUTTONS
uint32_t board_button_initialize(void)
{
  stm32_configgpio(GPIO_BTN_USER);
  return NUM_BUTTONS;
}

uint32_t board_buttons(void)
{
  return stm32_gpioread(GPIO_BTN_USER) ? 0 : BUTTON_USER_BIT;
}

#ifdef CONFIG_ARCH_IRQBUTTONS
int board_button_irq(int id, xcpt_t irqhandler, void *arg)
{
  if (id != BUTTON_USER)
    {
      return -EINVAL;
    }

  return stm32_gpiosetevent(GPIO_BTN_USER, true, true, true,
                            irqhandler, arg);
}
#endif
#endif
