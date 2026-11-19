/****************************************************************************
 * arch/arm/src/stm32c5/stm32_adc.c
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
#include <stdbool.h>
#include <stdint.h>
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <arch/board/board.h>

#include "arm_internal.h"
#include "stm32_adc.h"
#include "hardware/stm32c5xx_adc.h"
#include "hardware/stm32_rcc.h"

#ifdef CONFIG_STM32C5_ADC1_LPDMA
#  include <nuttx/clock.h>
#  include <nuttx/mutex.h>
#  include <nuttx/semaphore.h>
#  include "stm32_lpdma.h"
#  include "hardware/stm32_dmasigmap.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if STM32_HCLK_FREQUENCY == 48000000
#  define ADC_KERNEL_PRE  2 /* HCLK / 4 = 12 MHz */
#elif STM32_HCLK_FREQUENCY == 144000000
#  define ADC_KERNEL_PRE  4 /* HCLK / 16 = 9 MHz; PRE(n) divides by 2^n */
#else
#  error "C5 ADC kernel divider is only set for 48 MHz and 144 MHz HCLK"
#endif

/* The post-calibration delay covers four ADC clocks.
 * ao_setup runs inside the upper half's critical section.  Its waits use
 * bounded busy delays and do not depend on the scheduler or system tick.
 */

#define ADC_WAIT_US 1000

