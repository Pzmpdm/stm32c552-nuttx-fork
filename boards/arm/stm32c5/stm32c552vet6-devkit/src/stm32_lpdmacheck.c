/****************************************************************************
 * boards/arm/stm32c5/stm32c552vet6-devkit/src/stm32_lpdmacheck.c
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
#include <stdint.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <nuttx/clock.h>
#include <nuttx/kmalloc.h>
#include <nuttx/semaphore.h>

#include "stm32_lpdma.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Static endpoints remain valid even if hardware fails to stop.
 * The SRAM2 heap build allocates these instead, then frees them.
 */

#ifndef CONFIG_STM32_SRAM2_HEAP
static uint32_t g_source[32];
static uint32_t g_destination[34];
#endif
static sem_t g_completion = SEM_INITIALIZER(0);
static volatile int g_result;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void lpdmacheck_done(void *arg, int result)
{
  g_result = result;
  nxsem_post(&g_completion);
}

static int lpdmacheck_copy(struct stm32_lpdma_s *chan,
                           struct stm32_lpdma_config_s *cfg)
{
  int ret;

  nxsem_reset(&g_completion, 0);
  ret = stm32_lpdma_start(chan, cfg, lpdmacheck_done, NULL);
  if (ret < 0)
    {
      return ret;
    }

  ret = nxsem_tickwait_uninterruptible(&g_completion, MSEC2TICK(100));
  if (ret < 0)
    {
      int stop = stm32_lpdma_stop(chan);

      return stop < 0 ? stop : ret;
    }

  return g_result;
}

static int lpdmacheck_instance(unsigned int instance, unsigned int count,
                               uint32_t *source, uint32_t *destination,
                               bool *stopped)
{
  struct stm32_lpdma_s *channels[8];
  struct stm32_lpdma_s *extra;
  struct stm32_lpdma_config_s cfg;
  unsigned int n;
  unsigned int i;
  int ret = OK;

  for (n = 0; n < count; n++)
    {
      channels[n] = stm32_lpdma_alloc(instance);
      if (channels[n] == NULL)
        {
          ret = -ENOSPC;
          goto out;
        }
    }

  extra = stm32_lpdma_alloc(instance);
  if (extra != NULL)
    {
      stm32_lpdma_free(extra);
      ret = -EOVERFLOW;
      goto out;
    }

  cfg.source = (uintptr_t)source;
  cfg.destination = (uintptr_t)&destination[1];
  cfg.request = LPDMA_SOFTWARE_REQUEST;
  cfg.srcinc = true;
  cfg.dstinc = true;
  cfg.width = 4;
  cfg.nbytes = 0;
  if (stm32_lpdma_start(channels[0], &cfg, lpdmacheck_done, NULL) != -EINVAL)
    {
      ret = -EIO;
      goto out;
    }

  cfg.nbytes = 32 * sizeof(uint32_t);
  for (i = 0; i < count; i++)
    {
      unsigned int j;

      cfg.width = 1u << (i % 3);
      for (j = 0; j < 32; j++)
        {
          source[j] = 0x13579bdfu ^ (0x01020307u * j) ^ (i << 16);
        }

      memset(destination, 0xa5, 34 * sizeof(uint32_t));
      ret = lpdmacheck_copy(channels[i], &cfg);
      if (ret == -EIO || ret == -ETIMEDOUT)
        {
          syslog(LOG_INFO, "LPDMA%u channel %u retry: %d\n",
                 instance, i, ret);
          ret = lpdmacheck_copy(channels[i], &cfg);
        }

      if (ret < 0)
        {
          goto out;
        }

      if (memcmp(source, &destination[1], 32 * sizeof(uint32_t)) != 0 ||
          destination[0] != 0xa5a5a5a5u ||
          destination[33] != 0xa5a5a5a5u)
        {
          ret = -EIO;
          goto out;
        }
    }

out:
  while (n > 0)
    {
      int stop = stm32_lpdma_free(channels[--n]);

      if (stop < 0)
        {
          *stopped = false;
          ret = stop;
        }
    }

  if (ret == OK)
    {
      syslog(LOG_INFO, "LPDMA%u memory test: %u channels OK\n",
             instance, count);
    }
  else
    {
      syslog(LOG_ERR, "LPDMA%u memory test failed: %d\n", instance, ret);
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_STM32_SRAM2_HEAP
static uint32_t *lpdma_take_sram2(void)
{
  uint32_t *held[16];
  uint32_t *block = NULL;
  unsigned int n = 0;
  unsigned int i;

  while (n < 16 && block == NULL)
    {
      uint32_t *chunk = kmm_malloc(4096);

      if (chunk == NULL)
        {
          break;
        }

      if ((uintptr_t)chunk >= 0x20010000 &&
          (uintptr_t)chunk + 4096 <= 0x20020000)
        {
          block = chunk;
        }
      else
        {
          held[n++] = chunk;
        }
    }

  for (i = 0; i < n; i++)
    {
      kmm_free(held[i]);
    }

  return block;
}
#endif

void stm32_lpdmacheck(void)
{
  bool stopped = true;
#ifdef CONFIG_STM32_SRAM2_HEAP
  struct mallinfo before;
  struct mallinfo after;
  uint32_t *block;
  int ret;

  before = mallinfo();
  block = lpdma_take_sram2();
  if (block == NULL)
    {
      syslog(LOG_ERR, "LPDMA SRAM2 alloc failed\n");
      return;
    }

  ret = lpdmacheck_instance(1, 8, block, block + 32, &stopped);
  if (ret == OK)
    {
      ret = lpdmacheck_instance(2, 4, block, block + 32, &stopped);
    }

  if (ret == OK)
    {
      syslog(LOG_INFO, "LPDMA SRAM2 %08lx %08lx\n",
             (unsigned long)(uintptr_t)block,
             (unsigned long)(uintptr_t)(block + 33));
    }

  if (stopped)
    {
      kmm_free(block);
    }

  if (ret == OK)
    {
      after = mallinfo();
      syslog(LOG_INFO, "LPDMA SRAM2 released %d %d\n",
             before.fordblks, after.fordblks);
    }
#else
  if (lpdmacheck_instance(1, 8, g_source, g_destination, &stopped) == OK)
    {
      lpdmacheck_instance(2, 4, g_source, g_destination, &stopped);
    }
#endif
}
