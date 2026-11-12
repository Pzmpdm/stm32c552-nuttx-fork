# ##############################################################################
# boards/arm/stm32c5/stm32c552vet6-devkit/src/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

target_sources(board PRIVATE stm32_boot.c stm32_boardgpio.c)

if(CONFIG_STM32_SPI1)
  target_sources(board PRIVATE stm32_spi.c)
endif()

if(CONFIG_STM32_ADC1 AND CONFIG_ADC)
  target_sources(board PRIVATE stm32_adc.c)
endif()

if(CONFIG_STM32C552VET6_DEVKIT_LPDMA_SELFTEST)
  target_sources(board PRIVATE stm32_lpdmacheck.c)
endif()

set_property(GLOBAL PROPERTY LD_SCRIPT "${NUTTX_BOARD_DIR}/scripts/flash.ld")
