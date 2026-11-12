/****************************************************************************
 * arch/arm/src/stm32c5/hardware/stm32c5xx_adc.h
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

#ifndef __ARCH_ARM_SRC_STM32C5_HARDWARE_STM32C5XX_ADC_H
#define __ARCH_ARM_SRC_STM32C5_HARDWARE_STM32C5XX_ADC_H

/* RM0522 ADC1/2 register subset for single-ended regular conversions. */

#define STM32_ADC_ISR_OFFSET       0x0000
#define STM32_ADC_IER_OFFSET       0x0004
#define STM32_ADC_CR_OFFSET        0x0008
#define STM32_ADC_CFGR1_OFFSET     0x000c
#define STM32_ADC_CFGR2_OFFSET     0x0010
#define STM32_ADC_SMPR1_OFFSET     0x0014
#define STM32_ADC_SMPR2_OFFSET     0x0018
#define STM32_ADC_PCSEL_OFFSET     0x001c
#define STM32_ADC_SQR1_OFFSET      0x0030
#define STM32_ADC_SQR2_OFFSET      0x0034
#define STM32_ADC_SQR3_OFFSET      0x0038
#define STM32_ADC_SQR4_OFFSET      0x003c
#define STM32_ADC_DR_OFFSET        0x0040
#define STM32_ADC_CALFACT_OFFSET   0x00c4
#define STM32_ADC_CCR_OFFSET       0x0008

#define ADC_ISR_ADRDY              (1u << 0)
#define ADC_ISR_EOC                (1u << 2)
#define ADC_ISR_EOS                (1u << 3)
#define ADC_ISR_OVR                (1u << 4)
#define ADC_ISR_LDORDY             (1u << 12)
#define ADC_IER_EOCIE              ADC_ISR_EOC
#define ADC_IER_OVRIE              ADC_ISR_OVR
#define ADC_CR_ADEN                (1u << 0)
#define ADC_CR_ADDIS               (1u << 1)
#define ADC_CR_ADSTART             (1u << 2)
#define ADC_CR_ADSTP               (1u << 4)
#define ADC_CR_ADVREGEN            (1u << 28)
#define ADC_CR_DEEPPWD             (1u << 29)
#define ADC_CR_ADCAL               (1u << 31)
#define ADC_CFGR1_DMA_ONESHOT      (1u << 0)
#define ADC_SQR1_SQ1_SHIFT         6
#define ADC_SAMPLE_48CYCLES         5u
#define ADC_DATA_MASK              0x0fff

#endif /* __ARCH_ARM_SRC_STM32C5_HARDWARE_STM32C5XX_ADC_H */
