==========
ST STM32C5
==========

The STM32C5 family consists of mainstream microcontrollers based on the Arm
Cortex-M33 core.  NuttX supports STM32C562 devices and the STM32C552VE.

Supported MCUs
==============

=======================  =======  ==============================================
MCU                      Support  Notes
=======================  =======  ==============================================
STM32C562CE/KE/ME/RE/VE   Yes      512 KiB Flash, 128 KiB SRAM
STM32C552VE              Yes      512 KiB Flash, 64 KiB SRAM1 used by the port
=======================  =======  ==============================================

Peripheral Support
==================

The following list indicates peripherals supported in NuttX:

==========  =======
Peripheral  Support
==========  =======
RCC         Yes
GPIO        Yes
EXTI        Yes
USART/UART  Yes
LPUART      Yes

ADC         No
AES         No
COMP        No
CORDIC      No
CRC         No
CRS         No
DAC         No
DBG         No
FDCAN       No
FLASH       No
HASH        No
I2C         No
I3C         No
ICACHE      No
IWDG        No
LPDMA       No
LPTIM       No
PWR         No
RAMCFG      No
RNG         No
RTC         No
SBS         No
SPI/I2S     No
TAMP        No
TIM         No
USB_FS      No
WWDG        No
==========  =======

References
==========

[RM0522] STMicroelectronics, STM32C5 series Arm-based 32-bit MCUs.

Supported Boards
================

.. toctree::
   :glob:
   :maxdepth: 1

   boards/*/*
