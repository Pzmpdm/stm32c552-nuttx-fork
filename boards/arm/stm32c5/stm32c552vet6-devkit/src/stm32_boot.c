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

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <nuttx/board.h>
#include <nuttx/debug.h>
#include <nuttx/input/buttons.h>
#include <nuttx/leds/userled.h>

#include <arch/board/board.h>

#include "board_common.h"

#if defined(CONFIG_PWM) && defined(CONFIG_STM32_TIM2_PWM)
#  include "stm32_pwm.h"
#endif

#ifdef CONFIG_STM32C552VET6_DEVKIT_HSE_CLOCK
#  include "arm_internal.h"
#  include "hardware/stm32_flash.h"
#  include "hardware/stm32_rcc.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_STM32C552VET6_DEVKIT_HSE_CLOCK
#  define CLOCK_READY_TIMEOUT 240000
#  define FLASH_CLOCK_MASK    (FLASH_ACR_LATENCY_MASK | \
                               FLASH_ACR_WRHIGHFREQ_MASK)
#  define FLASH_CLOCK_144MHZ  (FLASH_ACR_LATENCY_4 | FLASH_ACR_WRHIGHFREQ_2)
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_STM32C552VET6_DEVKIT_HSE_CLOCK
static bool stm32_wait_clockreg(uintptr_t regaddr, uint32_t mask,
                               uint32_t expected)
{
  unsigned int timeout;

  for (timeout = CLOCK_READY_TIMEOUT; timeout > 0; timeout--)
    {
      if ((getreg32(regaddr) & mask) == expected)
        {
          return true;
        }
    }

  return false;
}

static bool stm32_try_hse_psi(void)
{
  modifyreg32(STM32_RCC_CR1, RCC_CR1_HSEBYP | RCC_CR1_HSEEXT, RCC_CR1_HSEON);
  if (!stm32_wait_clockreg(STM32_RCC_CR1, RCC_CR1_HSERDY, RCC_CR1_HSERDY))
    {
      return false;
    }

  modifyreg32(STM32_RCC_CR2,
              RCC_CR2_PSIREFSRC_MASK | RCC_CR2_PSIREF_MASK |
              RCC_CR2_PSIFREQ_MASK,
              RCC_CR2_PSIREFSRC_HSE | RCC_CR2_PSIREF_8MHZ |
              RCC_CR2_PSIFREQ_144MHZ);

  modifyreg32(STM32_RCC_CR1, 0, RCC_CR1_PSISON);
  if (!stm32_wait_clockreg(STM32_RCC_CR1, RCC_CR1_PSISRDY, RCC_CR1_PSISRDY))
    {
      return false;
    }

  modifyreg32(STM32_RCC_CFGR1, RCC_CFGR1_SW_MASK, RCC_CFGR1_SW_PSIS);
  return stm32_wait_clockreg(STM32_RCC_CFGR1, RCC_CFGR1_SWS_MASK,
                            RCC_CFGR1_SWS_PSIS);
}

static bool stm32_try_hsis(void)
{
  modifyreg32(STM32_RCC_CR1, 0, RCC_CR1_HSISON);
  if (!stm32_wait_clockreg(STM32_RCC_CR1, RCC_CR1_HSISRDY, RCC_CR1_HSISRDY))
    {
      return false;
    }

  modifyreg32(STM32_RCC_CFGR1, RCC_CFGR1_SW_MASK, RCC_CFGR1_SW_HSIS);
  if (!stm32_wait_clockreg(STM32_RCC_CFGR1, RCC_CFGR1_SWS_MASK,
                           RCC_CFGR1_SWS_HSIS))
    {
      return false;
    }

  modifyreg32(STM32_RCC_CR1, RCC_CR1_PSISON | RCC_CR1_HSEON, 0);
  return true;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void stm32_board_clockconfig(void)
{
#ifdef CONFIG_STM32C552VET6_DEVKIT_HSE_CLOCK
  /* Confirm the Flash timings before switching from reset HSIDIV3. */

  modifyreg32(STM32_FLASH_ACR, FLASH_CLOCK_MASK, FLASH_CLOCK_144MHZ);
  if (stm32_wait_clockreg(STM32_FLASH_ACR, FLASH_CLOCK_MASK,
                          FLASH_CLOCK_144MHZ) &&
      (stm32_try_hse_psi() || stm32_try_hsis()))
    {
      return;
    }

  /* Neither 144 MHz path is safe.  Starting with the wrong rate would
   * break both the console and the scheduler clock.
   */

  for (; ; )
    {
      asm volatile ("wfi");
    }
#else
  /* HSIDIV3 already drives SYSCLK at 48 MHz from reset. */

#endif
}

void stm32_board_initialize(void)
{
  board_autoled_initialize();
#ifdef CONFIG_STM32_SPI1
  stm32_spi1_initialize();
#endif
}

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
#if defined(CONFIG_PWM) && defined(CONFIG_STM32_TIM2_PWM)
  struct pwm_lowerhalf_s *pwm;
#endif
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

#if defined(CONFIG_STM32_SPI1) && defined(CONFIG_SPI_DRIVER)
  ret = stm32_spi1_register();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: SPI1 initialization failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_PWM) && defined(CONFIG_STM32_TIM2_PWM)
  pwm = stm32_pwminitialize(2);
  ret = pwm == NULL ? -ENODEV : pwm_register("/dev/pwm0", pwm);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: TIM2 PWM initialization failed: %d\n", ret);
    }
#endif
}
#endif
