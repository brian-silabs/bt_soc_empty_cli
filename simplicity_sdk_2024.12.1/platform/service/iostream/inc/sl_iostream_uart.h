/***************************************************************************//**
 * @file
 * @brief IO Stream UART Component.
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SL_IOSTREAM_UART_TYPE_H
#define SL_IOSTREAM_UART_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "em_device.h"
#include "sl_status.h"
#include "sl_iostream.h"
#include "dmadrv.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if (defined(SL_CATALOG_KERNEL_PRESENT))
#include "cmsis_os2.h"
#include "sl_cmsis_os2_common.h"
#endif

/***************************************************************************//**
 * @addtogroup iostream
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup iostream_uart I/O Stream UART
 * @brief I/O Stream UART
 * @details
 * ## Overview
 *
 *   UART layer provides a set of standard APIs that can be used with all type of
 *   UART interface.
 *
 * ## Initialization
 *
 *   Each UART stream type provides its initalization with parameters specific to them.
 * @note  Each UART stream requires a dedicated (L)DMA channel through DMADRV.
 *
 * ## Configuration
 *
 * ### RX Buffer Size
 *
 *    The `SL_IOSTREAM_<Peripheral>_<Instance>_RX_BUFFER_SIZE` parameter should
 *    be chosen based on the time it takes for the MCU to consume the RX buffer,
 *    otherwise flow control will have to be asserted to avoid data loss.
 *    If hardware flow control is unavailable, data will be dropped.
 *
 *    Let's assume that the MCU is able to call the IOStream Read function with
 *    a maximum delay of 1ms. We have:
 *    ```
 *    (1) MaximumDelay = RXBufferSize * TimePerDatum
 *
 *    Where TimePerDatum is defined by (#startBits + #dataBits + #parityBits + #stopBits)/BaudRate.
 *    With default settings, we have 1 start & stop bit, 8 data bits and no parity.
 *
 *    Rewriting (1), we have:
 *
 *    (2) MaximumDelay = RXBufferSize * 10/BaudRate <=> RXBufferSize = MaximumDelay*Baudrate/10
 *
 *    With a baudrate of 921,600 and a maximum consumption delay of 1ms, we have
 *    a recommended RXBufferSize of:
 *
 *    (3) RXBufferSize = 0.001*921600/10 = 93Bytes
 *    ```
 *    This should ensure that flow control does not have to be asserted, slowing
 *    down the bus and if unavailable, that no data will be dropped.
 *
 * ### Baudrate
 *
 *    IOStream UART leverages the DMA in order consume data from the UART peripheral.
 *    When user reads data from IOStream, the internal reception buffer gets more
 *    room for receiving new data. However, this update process pauses the DMA,
 *    meaning that any new data coming from the bus will remain in the UART peripheral's
 *    FIFO. If too much data is received before the update can complete, hardware
 *    flow control will have be enforces, or data will be dropped if unavailable.
 *
 *    Measurements show that the DMA update executes in ~23.7us with -O3 optimization
 *    (-Os gives slightly worst performance of ~32.7us). With these, the following
 *    equations can be followed to understand the maximum baudrate supported by
 *    the current design, when no flow control is available:
 *    ```
 *    The number of bytes received during the ring buffer update is given by:
 *
 *    (1) UpdateTime = FIFOSize * TimePerDatum
 *
 *    Where TimePerDatum is defined by (#startBits + #dataBits + #parityBits + #stopBits)/BaudRate.
 *    With default settings, we have 1 start & stop bit, 8 data bits and no parity.
 *    Rewriting (1), we have:
 *
 *    (2) UpdateTime = FIFOSize * 10/Baudrate
 *
 *    For USART, we have a FIFO of size 2. This means that for an update time of
 *    23.7us, we have a max baudrate of:
 *
 *    (3) 23.7 = 2 * 10/Baudrate <=> Baudrate = 2*10/23.7 = 843,881Baud.
 *    ```
 *    The maximum "real" baudrate is then 460,800 Baud, otherwise hardware flow
 *    control will have to be asserted, or data will be dropped.
 *
 *    To achieve higher throughput without flow control assertion, users can use
 *    the EUSART peripheral, which boasts a 16Bytes FIFO, allow for baudrate of
 *    upwards of 921600 without data loss with no hardware flow control.
 *
 * @{
 ******************************************************************************/

// -----------------------------------------------------------------------------
// Data Types

#define uartFlowControlNone  0                     ///< uart flow control none
#define uartFlowControlSoftware    0xFFFF          ///< uart flow control software
#define UARTXON     0x11                           ///< uartx on
#define UARTXOFF    0x13                           ///< uartx off

