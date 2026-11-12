/****************************************************************************
 * arch/arm/src/stm32c5/stm32_lpdma.h
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

#ifndef __ARCH_ARM_SRC_STM32C5_STM32_LPDMA_H
#define __ARCH_ARM_SRC_STM32C5_STM32_LPDMA_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LPDMA_SOFTWARE_REQUEST (-1)

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct stm32_lpdma_s;

struct stm32_lpdma_config_s
{
  uintptr_t source;
  uintptr_t destination;
  uint16_t nbytes;
  int16_t request;
  uint8_t width;                 /* Bytes per transfer: 1, 2 or 4 */
  bool srcinc;
  bool dstinc;
};

/* Called in interrupt context.  Result is zero or a negated errno. */

typedef void (*stm32_lpdma_callback_t)(void *arg, int result);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Allocate from the selected instance (1 or 2).  NULL if unavailable. */

struct stm32_lpdma_s *stm32_lpdma_alloc(unsigned int instance);

/* Start a single block with equal source and destination widths.  Keep both
 * endpoints valid until completion or a successful stop.  Hardware requests
 * come from the family request map and must have a single owner.
 */

int stm32_lpdma_start(struct stm32_lpdma_s *chan,
                      const struct stm32_lpdma_config_s *cfg,
                      stm32_lpdma_callback_t callback, void *arg);

/* Cancel without a callback.  Free retains ownership if stop times out. */

int stm32_lpdma_stop(struct stm32_lpdma_s *chan);
int stm32_lpdma_free(struct stm32_lpdma_s *chan);

#endif /* __ARCH_ARM_SRC_STM32C5_STM32_LPDMA_H */
