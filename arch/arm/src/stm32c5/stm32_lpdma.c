/****************************************************************************
 * arch/arm/src/stm32c5/stm32_lpdma.c
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
#include <stddef.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <arch/barriers.h>

#include "arm_internal.h"
#include "stm32_lpdma.h"
#include "hardware/stm32_gpdma.h"
#include "hardware/stm32_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* LPDMA shares these channel registers with GPDMA.  Offsets here are
 * relative to each channel's CLBAR (instance + 0x50 + channel * 0x80).
 */

#define LPDMA_CFCR         0x0c
#define LPDMA_CSR          0x10
#define LPDMA_CCR          0x14
#define LPDMA_CTR1         0x40
#define LPDMA_CTR2         0x44
#define LPDMA_CBR1         0x48
#define LPDMA_CSAR         0x4c
#define LPDMA_CDAR         0x50
#define LPDMA_CLLR         0x7c
#define LPDMA_ERRORS       (GPDMA_CXSR_DTEF | GPDMA_CXSR_USEF | \
                            GPDMA_CXSR_ULEF | GPDMA_CXSR_TOF)
#define LPDMA_TERMINAL     (GPDMA_CXSR_TCF | LPDMA_ERRORS)
#define LPDMA_WAIT_LOOPS   1000
#define LPDMA_CHAN(i, n)   { .base = STM32_DMA##i##_BASE + 0x50 + (n) * 0x80, \
                              .irq = STM32_DMA##i##_IRQ_FIRST + (n), \
                              .instance = (i) }

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct stm32_lpdma_s
{
  uintptr_t base;
  int irq;
  unsigned int instance;
  bool allocated;
  bool available;
  stm32_lpdma_callback_t callback;
  void *arg;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct stm32_lpdma_s g_channels[] =
{
#ifdef CONFIG_STM32_DMA1
  LPDMA_CHAN(1, 0), LPDMA_CHAN(1, 1), LPDMA_CHAN(1, 2), LPDMA_CHAN(1, 3),
  LPDMA_CHAN(1, 4), LPDMA_CHAN(1, 5), LPDMA_CHAN(1, 6), LPDMA_CHAN(1, 7),
#endif
#ifdef CONFIG_STM32_DMA2
  LPDMA_CHAN(2, 0), LPDMA_CHAN(2, 1), LPDMA_CHAN(2, 2), LPDMA_CHAN(2, 3),
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int lpdma_interrupt(int irq, void *context, void *arg)
{
  struct stm32_lpdma_s *chan = arg;
  stm32_lpdma_callback_t callback;
  uint32_t status = getreg32(chan->base + LPDMA_CSR);

  putreg32(status & GPDMA_CXCR_ALLINTS, chan->base + LPDMA_CFCR);
  if ((status & LPDMA_TERMINAL) != 0 && chan->callback != NULL)
    {
      callback = chan->callback;
      chan->callback = NULL;
      UP_DMB();
      callback(chan->arg, (status & LPDMA_ERRORS) != 0 ? -EIO : OK);
    }

  return OK;
}

static int lpdma_stop_locked(struct stm32_lpdma_s *chan)
{
  unsigned int remaining;

  chan->callback = NULL;
  if ((getreg32(chan->base + LPDMA_CCR) & GPDMA_CXCR_EN) != 0)
    {
      /* Suspend before resetting an enabled channel.  Disabling channel
       * interrupt sources here also prevents callbacks during cancellation.
       */

      putreg32(GPDMA_CXCR_SUSP, chan->base + LPDMA_CCR);
      for (remaining = LPDMA_WAIT_LOOPS; remaining > 0; remaining--)
        {
          if ((getreg32(chan->base + LPDMA_CSR) & GPDMA_CXSR_SUSPF) != 0 ||
              (getreg32(chan->base + LPDMA_CCR) & GPDMA_CXCR_EN) == 0)
            {
              break;
            }

          up_udelay(1);
        }

      if (remaining == 0)
        {
          return -ETIMEDOUT;
        }
    }

  putreg32(GPDMA_CXCR_RESET, chan->base + LPDMA_CCR);
  for (remaining = LPDMA_WAIT_LOOPS; remaining > 0; remaining--)
    {
      if ((getreg32(chan->base + LPDMA_CCR) &
          (GPDMA_CXCR_EN | GPDMA_CXCR_SUSP)) == 0)
        {
          putreg32(GPDMA_CXCR_ALLINTS, chan->base + LPDMA_CFCR);
          return OK;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm_dma_initialize(void)
{
  struct stm32_lpdma_s *chan;
  unsigned int i;

  /* The selected LPDMA instance clocks were enabled during RCC startup. */

  for (i = 0; i < sizeof(g_channels) / sizeof(g_channels[0]); i++)
    {
      chan = &g_channels[i];
      if (lpdma_stop_locked(chan) == OK &&
          irq_attach(chan->irq, lpdma_interrupt, chan) == OK)
        {
          chan->available = true;
          up_enable_irq(chan->irq);
        }
    }
}

struct stm32_lpdma_s *stm32_lpdma_alloc(unsigned int instance)
{
  struct stm32_lpdma_s *chan = NULL;
  irqstate_t flags = enter_critical_section();
  unsigned int i;

  for (i = 0; i < sizeof(g_channels) / sizeof(g_channels[0]); i++)
    {
      if (g_channels[i].instance == instance && g_channels[i].available &&
          !g_channels[i].allocated)
        {
          chan = &g_channels[i];
          chan->allocated = true;
          break;
        }
    }

  leave_critical_section(flags);
  return chan;
}

int stm32_lpdma_start(struct stm32_lpdma_s *chan,
                      const struct stm32_lpdma_config_s *cfg,
                      stm32_lpdma_callback_t callback, void *arg)
{
  uint32_t width;
  uint32_t tr1;
  uint32_t request;
  irqstate_t flags;
  int ret = OK;

  if (chan == NULL || cfg == NULL || callback == NULL ||
      (cfg->width != 1 && cfg->width != 2 && cfg->width != 4) ||
      cfg->nbytes == 0 || cfg->request < LPDMA_SOFTWARE_REQUEST ||
      cfg->request > 127 ||
      ((cfg->source | cfg->destination | cfg->nbytes) & (cfg->width - 1)))
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  if (!chan->allocated)
    {
      ret = -EINVAL;
    }
  else if (chan->callback != NULL ||
           (getreg32(chan->base + LPDMA_CCR) & GPDMA_CXCR_EN) != 0)
    {
      ret = -EBUSY;
    }
  else
    {
      /* LPDMA has no FIFO or burst.  Select an equal width for each beat
       * and use BNDT in bytes.  Hardware request direction comes from
       * REQSEL.  The GPDMA DREQ bit is reserved on this LPDMA.
       */

      width = cfg->width >> 1;
      tr1 = (width << GPDMA_CXTR1_SDW_LOG2_SHIFT) |
            (width << GPDMA_CXTR1_DDW_LOG2_SHIFT) |
            (cfg->srcinc ? GPDMA_CXTR1_SINC : 0) |
            (cfg->dstinc ? GPDMA_CXTR1_DINC : 0);
      request = cfg->request == LPDMA_SOFTWARE_REQUEST ? GPDMA_CXTR2_SWREQ :
                                                       cfg->request;
      putreg32(GPDMA_CXCR_ALLINTS, chan->base + LPDMA_CFCR);
      putreg32(0, chan->base + LPDMA_CLLR);
      putreg32(tr1, chan->base + LPDMA_CTR1);
      putreg32(request, chan->base + LPDMA_CTR2);
      putreg32(cfg->nbytes, chan->base + LPDMA_CBR1);
      putreg32(cfg->source, chan->base + LPDMA_CSAR);
      putreg32(cfg->destination, chan->base + LPDMA_CDAR);
      chan->callback = callback;
      chan->arg = arg;
      UP_DMB();
      putreg32(GPDMA_CXCR_EN | GPDMA_CXCR_TCIE | GPDMA_CXCR_DTEIE |
               GPDMA_CXCR_ULEIE | GPDMA_CXCR_USEIE | GPDMA_CXCR_TOIE,
               chan->base + LPDMA_CCR);
    }

  leave_critical_section(flags);
  return ret;
}

int stm32_lpdma_stop(struct stm32_lpdma_s *chan)
{
  irqstate_t flags = enter_critical_section();
  int ret = chan != NULL && chan->allocated ? lpdma_stop_locked(chan) :
                                             -EINVAL;

  leave_critical_section(flags);
  return ret;
}

int stm32_lpdma_free(struct stm32_lpdma_s *chan)
{
  irqstate_t flags = enter_critical_section();
  int ret = chan != NULL && chan->allocated ? lpdma_stop_locked(chan) :
                                             -EINVAL;

  if (ret == OK)
    {
      chan->allocated = false;
    }

  leave_critical_section(flags);
  return ret;
}