#ifdef CONFIG_STM32C5_ADC1_LPDMA
#  define ADC_DMA_SAMPLES 4
#  if CONFIG_ADC_FIFOSIZE <= ADC_DMA_SAMPLES
#    error "ADC FIFO must hold an entire DMA block"
#  endif
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct stm32_adc_s
{
  const struct adc_callback_s *cb;
  uint8_t channel;
  bool active;
  bool rxenabled;
#ifdef CONFIG_STM32C5_ADC1_LPDMA
  struct stm32_lpdma_s *dma;
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int adc_bind(struct adc_dev_s *dev,
                    const struct adc_callback_s *callback);
static void adc_reset(struct adc_dev_s *dev);
static int adc_setup(struct adc_dev_s *dev);
static void adc_shutdown(struct adc_dev_s *dev);
static void adc_rxint(struct adc_dev_s *dev, bool enable);
static int adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct adc_ops_s g_adcops =
{
  .ao_bind = adc_bind,
  .ao_reset = adc_reset,
  .ao_setup = adc_setup,
  .ao_shutdown = adc_shutdown,
  .ao_rxint = adc_rxint,
  .ao_ioctl = adc_ioctl,
};

#ifdef CONFIG_STM32C5_ADC1_LPDMA
static uint32_t g_dma_samples[ADC_DMA_SAMPLES];
static sem_t g_dma_done = SEM_INITIALIZER(0);
static mutex_t g_dma_lock = NXMUTEX_INITIALIZER;
static volatile int g_dma_result;
#endif

static struct stm32_adc_s g_adcpriv;
static struct adc_dev_s g_adcdev =
{
  .ad_ops = &g_adcops,
  .ad_priv = &g_adcpriv,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static bool adc_wait(unsigned int offset, uint32_t mask, uint32_t expected)
{
  unsigned int remaining;

  for (remaining = ADC_WAIT_US; remaining > 0; remaining--)
    {
      if ((getreg32(STM32_ADC1_BASE + offset) & mask) == expected)
        {
          return true;
        }

      up_udelay(1);
    }

  return false;
}

static void adc_hwreset(void)
{
  /* Only ADC1 is exposed.  ADC12 reset and the ADCDAC clock are shared;
   * ADC2/DAC integration must add ownership before reusing this sequence.
   */

  modifyreg32(STM32_RCC_AHB2ENR, 0, RCC_AHB2ENR_ADC12EN);
  (void)getreg32(STM32_RCC_AHB2ENR);
  modifyreg32(STM32_RCC_AHB2RSTR, 0, RCC_AHB2RSTR_ADC12RST);
  modifyreg32(STM32_RCC_AHB2RSTR, RCC_AHB2RSTR_ADC12RST, 0);
}

static int adc_bind(struct adc_dev_s *dev,
                    const struct adc_callback_s *callback)
{
  struct stm32_adc_s *priv = dev->ad_priv;

  priv->cb = callback;
  return OK;
}

static bool adc_stop_conversion(void)
{
  if ((getreg32(STM32_ADC1_BASE + STM32_ADC_CR_OFFSET) &
       ADC_CR_ADSTART) == 0)
    {
      return true;
    }

  putreg32(ADC_CR_ADVREGEN | ADC_CR_ADSTP,
           STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  return adc_wait(STM32_ADC_CR_OFFSET, ADC_CR_ADSTART | ADC_CR_ADSTP, 0);
}

static void adc_reset(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;

  priv->active = false;
  priv->rxenabled = false;
  adc_hwreset();
  putreg32(ADC_CR_DEEPPWD, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  modifyreg32(STM32_RCC_AHB2ENR, RCC_AHB2ENR_ADC12EN, 0);
#ifdef CONFIG_STM32C5_ADC1_LPDMA
  if (priv->dma != NULL && stm32_lpdma_free(priv->dma) == OK)
    {
      priv->dma = NULL;
    }
#endif
}

#ifndef CONFIG_STM32C5_ADC1_LPDMA
static int adc_interrupt(int irq, void *context, void *arg)
{
  struct adc_dev_s *dev = arg;
  struct stm32_adc_s *priv = dev->ad_priv;
  uint32_t status = getreg32(STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  uint32_t sample;

  if ((status & ADC_ISR_OVR) != 0 && priv->cb != NULL &&
      priv->cb->au_reset != NULL)
    {
      priv->cb->au_reset(dev);
    }

  if ((status & ADC_ISR_EOC) != 0)
    {
      /* Reading DR acknowledges EOC.  Preserve the 12-bit unsigned code. */

      sample = getreg32(STM32_ADC1_BASE + STM32_ADC_DR_OFFSET);
      if (priv->rxenabled && priv->cb != NULL)
        {
          priv->cb->au_receive(dev, priv->channel, sample & ADC_DATA_MASK);
        }
    }

  putreg32(status & (ADC_ISR_EOS | ADC_ISR_OVR),
           STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  return OK;
}

#endif

#ifdef CONFIG_STM32C5_ADC1_LPDMA
static void adc_dma_complete(void *arg, int result)
{
  g_dma_result = result;
  nxsem_post(&g_dma_done);
}

static int adc_dma_trigger(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;
  struct stm32_lpdma_config_s cfg =
  {
    .source = STM32_ADC1_BASE + STM32_ADC_DR_OFFSET,
    .destination = (uintptr_t)g_dma_samples,
    .nbytes = sizeof(g_dma_samples),
    .request = LPDMA_REQ_ADC1,
    .width = sizeof(g_dma_samples[0]),
    .dstinc = true,
  };

  uint8_t channels[ADC_DMA_SAMPLES];
  irqstate_t flags;
  unsigned int i;
  int ret;

  ret = nxmutex_lock(&g_dma_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (!priv->active || !priv->rxenabled || priv->cb == NULL ||
      priv->cb->au_receive_batch == NULL)
    {
      ret = -EIO;
      goto error;
    }

  if ((getreg32(STM32_ADC1_BASE + STM32_ADC_CR_OFFSET) &
       ADC_CR_ADSTART) != 0)
    {
      ret = -EBUSY;
      goto error;
    }

  /* Arm DMA before generating the first ADC request.  Wait in task context
   * for one four-rank sequence, so a missing request cannot block forever.
   */

  putreg32(ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR,
           STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  nxsem_reset(&g_dma_done, 0);
  ret = stm32_lpdma_start(priv->dma, &cfg, adc_dma_complete, NULL);
  if (ret < 0)
    {
      goto error;
    }

  putreg32(ADC_CFGR1_DMA_ONESHOT, STM32_ADC1_BASE + STM32_ADC_CFGR1_OFFSET);
  putreg32(ADC_CR_ADVREGEN | ADC_CR_ADSTART,
           STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  ret = nxsem_tickwait_uninterruptible(&g_dma_done, MSEC2TICK(100));
  if (ret == OK)
    {
      ret = g_dma_result;
    }

  if (ret == OK && !adc_wait(STM32_ADC_CR_OFFSET, ADC_CR_ADSTART, 0))
    {
      ret = -ETIMEDOUT;
    }

  if (ret == OK && (getreg32(STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET) &
                    ADC_ISR_OVR) != 0)
    {
      ret = -EIO;
    }

  if (ret < 0)
    {
      if (!adc_stop_conversion() || stm32_lpdma_stop(priv->dma) < 0)
        {
          adc_reset(dev);
        }
      else
        {
          putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR1_OFFSET);
          putreg32(ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR,
                   STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
        }

      goto error;
    }

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR1_OFFSET);
  putreg32(ADC_ISR_EOS, STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  for (i = 0; i < ADC_DMA_SAMPLES; i++)
    {
      channels[i] = priv->channel;
    }

  flags = enter_critical_section();
  ret = priv->cb->au_receive_batch(dev, channels, g_dma_samples,
                                   ADC_DMA_SAMPLES);
  leave_critical_section(flags);
  if (ret == OK)
    {
      nxmutex_unlock(&g_dma_lock);
      return OK;
    }

error:
  if (priv->cb != NULL && priv->cb->au_reset != NULL)
    {
      flags = enter_critical_section();
      priv->cb->au_reset(dev);
      leave_critical_section(flags);
    }

  nxmutex_unlock(&g_dma_lock);
  return ret;
}
#endif

static int adc_setup(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;
  unsigned int smpr;
  unsigned int shift;
  uint32_t sequence;
  int ret;

#ifdef CONFIG_STM32C5_ADC1_LPDMA
  unsigned int rank;

  if (priv->dma != NULL)
    {
      ret = stm32_lpdma_free(priv->dma);
      if (ret < 0)
        {
          return ret;
        }

      priv->dma = NULL;
    }
#endif

  modifyreg32(STM32_RCC_CCIPR2,
              RCC_CCIPR2_ADCDACSEL_MASK | RCC_CCIPR2_ADCDACPRE_MASK,
              RCC_CCIPR2_ADCDACSEL_HCLK | RCC_CCIPR2_ADCDACPRE(ADC_KERNEL_PRE));
  adc_hwreset();

  /* Exit deep power down and wait for the regulator before calibration. */

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  putreg32(ADC_CR_ADVREGEN, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  if (!adc_wait(STM32_ADC_ISR_OFFSET, ADC_ISR_LDORDY, ADC_ISR_LDORDY))
    {
      ret = -ETIMEDOUT;
      goto errout;
    }

  putreg32(ADC_CR_ADVREGEN | ADC_CR_ADCAL,
           STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  if (!adc_wait(STM32_ADC_CR_OFFSET, ADC_CR_ADCAL, 0))
    {
      ret = -ETIMEDOUT;
      goto errout;
    }

  up_udelay(4);

  /* Single-ended, 12-bit, right aligned, software-triggered, one shot. */

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR1_OFFSET);
#ifdef CONFIG_STM32C5_ADC1_LPDMA
  sequence = ADC_DMA_SAMPLES - 1;
  for (rank = 0; rank < ADC_DMA_SAMPLES; rank++)
    {
      sequence |= (uint32_t)priv->channel << (ADC_SQR1_SQ1_SHIFT + 6 * rank);
    }
#else
  sequence = (uint32_t)priv->channel << ADC_SQR1_SQ1_SHIFT;
#endif
  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR2_OFFSET);
  putreg32(0, STM32_ADC12_COMMON_BASE + STM32_ADC_CCR_OFFSET);
  putreg32(1u << priv->channel, STM32_ADC1_BASE + STM32_ADC_PCSEL_OFFSET);
  smpr = priv->channel < 10 ? STM32_ADC_SMPR1_OFFSET :
                             STM32_ADC_SMPR2_OFFSET;
  shift = (priv->channel % 10) * 3;
  putreg32(ADC_SAMPLE_48CYCLES << shift, STM32_ADC1_BASE + smpr);
  putreg32(sequence, STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);
  putreg32(ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR,
           STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  putreg32(ADC_CR_ADVREGEN | ADC_CR_ADEN,
           STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  if (!adc_wait(STM32_ADC_ISR_OFFSET, ADC_ISR_ADRDY, ADC_ISR_ADRDY))
    {
      ret = -ETIMEDOUT;
      goto errout;
    }

#ifdef CONFIG_STM32C5_ADC1_LPDMA
  priv->dma = stm32_lpdma_alloc(1);
  if (priv->dma == NULL)
    {
      ret = -EBUSY;
      goto errout;
    }
#else
  ret = irq_attach(STM32_IRQ_ADC1, adc_interrupt, dev);
  if (ret < 0)
    {
      goto errout;
    }

  up_enable_irq(STM32_IRQ_ADC1);
#endif
  priv->active = true;
  return OK;

errout:
  adc_reset(dev);
  return ret;
}

static void adc_shutdown(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;

#ifdef CONFIG_STM32C5_ADC1_LPDMA
  /* Error recovery may already have reset ADC1 and gated its clock. */

  if (!priv->active)
    {
      return;
    }
#endif

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_IER_OFFSET);
#ifndef CONFIG_STM32C5_ADC1_LPDMA
  up_disable_irq(STM32_IRQ_ADC1);
  irq_detach(STM32_IRQ_ADC1);
#endif
  priv->active = false;
  priv->rxenabled = false;

  if (!adc_stop_conversion())
    {
      adc_reset(dev);
      return;
    }

#ifdef CONFIG_STM32C5_ADC1_LPDMA
  if (priv->dma != NULL && stm32_lpdma_free(priv->dma) == OK)
    {
      priv->dma = NULL;
    }
#endif

  if ((getreg32(STM32_ADC1_BASE + STM32_ADC_CR_OFFSET) & ADC_CR_ADEN) != 0)
    {
      putreg32(ADC_CR_ADVREGEN | ADC_CR_ADDIS,
               STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
      if (!adc_wait(STM32_ADC_CR_OFFSET, ADC_CR_ADEN | ADC_CR_ADDIS, 0))
        {
          adc_reset(dev);
          return;
        }
    }

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  putreg32(ADC_CR_DEEPPWD, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  modifyreg32(STM32_RCC_AHB2ENR, RCC_AHB2ENR_ADC12EN, 0);
}

static void adc_rxint(struct adc_dev_s *dev, bool enable)
{
  struct stm32_adc_s *priv = dev->ad_priv;

  priv->rxenabled = enable;
#ifndef CONFIG_STM32C5_ADC1_LPDMA
  putreg32(enable ? ADC_IER_EOCIE | ADC_IER_OVRIE : 0,
           STM32_ADC1_BASE + STM32_ADC_IER_OFFSET);
#endif
}

static int adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg)
{
#ifndef CONFIG_STM32C5_ADC1_LPDMA
  struct stm32_adc_s *priv = dev->ad_priv;
  irqstate_t flags;
  int ret = OK;
#endif

  switch (cmd)
    {
      case ANIOC_TRIGGER:
#ifdef CONFIG_STM32C5_ADC1_LPDMA
        return adc_dma_trigger(dev);
#else
        flags = enter_critical_section();
        if (!priv->active || !priv->rxenabled)
          {
            ret = -EIO;
          }
        else if ((getreg32(STM32_ADC1_BASE + STM32_ADC_CR_OFFSET) &
                  ADC_CR_ADSTART) != 0 ||
                 (getreg32(STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET) &
                  ADC_ISR_EOC) != 0)
          {
            ret = -EBUSY;
          }
        else
          {
            putreg32(ADC_CR_ADVREGEN | ADC_CR_ADSTART,
                     STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
          }

        leave_critical_section(flags);
        return ret;
#endif

      case ANIOC_GET_NCHANNELS:
        return 1;

      default:
        return -ENOTTY;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct adc_dev_s *stm32_adc_initialize(int intf, const uint8_t *chanlist,
                                     int nchannels)
{
  if (intf != 1 || chanlist == NULL || nchannels != 1 || chanlist[0] > 11 ||
      g_adcpriv.active)
    {
      return NULL;
    }

  g_adcpriv.channel = chanlist[0];
  return &g_adcdev;
}