/// @brief I/O Stream UART stream object
typedef struct {
  sl_iostream_t stream;                                               ///< stream
  sl_status_t (*deinit)(void *stream);                                ///< uart deinit
  void (*set_auto_cr_lf)(void *context, bool on);                     ///< set_auto_cr_lf
  bool (*get_auto_cr_lf)(void *context);                              ///< get_auto_cr_lf
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  void (*set_rx_energy_mode_restriction)(void *context, bool on);     ///< set_rx_energy_mode_restriction. Available only when Power Manager present.
  bool (*get_rx_energy_mode_restriction)(void *context);              ///< get_rx_energy_mode_restriction. Available only when Power Manager present.
#if !defined(SL_CATALOG_KERNEL_PRESENT) || defined(DOXYGEN)
  sl_power_manager_on_isr_exit_t (*sleep_on_isr_exit)(void *context); ///< sleep_on_isr_exit. Available only when Power Manager present and kernel not present.
#endif
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
  void (*set_read_block)(void *context, bool on);                     ///< set_read_block. Available only when kernel present.
  bool (*get_read_block)(void *context);                              ///< get_read_block. Available only when kernel present.
#endif
} sl_iostream_uart_t;

/// @brief I/O Stream (L)DMA Config
typedef struct {
  DMADRV_PeripheralSignal_t peripheral_signal;  ///< Peripheral signal to trigger a DMA transfer on
  uint8_t *src;                                 ///< Pointer to IO Stream peripheral data register
} sl_iostream_dma_config_t;

/// @brief I/O Steam (L)DMA Context
typedef struct {
  sl_iostream_dma_config_t cfg;                       ///< DMA Configuration
  uint8_t channel;                                    ///< DMA Channel
  #if defined(EMDRV_DMADRV_LDMA)
  LDMA_Descriptor_t rx_resume_desc;                   ///< DMA reception resume descriptor
  LDMA_Descriptor_t wrap_desc;                        ///< DMA wrap descriptor
  #elif defined(EMDRV_DMADRV_LDMA_S3)
  sl_hal_ldma_descriptor_t rx_resume_desc;            ///< DMA reception resume descriptor
  sl_hal_ldma_descriptor_t wrap_desc;                 ///< DMA wrap descriptor
  #endif
} sl_iostream_dma_context_t;

/// @brief I/O Stream UART config
typedef struct {
  sl_iostream_dma_config_t dma_cfg;                     ///< DMA Config
  IRQn_Type rx_irq_number;                              ///< rx_irq_number
  IRQn_Type tx_irq_number;                              ///< tx_irq_number
  uint8_t *rx_buffer;                                   ///< UART Rx Buffer
  size_t rx_buffer_length;                              ///< UART Rx Buffer length
  bool lf_to_crlf;                                      ///< lf_to_crlf
  bool rx_when_sleeping;                                ///< rx_when_sleeping
  bool sw_flow_control;                                 ///< sw_flow_control
} sl_iostream_uart_config_t;

/// @brief I/O Stream UART context
typedef struct {
  sl_iostream_dma_context_t dma;            ///< DMA Context
  uint8_t *rx_buffer;                       ///< UART Rx Buffer
  size_t rx_buffer_len;                     ///< UART Rx Buffer length
  uint8_t *rx_read_ptr;                     ///< Address of the next byte to be read
  sl_status_t (*tx)(void *context, char c); ///< Tx function pointer
  void (*tx_completed)(void *context, bool enable); ///< Pointer to a function handling the Tx Completed event
  sl_status_t (*deinit)(void *context);     ///< DeInit function pointer
  bool lf_to_crlf;                          ///< lf_to_crlf
  bool sw_flow_control;                     ///< software flow control
  uint8_t *ctrl_char_scan_ptr;              ///< Pointer to where the last control character scan ended
  volatile bool xon;                        ///< Transmitter enabled
  bool remote_xon;                          ///< Remote Transmitter enabled
  IRQn_Type rx_irq_number;                  ///< Receive IRQ Number
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  IRQn_Type tx_irq_number;                  ///< Transmit IRQ Number
  volatile bool tx_idle;                    ///< tx_idle. Available only when Power Manager present.
  bool em_req_added;                        ///< em_req_added. Available only when Power Manager present.
  sl_power_manager_em_t rx_em;              ///< rx_em. Available only when Power Manager present.
  sl_power_manager_em_t tx_em;              ///< tx_em. Available only when Power Manager present.
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
  bool block;                                ///< block. Available only when kernel present.
  osMutexId_t read_lock;                     ///< read_lock. Available only when kernel present.
  __ALIGNED(4) uint8_t read_lock_cb[osMutexCbSize];         ///< read_lock control block. Available only when kernel present.
  osEventFlagsId_t rx_data_flag;              ///< rx_data_flag. Available only when kernel present.
  __ALIGNED(4) uint8_t rx_data_flag_cb[osEventFlagsCbSize];   ///< rx_data_flag control block. Available only when kernel present.
  osMutexId_t write_lock;                    ///< write_lock. Available only when kernel present.
  __ALIGNED(4) uint8_t write_lock_cb[osMutexCbSize];        ///< write_lock control block. Available only when kernel present.
#elif defined(SL_CATALOG_POWER_MANAGER_PRESENT) || defined(DOXYGEN)
  sl_power_manager_on_isr_exit_t sleep;      ///< sleep. Available only when kernel not present and Power Manager present.
#endif
} sl_iostream_uart_context_t;

