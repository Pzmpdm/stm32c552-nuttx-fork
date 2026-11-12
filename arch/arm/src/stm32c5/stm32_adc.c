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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if STM32_HCLK_FREQUENCY != 48000000
#  error "Initial C5 ADC support requires a 48 MHz HCLK"
#endif

/* HCLK / 4 = 12 MHz.  The post-calibration delay covers four ADC clocks.
 * ao_setup runs inside the upper half's critical section.  Its waits use
 * bounded busy delays and do not depend on the scheduler or system tick.
 */

#define ADC_WAIT_US 1000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct stm32_adc_s
{
  const struct adc_callback_s *cb;
  uint8_t channel;
  bool active;
  bool rxenabled;
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

static void adc_reset(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;

  priv->active = false;
  priv->rxenabled = false;
  adc_hwreset();
  putreg32(ADC_CR_DEEPPWD, STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  modifyreg32(STM32_RCC_AHB2ENR, RCC_AHB2ENR_ADC12EN, 0);
}

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

static int adc_setup(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;
  unsigned int smpr;
  unsigned int shift;
  int ret;

  modifyreg32(STM32_RCC_CCIPR2,
              RCC_CCIPR2_ADCDACSEL_MASK | RCC_CCIPR2_ADCDACPRE_MASK,
              RCC_CCIPR2_ADCDACSEL_HCLK | RCC_CCIPR2_ADCDACPRE(2));
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

  /* Single-ended, 12-bit, right aligned, software-triggered, one shot.
   * No DMA, oversampling, injected group or dual ADC mode.
   */

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR1_OFFSET);
  putreg32(0, STM32_ADC1_BASE + STM32_ADC_CFGR2_OFFSET);
  putreg32(0, STM32_ADC12_COMMON_BASE + STM32_ADC_CCR_OFFSET);
  putreg32(1u << priv->channel, STM32_ADC1_BASE + STM32_ADC_PCSEL_OFFSET);
  smpr = priv->channel < 10 ? STM32_ADC_SMPR1_OFFSET :
                             STM32_ADC_SMPR2_OFFSET;
  shift = (priv->channel % 10) * 3;
  putreg32(ADC_SAMPLE_48CYCLES << shift, STM32_ADC1_BASE + smpr);
  putreg32((uint32_t)priv->channel << ADC_SQR1_SQ1_SHIFT,
           STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);
  putreg32(ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR,
           STM32_ADC1_BASE + STM32_ADC_ISR_OFFSET);
  putreg32(ADC_CR_ADVREGEN | ADC_CR_ADEN,
           STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  if (!adc_wait(STM32_ADC_ISR_OFFSET, ADC_ISR_ADRDY, ADC_ISR_ADRDY))
    {
      ret = -ETIMEDOUT;
      goto errout;
    }

  ret = irq_attach(STM32_IRQ_ADC1, adc_interrupt, dev);
  if (ret < 0)
    {
      goto errout;
    }

  priv->active = true;
  up_enable_irq(STM32_IRQ_ADC1);
  return OK;

errout:
  adc_reset(dev);
  return ret;
}

static void adc_shutdown(struct adc_dev_s *dev)
{
  struct stm32_adc_s *priv = dev->ad_priv;
  uint32_t control;

  putreg32(0, STM32_ADC1_BASE + STM32_ADC_IER_OFFSET);
  up_disable_irq(STM32_IRQ_ADC1);
  irq_detach(STM32_IRQ_ADC1);
  priv->active = false;
  priv->rxenabled = false;

  control = getreg32(STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
  if ((control & ADC_CR_ADSTART) != 0)
    {
      putreg32(ADC_CR_ADVREGEN | ADC_CR_ADSTP,
               STM32_ADC1_BASE + STM32_ADC_CR_OFFSET);
      if (!adc_wait(STM32_ADC_CR_OFFSET, ADC_CR_ADSTART | ADC_CR_ADSTP, 0))
        {
          adc_reset(dev);
          return;
        }
    }

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
  putreg32(enable ? ADC_IER_EOCIE | ADC_IER_OVRIE : 0,
           STM32_ADC1_BASE + STM32_ADC_IER_OFFSET);
}

static int adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg)
{
  struct stm32_adc_s *priv = dev->ad_priv;
  irqstate_t flags;
  int ret = OK;

  switch (cmd)
    {
      case ANIOC_TRIGGER:
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
