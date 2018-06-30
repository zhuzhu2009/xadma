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

// ========================= include dependencies =================================================

#include <ntddk.h>
#include "areg.h"
#include "adma_engine.h"
#include "ainterrupt.h"

// ========================= constants ============================================================

#define ADMA_MAX_NUM_BARS (6)//change by zhuce according to UG-01145 Page149

// ========================= type declarations ====================================================


/// Callback function type for user-defined work to be executed on receiving a user event.
typedef void(*PFN_ADMA_USER_WORK)(ULONG eventId, void* userData);

/// user event interrupt context
typedef struct ADMA_EVENT_T {
    PFN_ADMA_USER_WORK work; // user callback 
    void* userData; // custom user data. will be passed into work callback function
    WDFINTERRUPT irq; //wdf interrupt handle
} ADMA_EVENT;

/// The ADMA device context
typedef struct ADMA_DEVICE_T {

    // WDF 
    WDFDEVICE wdfDevice;

    // PCIe BAR access
    UINT numBars;
    PVOID bar[ADMA_MAX_NUM_BARS]; // kernel virtual address of BAR
    ULONG barLength[ADMA_MAX_NUM_BARS];
    ULONG configBarIdx;
    LONG userBarIdx;
    LONG bypassBarIdx;
    volatile ADMA_CONFIG_REGS *configRegs;
    volatile ADMA_IRQ_REGS *interruptRegs;
    volatile ADMA_SGDMA_COMMON_REGS * sgdmaRegs;

    // DMA Engine management
    ADMA_ENGINE engines[ADMA_MAX_NUM_CHANNELS][ADMA_NUM_DIRECTIONS];
    WDFDMAENABLER dmaEnabler;   // WDF DMA Enabler for the engine queues

    // Interrupt Resources
    WDFINTERRUPT lineInterrupt;
    WDFINTERRUPT channelInterrupts[ADMA_MAX_CHAN_IRQ];

    // user events
    ADMA_EVENT userEvents[ADMA_MAX_USER_IRQ];

} ADMA_DEVICE, *PADMA_DEVICE;

// ========================= function declarations ================================================


