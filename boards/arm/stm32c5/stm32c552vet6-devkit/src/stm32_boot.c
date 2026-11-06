/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/src/stm32_boot.c
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
#include <nuttx/board.h>
#include <nuttx/debug.h>
#include <nuttx/input/buttons.h>
#include <nuttx/leds/userled.h>

#include <arch/board/board.h>

#include "board_common.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void stm32_board_clockconfig(void)
{
  /* Start from a system reset.  HSIDIV3 already drives SYSCLK at 48 MHz,
   * the bus prescalers are /1, and the reset FLASH latency is safe.
   * The common C5 code enables the peripheral clocks after this hook.
   */
}

void stm32_board_initialize(void)
{
  board_autoled_initialize();
}

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret = stm32_common_initialize();

  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: stm32_common_initialize() failed: %d\n", ret);
    }

#if defined(CONFIG_USERLED_LOWER) && !defined(CONFIG_ARCH_LEDS)
  ret = userled_lower_initialize("/dev/userleds");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: userled_lower_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_INPUT_BUTTONS_LOWER
  ret = btn_lower_initialize("/dev/buttons");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: btn_lower_initialize() failed: %d\n", ret);
    }
#endif
}
#endif
