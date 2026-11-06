/****************************************************************************
 * arch/arm/src/stm32c5/stm32_rcc.c
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

#include <arch/board/board.h>

#include "arm_internal.h"
#include "stm32_rcc.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void stm32_configureclock(void)
{
#ifdef CONFIG_ARCH_BOARD_STM32_CUSTOM_CLOCKCONFIG
  stm32_board_clockconfig();
#else
  stm32_stdclockconfig();
#endif
}

#ifdef CONFIG_STM32_SRAM2_INIT
static void stm32_sram2init(void)
{
  volatile uint32_t *word = (volatile uint32_t *)STM32_SRAM2_BASE;
  volatile uint32_t *end = word + STM32_SRAM2_SIZE / sizeof(*word);

  /* Keep ECC at the option-byte setting.  Use aligned full-word writes to
   * initialize its check bits without an implicit read of uninitialized RAM.
   */

  modifyreg32(STM32_RCC_AHB1ENR, 0, RCC_AHB1ENR_SRAM2EN);
  (void)getreg32(STM32_RCC_AHB1ENR);

  while (word < end)
    {
      *word++ = 0;
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name
 *
 * Description
 *   Called to establish the clock settings based on the values in board.h.
 *   This function (by default) will reset most everything, enable the PLL,
 *   and enable peripheral clocking for all peripherals enabled in the NuttX
 *   configuration file.
 *
 *   If CONFIG_ARCH_BOARD_STM32_CUSTOM_CLOCKCONFIG is defined, then
 *   clocking will be enabled by an externally provided, board-specific
 *   function called stm32_board_clockconfig().
 *
 * Input Parameters
 *   None
 *
 * Returned Value
 *   None
 *
 ****************************************************************************/

void stm32_clockconfig(void)
{
  stm32_configureclock();

  /* Enable peripheral clocking */

  stm32_rcc_enableperipherals();

#ifdef CONFIG_STM32_SRAM2_INIT
  stm32_sram2init();
#endif
}

/****************************************************************************
 * Name
 *
 * Description
 *   Re-enable the clock and restore the clock settings based on settings in
 *   board.h.  This function is only available to support low-power modes of
 *   operation
 *   re-enable/re-start the PLL
 *
 *   This function performs a subset of the operations performed by
 *   stm32_clockconfig()
 *   reset the currently enabled peripheral clocks.
 *
 *   If CONFIG_ARCH_BOARD_STM32_CUSTOM_CLOCKCONFIG is defined, then
 *   clocking will be enabled by an externally provided, board-specific
 *   function called stm32_board_clockconfig().
 *
 * Input Parameters
 *   None
 *
 * Returned Value
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_PM
void stm32_clockenable(void)
{
  stm32_configureclock();
}
#endif
