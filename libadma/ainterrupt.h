/*
* ADMA Interrupt Service Routines, Handlers and Dispatch Functions
* ===============================
*
* Copyright 2018 Qinger Inc.
*
* Maintainer:
* -----------
* zhuce <qingermaker@sina.com>
*
*/
#pragma once

// ========================= include dependencies =================================================
#include <ntddk.h>
#include "adma_engine.h"

// ========================= declarations =================================================

#define ADMA_MAX_USER_IRQ (16)
#define ADMA_MAX_NUM_IRQ (ADMA_MAX_USER_IRQ + ADMA_MAX_CHAN_IRQ)

typedef struct _IRQ_CONTEXT {
    ULONG eventId;
    UINT32 channelIrqPending; // channel irq that have fired
    UINT32 userIrqPending; // user event irq that have fired
    ADMA_ENGINE* engine;
    volatile ADMA_IRQ_REGS* regs;
    PADMA_DEVICE adma;
} IRQ_CONTEXT, *PIRQ_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(IRQ_CONTEXT, GetIrqContext)


/// Initialize the interrupt resources given by the OS for use by DMA engines and user events 
 NTSTATUS SetupInterrupts(IN PADMA_DEVICE adma,
                         IN WDFCMRESLIST ResourcesRaw,
                         IN WDFCMRESLIST ResourcesTranslated);

