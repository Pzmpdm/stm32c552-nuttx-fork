/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/include/board.h
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

#ifndef __BOARDS_ARM_STM32C5_STM32C552VET6_DEVKIT_INCLUDE_BOARD_H
#define __BOARDS_ARM_STM32C5_STM32C552VET6_DEVKIT_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Both clock profiles use the reset bus prescalers.  The optional HSE
 * profile uses 144 MHz PSIS or HSIS.  The default uses 48 MHz HSIDIV3.
 */

#define STM32_HSE_FREQUENCY          8000000ul
#define STM32_LSE_FREQUENCY          32768ul
#ifdef CONFIG_STM32C552VET6_DEVKIT_HSE_CLOCK
#  define STM32_SYSCLK_FREQUENCY     144000000ul
#else
#  define STM32_SYSCLK_FREQUENCY     48000000ul
#endif
#define STM32_HCLK_FREQUENCY         STM32_SYSCLK_FREQUENCY
#define STM32_PCLK1_FREQUENCY        STM32_HCLK_FREQUENCY
#define STM32_PCLK2_FREQUENCY        STM32_HCLK_FREQUENCY
#define STM32_PCLK3_FREQUENCY        STM32_HCLK_FREQUENCY

/* APB1 is undivided, so TIM2 uses the same rate as PCLK1. */

#define STM32_TIM2_CLKIN             STM32_PCLK1_FREQUENCY

/* H4 pin 4: PA9 TX; H4 pin 6: PA10 RX.  Both use AF7. */

#define GPIO_USART1_TX               GPIO_USART1_TX_2
#define GPIO_USART1_RX               GPIO_USART1_RX_2

/* H2 pin 1: PA2 TX; H2 pin 6: PA3 RX.  Both use AF7. */

#define GPIO_USART2_TX               GPIO_USART2_TX_1
#define GPIO_USART2_RX               GPIO_USART2_RX_1

/* H3 pins 21/20: PB8 SCL / PB9 SDA, AF4, external pull-ups required. */

#define GPIO_I2C1_SCL                (GPIO_I2C1_SCL_3 | GPIO_OPENDRAIN | \
                                     GPIO_SPEED_50MHZ)
#define GPIO_I2C1_SDA                (GPIO_I2C1_SDA_2 | GPIO_OPENDRAIN | \
                                     GPIO_SPEED_50MHZ)

/* 48 MHz PCLK1, analog/digital filters off.  Conservative TIMINGR values
 * for rise <= 300 ns and fall <= 100 ns.  Actual SCL must be measured.
 */

#define STM32_I2C_TIMINGR_100KHZ     0x20814f4ful
#define STM32_I2C_TIMINGR_400KHZ     0x20611214ul

/* H2 pin 3: PA0 TIM2_CH1 output, AF1. */

#define GPIO_TIM2_CH1OUT             GPIO_TIM2_CH1OUT_1

/* LED2 and SW2 are both active low. */

#define BOARD_LED_USER               0
#define BOARD_NLEDS                  1
#define BOARD_LED_USER_BIT           (1 << BOARD_LED_USER)

#define LED_STARTED                  0
#define LED_HEAPALLOCATE             0
#define LED_IRQSENABLED              0
#define LED_STACKCREATED             1
#define LED_INIRQ                    0
#define LED_SIGNAL                   0
#define LED_ASSERTION                0
#define LED_PANIC                    2
#define LED_IDLE                     0

#define BUTTON_USER                  0
#define NUM_BUTTONS                  1
#define BUTTON_USER_BIT              (1 << BUTTON_USER)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#ifdef __cplusplus
extern "C"
{
#endif

void stm32_board_clockconfig(void);
void stm32_board_initialize(void);

#ifdef __cplusplus
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __BOARDS_ARM_STM32C5_STM32C552VET6_DEVKIT_INCLUDE_BOARD_H */
