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

/* The 8 MHz HSE and 32.768 kHz LSE crystals are unused for NSH.
 * Use the 48 MHz HSIDIV3 system clock and the reset bus prescalers.
 */

#define STM32_HSE_FREQUENCY          8000000ul
#define STM32_LSE_FREQUENCY          32768ul
#define STM32_SYSCLK_FREQUENCY       48000000ul
#define STM32_HCLK_FREQUENCY         STM32_SYSCLK_FREQUENCY
#define STM32_PCLK1_FREQUENCY        STM32_HCLK_FREQUENCY
#define STM32_PCLK2_FREQUENCY        STM32_HCLK_FREQUENCY
#define STM32_PCLK3_FREQUENCY        STM32_HCLK_FREQUENCY

/* H4 pin 4: PA9 TX; H4 pin 6: PA10 RX.  Both use AF7. */

#define GPIO_USART1_TX               GPIO_USART1_TX_2
#define GPIO_USART1_RX               GPIO_USART1_RX_2

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
