/*
* ADMA KMDF Driver Library
* ===========
*
* Copyright 2018 Qinger Inc.
*
* Maintainer:
* -----------
* zhuce <qingermaker@sina.com>
*
* References:
* -----------
*   [1] ug_a10_pcie_avmm_dma.pdf - Intel Arria 10 or Intel Cyclone10 Avalon-MM DMA Interface for PCIe* Solutions User Guide
*/

#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "adevice.h"

#define ADMA_MAKE_VERSION(major, minor, patch) (((major) << 24) | ((minor) << 26) | (patch))
#define ADMA_VERSION_MAJOR(version) (((uint32_t)(version) >> 24) & 0xff)
#define ADMA_VERSION_MINOR(version) (((uint32_t)(version) >> 16) & 0xff)
#define ADMA_VERSION_PATCH(version) ((uint32_t)(version) & 0xffff)

// TODO Bump this number when making a new release
#define ADMA_LIB_VERSION ADMA_MAKE_VERSION(2017, 4, 1)

/**
 * \brief Open and initialize an ADMA device given by the WDFDEVICE handle.
 * \param wdfDevice     [IN]        The OS device handle
 * \param adma          [IN]        The ADMA device context
 * \param ResourcesRaw  [IN]        List of PCIe resources assigned to this device
 * \param ResourcesTranslated [IN]  List of PCIe resources assigned to this device
 * \return STATUS_SUCCESS on successful completion. All other return values indicate error conditions. 
 */
NTSTATUS ADMA_DeviceOpen(WDFDEVICE wdfDevice,
                         PADMA_DEVICE adma,
                         WDFCMRESLIST ResourcesRaw,
                         WDFCMRESLIST ResourcesTranslated);

/**
 * \brief Close and cleanup the ADMA device.
 * \param xdma          [IN]        The ADMA device context
 */
void ADMA_DeviceClose(PADMA_DEVICE xdma);

/**
 * \brief Register a callback function to execute when user events occur. 
 * \param adma          [IN]        The ADMA device context
 * \param index         [IN]        The Event ID of the user event (0-15)
 * \param handler       [IN]        The callback function to execute on event detection
 * \param userData      [IN]        Custom user data/handle which will be passed to the callback 
 *                                  function. 
 * \return STATUS_SUCCESS on successful completion. All other return values indicate error conditions. 
 */
NTSTATUS ADMA_UserIsrRegister(PADMA_DEVICE xdma,
                              ULONG index,
                              PFN_ADMA_USER_WORK handler,
                              void* userData);

/**
 * \brief Enable interrupt handling for a specific user event. 
 * \param xdma          [IN]        The ADMA device context
 * \param eventId       [IN]        The Event ID of the user event (0-15)
 * \return STATUS_SUCCESS on successful completion. All other return values indicate error conditions. 
 */
NTSTATUS ADMA_UserIsrEnable(PADMA_DEVICE xdma, ULONG eventId);

/**
 * \brief Disable interrupt handling for a specific user event. 
 * \param adma          [IN]        The ADMA device context
 * \param eventId       [IN]        The Event ID of the user event (0-15)
 * \return STATUS_SUCCESS on successful completion. All other return values indicate error conditions. 
 */
NTSTATUS ADMA_UserIsrDisable(PADMA_DEVICE xdma, ULONG eventId);

/**
 * \brief OS callback function for programming the ADMA engine
 * \param Transaction    [IN]        The WDFDMATRANSACTION handle
 * \param Device         [IN]        The WDFDEVICE handle
 * \param Context        [IN]        The ADMA engine context
 * \param Direction      [IN]        Data transaction direction. H2C=WdfDmaDirectionToDevice. C2H=WdfDmaDirectionFromDevice
 * \param SgList         [IN]        The Scatter-Gather list describing the Host-side memory.
 * \return TRUE on success, else FALSE
 */
EVT_WDF_PROGRAM_DMA ADMA_EngineProgramDma;

/**
 * \brief Select between polling and interrupts as a mechanism for determining dma transfer 
 *        completion on a per DMA engine basis.
 * \param engine        [IN]        The DMA engine context
 * \param pollMode      [IN]        true = use polling, false = use interrupts
 */
void ADMA_EngineSetPollMode(ADMA_ENGINE* engine, BOOLEAN pollMode);