// -----------------------------------------------------------------------------
// Prototypes

/***************************************************************************//**
 * UART Stream De-init.
 *
 * @param[in] iostream_uart  UART stream object.
 *
 * @return Status result
 ******************************************************************************/
__STATIC_INLINE sl_status_t sl_iostream_uart_deinit(sl_iostream_uart_t *iostream_uart)
{
  #if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  iostream_uart->set_rx_energy_mode_restriction(iostream_uart->stream.context, false);
  #endif
  return iostream_uart->deinit(iostream_uart);
}

/***************************************************************************//**
 * Configure Automatic line conversion.
 *
 * @param[in] iostream_uart  UART stream object.
 *
 * @param[in] on  If true, automatic LF to CRLF conversion will be enabled.
 ******************************************************************************/
__STATIC_INLINE void sl_iostream_uart_set_auto_cr_lf(sl_iostream_uart_t *iostream_uart,
                                                     bool on)
{
  iostream_uart->set_auto_cr_lf(iostream_uart->stream.context, on);
}

/***************************************************************************//**
 * Get Automatic line conversion.
 *
 * @param[in] iostream_uart   UART stream object.
 *
 * @return Auto-conversion mode.
 ******************************************************************************/
__STATIC_INLINE bool sl_iostream_uart_get_auto_cr_lf(sl_iostream_uart_t *iostream_uart)
{
  return iostream_uart->get_auto_cr_lf(iostream_uart->stream.context);
}

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
/***************************************************************************//**
 * Set next byte detect IRQ.
 *
 * @param[in] iostream_uart  UART stream object.
 ******************************************************************************/
void sl_iostream_uart_prepare_for_sleep(sl_iostream_uart_t *iostream_uart);

/***************************************************************************//**
 * Restore the UART for normal operation after wakeup.
 *
 * @param[in] iostream_uart  UART stream object.
 ******************************************************************************/
void sl_iostream_uart_wakeup(sl_iostream_uart_t *iostream_uart);

/***************************************************************************//**
 * Add or remove energy mode restriction to enable/disable reception when the
 * system goes to sleep.
 *
 * @param[in] iostream_uart  UART context.
 *
 * @param[in] on  If true, will be able to receive data when sleeping. i.e it
 *                affects the lowest power level that the system can go.
 *                Otherwise, it might not be possible to receive data when sleeping.
 ******************************************************************************/
__STATIC_INLINE void sl_iostream_uart_set_rx_energy_mode_restriction(sl_iostream_uart_t *iostream_uart,
                                                                     bool on)
{
  iostream_uart->set_rx_energy_mode_restriction(iostream_uart->stream.context, on);
}

/***************************************************************************//**
 * Get reception energy mode restriction configuration.
 *
 * @param[in] iostream_uart  UART context.
 *
 * @return Sleep configuration.
 ******************************************************************************/
__STATIC_INLINE bool sl_iostream_uart_get_rx_energy_mode_restriction(sl_iostream_uart_t *iostream_uart)
{
  return iostream_uart->get_rx_energy_mode_restriction(iostream_uart->stream.context);
}
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
/***************************************************************************//**
 * Configure Read blocking mode.
 *
 * @param[in] iostream_uart  UART context.
 *
 * @param[in] on  If false, the read API will be non-blocking. Otherwise the
 *                read API will block until data is received.
 ******************************************************************************/
__STATIC_INLINE void sl_iostream_uart_set_read_block(sl_iostream_uart_t *iostream_uart,
                                                     bool on)
{
  iostream_uart->set_read_block(iostream_uart->stream.context, on);
}

/***************************************************************************//**
 * Get read API block configuration.
 *
 * @param[in] iostream_uart  UART context.
 *
 * @return Block mode.
 ******************************************************************************/
__STATIC_INLINE bool sl_iostream_uart_get_read_block(sl_iostream_uart_t *iostream_uart)
{
  return iostream_uart->get_read_block(iostream_uart->stream.context);
}
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) && !defined(SL_CATALOG_KERNEL_PRESENT)
/***************************************************************************//**
 * Get the UART status on isr exit (sleep, wakeup or ignore).
 *
 * @param[in] iostream_uart  UART context.
 *
 * @return Sleep status:
 *            SL_POWER_MANAGER_IGNORE; UART has not generated the ISR
 *            SL_POWER_MANAGER_WAKEUP; UART generated the ISR and the system must wakeup
 *            SL_POWER_MANAGER_SLEEP; UART generated the ISR and the system can go back to sleep.
 ******************************************************************************/
__STATIC_INLINE sl_power_manager_on_isr_exit_t sl_iostream_uart_sleep_on_isr_exit(sl_iostream_uart_t *iostream_uart)
{
  return iostream_uart->sleep_on_isr_exit(iostream_uart->stream.context);
}

#endif

/** @} (end addtogroup iostream_uart) */
/** @} (end addtogroup iostream) */

#ifdef __cplusplus
}
#endif

#endif // SL_IOSTREAM_UART_H
