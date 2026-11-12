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
#include <string.h>
#include <syslog.h>
#include <nuttx/clock.h>
#include <nuttx/semaphore.h>

#include "stm32_lpdma.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Static endpoints remain valid even if hardware fails to stop. */

static uint32_t g_source[32];
static uint32_t g_destination[34];
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

static int lpdmacheck_instance(unsigned int instance, unsigned int count)
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

  cfg.source = (uintptr_t)g_source;
  cfg.destination = (uintptr_t)&g_destination[1];
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

  cfg.nbytes = sizeof(g_source);
  for (i = 0; i < count; i++)
    {
      unsigned int j;

      cfg.width = 1u << (i % 3);
      for (j = 0; j < sizeof(g_source) / sizeof(g_source[0]); j++)
        {
          g_source[j] = 0x13579bdfu ^ (0x01020307u * j) ^ (i << 16);
        }

      memset(g_destination, 0xa5, sizeof(g_destination));
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

      if (memcmp(g_source, &g_destination[1], sizeof(g_source)) != 0 ||
          g_destination[0] != 0xa5a5a5a5u ||
          g_destination[33] != 0xa5a5a5a5u)
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

void stm32_lpdmacheck(void)
{
  if (lpdmacheck_instance(1, 8) == OK)
    {
      lpdmacheck_instance(2, 4);
    }
}
