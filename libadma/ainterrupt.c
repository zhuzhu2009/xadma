/*
* ADMA Interrupt Service Routines, Handlers and Dispatch Functions
* ===============================
*
* Copyright 2017 Xilinx Inc.
* Copyright 2010-2012 Sidebranch
* Copyright 2010-2012 Leon Woestenberg <leon@sidebranch.com>
*
* Maintainer:
* -----------
* Alexander Hornburg <alexande@xilinx.com>
*
*/

// ========================= include dependencies =================================================
#include "adevice.h"
#include "adma_engine.h"
#include "ainterrupt.h"
#include "apcie_common.h"

#include "trace.h"
#ifdef DBG
// The trace message header (.tmh) file must be included in a source file before any WPP macro 
// calls and after defining a WPP_CONTROL_GUIDS macro (defined in trace.h). see trace.h
#include "ainterrupt.tmh"
#endif


// ====================== declarations ============================================================



EVT_WDF_INTERRUPT_ISR        EvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC        EvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE     EvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE    EvtInterruptDisable;

EVT_WDF_INTERRUPT_ISR        EvtChannelInterruptIsr;
EVT_WDF_INTERRUPT_DPC        EvtChannelInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE     EvtChannelInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE    EvtChannelInterruptDisable;

EVT_WDF_INTERRUPT_ISR        EvtUserInterruptIsr;
EVT_WDF_INTERRUPT_DPC        EvtUserInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE     EvtUserInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE    EvtUserInterruptDisable;

// ====================== static setup functions =================================================

static UINT32 BuildVectorReg(UINT32 a, UINT32 b, UINT32 c, UINT32 d) {
    UINT32 reg_val = 0;
    reg_val |= (a & 0x1f) << 0;
    reg_val |= (b & 0x1f) << 8;
    reg_val |= (c & 0x1f) << 16;
    reg_val |= (d & 0x1f) << 24;
    return reg_val;
}

static NTSTATUS SetupUserInterrupt(IN PADMA_DEVICE adma, IN ULONG index,
                                   IN PCM_PARTIAL_RESOURCE_DESCRIPTOR resource,
                                   IN PCM_PARTIAL_RESOURCE_DESCRIPTOR translatedResource) {
    WDF_INTERRUPT_CONFIG config;
    WDF_INTERRUPT_CONFIG_INIT(&config, EvtUserInterruptIsr, EvtUserInterruptDpc);
    config.InterruptRaw = resource;
    config.InterruptTranslated = translatedResource;
    config.EvtInterruptEnable = EvtUserInterruptEnable;
    config.EvtInterruptDisable = EvtUserInterruptDisable;

    WDF_OBJECT_ATTRIBUTES attribs;
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, IRQ_CONTEXT);

    NTSTATUS status = WdfInterruptCreate(adma->wdfDevice, &config, &attribs,
                                         &(adma->userEvents[index].irq));
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "WdfInterruptCreate failed: %!STATUS!", status);
    }

    PIRQ_CONTEXT irqContext = GetIrqContext(adma->userEvents[index].irq);
    irqContext->eventId = index; // msg Id = irq index = event id
    irqContext->regs = adma->interruptRegs;
    irqContext->adma = adma;
    return status;
}

static NTSTATUS SetupChannelInterrupt(IN PADMA_DEVICE adma, IN ULONG index,
                                      IN PCM_PARTIAL_RESOURCE_DESCRIPTOR resource,
                                      IN PCM_PARTIAL_RESOURCE_DESCRIPTOR translatedResource) {
    WDF_INTERRUPT_CONFIG config;
    WDF_INTERRUPT_CONFIG_INIT(&config, EvtChannelInterruptIsr, EvtChannelInterruptDpc);
    config.InterruptRaw = resource;
    config.InterruptTranslated = translatedResource;
    config.EvtInterruptEnable = EvtChannelInterruptEnable;
    config.EvtInterruptDisable = EvtChannelInterruptDisable;

    WDF_OBJECT_ATTRIBUTES attribs;
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, IRQ_CONTEXT);

    NTSTATUS status = WdfInterruptCreate(adma->wdfDevice, &config, &attribs,
                                         &(adma->channelInterrupts[index]));
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "WdfInterruptCreate failed: %!STATUS!", status);
    }
    PIRQ_CONTEXT irqContext = GetIrqContext(adma->channelInterrupts[index]);
    irqContext->regs = adma->interruptRegs;
    irqContext->adma = adma;
    return status;
}

static NTSTATUS SetupDeviceInterrupt(IN PADMA_DEVICE adma, IN PCM_PARTIAL_RESOURCE_DESCRIPTOR resource,
                                     IN PCM_PARTIAL_RESOURCE_DESCRIPTOR translatedResource) {
    WDF_INTERRUPT_CONFIG config;
    WDF_INTERRUPT_CONFIG_INIT(&config, EvtInterruptIsr, EvtInterruptDpc);
    config.InterruptRaw = resource;
    config.InterruptTranslated = translatedResource;
    config.EvtInterruptEnable = EvtInterruptEnable;
    config.EvtInterruptDisable = EvtInterruptDisable;

    WDF_OBJECT_ATTRIBUTES attribs;
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, IRQ_CONTEXT);

    NTSTATUS status = WdfInterruptCreate(adma->wdfDevice, &config, &attribs, &adma->lineInterrupt);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "WdfInterruptCreate failed: %!STATUS!", status);
    }
    PIRQ_CONTEXT irqContext = GetIrqContext(adma->lineInterrupt);
    irqContext->adma = adma;
    irqContext->regs = adma->interruptRegs;
    return status;
}

static NTSTATUS SetupSingleInterrupt(IN PADMA_DEVICE adma, IN WDFCMRESLIST ResourcesRaw,
                                     IN WDFCMRESLIST ResourcesTranslated) {
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceRaw;
    NTSTATUS status = STATUS_INTERNAL_ERROR;
    ULONG numResources = WdfCmResourceListGetCount(ResourcesTranslated);
    UINT32 vectorValue = 0;

    ASSERT(adma->interruptRegs != NULL);

    for (UINT i = 0; i < numResources; i++) {
        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        resourceRaw = WdfCmResourceListGetDescriptor(ResourcesRaw, i);

        if (resource->Type != CmResourceTypeInterrupt) {
            continue;
        }

        status = SetupDeviceInterrupt(adma, resourceRaw, resource);
        if (!NT_SUCCESS(status)) {
            TraceError(DBG_INIT, "Error in setup device interrupt: %!STATUS!", status);
            return status;
        }

        if (!(resource->Flags & CM_RESOURCE_INTERRUPT_MESSAGE)) { // LINE interrupt

            status = GetLineInterruptPin(adma->wdfDevice, &vectorValue); // todo is this required?
            if (!NT_SUCCESS(status)) {
                TraceError(DBG_INIT, "GetLineInterruptPin failed! %!STATUS!", status);
                return status;
            }

            // Windows: A=1, B=2, C=3, D=4
            // ADMA:    A=0, B=1, C=2, D=3
            vectorValue--;
        }

        status = STATUS_SUCCESS;
        break;
    }

    TraceVerbose(DBG_INIT, "User/Channel interrupt vector value = %u", vectorValue);

    adma->interruptRegs->userVector[0] = vectorValue;
    adma->interruptRegs->userVector[1] = vectorValue;
    adma->interruptRegs->userVector[2] = vectorValue;
    adma->interruptRegs->userVector[3] = vectorValue;
    adma->interruptRegs->channelVector[0] = vectorValue;
    adma->interruptRegs->channelVector[1] = vectorValue;

    return status;
}

static NTSTATUS SetupMsixInterrupts(IN PADMA_DEVICE adma, IN WDFCMRESLIST ResourcesRaw,
                                    IN WDFCMRESLIST ResourcesTranslated) {
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceRaw;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG numResources = WdfCmResourceListGetCount(ResourcesTranslated);
    ULONG interruptCount = 0;

    ASSERT(adma->interruptRegs != NULL);

    for (UINT i = 0; (i < numResources) && (interruptCount < ADMA_MAX_NUM_IRQ); i++) {

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        resourceRaw = WdfCmResourceListGetDescriptor(ResourcesRaw, i);

        if (resource->Type != CmResourceTypeInterrupt) {
            continue;
        }

        // assign first 16 interrupt resources to user events
        if (interruptCount < ADMA_MAX_USER_IRQ) {
            status = SetupUserInterrupt(adma, interruptCount, resourceRaw, resource);
        } else { // assign next 8 interrupt resources to dma engines
            status = SetupChannelInterrupt(adma, interruptCount - ADMA_MAX_USER_IRQ,
                                           resourceRaw, resource);
        }
        if (!NT_SUCCESS(status)) {
            TraceError(DBG_INIT, "Error in setup device interrupt: %!STATUS!", status);
            return status;
        }

        ++interruptCount;
    }

    // first 16 msg IDs are user irq
    adma->interruptRegs->userVector[0] = BuildVectorReg(0, 1, 2, 3);
    adma->interruptRegs->userVector[1] = BuildVectorReg(4, 5, 6, 7);
    adma->interruptRegs->userVector[2] = BuildVectorReg(8, 9, 10, 11);
    adma->interruptRegs->userVector[3] = BuildVectorReg(12, 13, 14, 15);

    // next 8 are dma channel
    adma->interruptRegs->channelVector[0] = BuildVectorReg(16, 17, 18, 19);
    adma->interruptRegs->channelVector[1] = BuildVectorReg(20, 21, 22, 23);

    return status;
}

static NTSTATUS SetupMultiMsiInterrupts(IN PADMA_DEVICE adma, IN WDFCMRESLIST ResourcesRaw,
                                        IN WDFCMRESLIST ResourcesTranslated, IN USHORT numVectors) {
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceRaw;
    NTSTATUS status = STATUS_INTERNAL_ERROR;
    ULONG numResources = WdfCmResourceListGetCount(ResourcesTranslated);
#if 0
    ASSERT(adma->interruptRegs != NULL);
#endif
    for (UINT i = 0; i < numResources; i++) {
        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        resourceRaw = WdfCmResourceListGetDescriptor(ResourcesRaw, i);

        if (resource->Type != CmResourceTypeInterrupt) {
            continue;
        }
        resource->u.MessageInterrupt.Raw.MessageCount = numVectors;
        resourceRaw->u.MessageInterrupt.Raw.MessageCount = numVectors;
        // individual resource/msgId for each user interrupt (0-15)
        for (int n = 0; n < ADMA_MAX_USER_IRQ; n++) {
//            status = SetupUserInterrupt(adma, 0, resourceRaw, resource);
			status = SetupUserInterrupt(adma, n, resourceRaw, resource);//fixed by zc
			if (!NT_SUCCESS(status)) {
                TraceError(DBG_INIT, "Error in setup user interrupt: %!STATUS!", status);
                return status;
            }
        }

        // individual resource/msgId for each channel interrupt (H2C 0-3 and C2H 0-3)
        for (int n = 0; n < (2 * ADMA_MAX_NUM_CHANNELS); n++) {
            status = SetupChannelInterrupt(adma, n, resourceRaw, resource);
            if (!NT_SUCCESS(status)) {
                TraceError(DBG_INIT, "Error in setup channel interrupt: %!STATUS!", status);
                return status;
            }
        }
        status = STATUS_SUCCESS;
        break;
    }

#if 0
    TraceVerbose(DBG_INIT, "User interrupt msg id = 0");
    TraceVerbose(DBG_INIT, "Channel interrupt msg ids = H2C[1,2,3,4], C2H[5,6,7,8]");

    // first 16 msg IDs are user irq
    adma->interruptRegs->userVector[0] = BuildVectorReg(0, 1, 2, 3);
    adma->interruptRegs->userVector[1] = BuildVectorReg(4, 5, 6, 7);
    adma->interruptRegs->userVector[2] = BuildVectorReg(8, 9, 10, 11);
    adma->interruptRegs->userVector[3] = BuildVectorReg(12, 13, 14, 15);

    // next 8 are dma channel
    adma->interruptRegs->channelVector[0] = BuildVectorReg(16, 17, 18, 19);
    adma->interruptRegs->channelVector[1] = BuildVectorReg(20, 21, 22, 23);
#endif
    return status;
}

static NTSTATUS CountInterruptResources(IN WDFCMRESLIST ResourcesTranslated,
                                        OUT PULONG numInterruptResources) {
    const ULONG numResources = WdfCmResourceListGetCount(ResourcesTranslated);
    TraceVerbose(DBG_INIT, "# PCIe resources = %d", numResources);

    for (UINT i = 0; i < numResources; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (!resource) {
            TraceError(DBG_INIT, "WdfResourceCmGetDescriptor() fails");
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        if (resource->Type == CmResourceTypeInterrupt) {
            (*numInterruptResources)++;
        }
    }
    return STATUS_SUCCESS;
}

// ====================== line/msi interrupt callback functions ===================================

NTSTATUS EvtInterruptEnable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    IRQ_CONTEXT* irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    irq->regs->channelIntEnableW1S = 0xFFFFFFFFUL;
    irq->regs->userIntEnableW1S = 0xFFFFFFFFUL;
    TraceVerbose(DBG_IRQ, "enabled ALL interrupts");
    return STATUS_SUCCESS;
}

NTSTATUS EvtInterruptDisable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    IRQ_CONTEXT* irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    irq->regs->channelIntEnableW1C = 0xFFFFFFFFUL;
    irq->regs->userIntEnableW1C = 0xFFFFFFFFUL;
    TraceVerbose(DBG_IRQ, "disabled ALL interrupts");
    return STATUS_SUCCESS;
}

BOOLEAN EvtInterruptIsr(IN WDFINTERRUPT Interrupt, IN ULONG MessageID)
// interrupt service routine - handle line interrupts
{
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);
    UINT32 chIrq = 0;
    UINT32 userIrq = 0;

    TraceInfo(DBG_IRQ, "irq messageId = %u", MessageID);

    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);

    // read channel interrupt request registers
    // channel interrupt(s) requested?
    chIrq = irq->regs->channelIntRequest;
    TraceVerbose(DBG_IRQ, "chan EN=0x%08X RQ=0x%08X PE=0x%08X",
                 irq->regs->channelIntEnable, irq->regs->channelIntRequest, irq->regs->channelIntPending);
    if (chIrq) {
        irq->channelIrqPending |= chIrq; // remember fired channel interrupts
        irq->regs->channelIntEnableW1C = chIrq; // disable fired channel interrupts
    }

    // read user interrupts that are pending in the controller - flushes previous write
    // user interrupt(s) requested?
    userIrq = irq->regs->userIntRequest;
    TraceVerbose(DBG_IRQ, "user EN=0x%08X RQ=0x%08X PE=0x%08X",
                 irq->regs->userIntEnable, irq->regs->userIntRequest, irq->regs->userIntPending);
    if (userIrq) {
        irq->userIrqPending |= userIrq; // remember fired user interrupts
        irq->regs->userIntEnableW1C = userIrq; // disable fired user interrupts
    }

    // was the interrupt handled correctly?
    if (!chIrq && !userIrq) {
        TraceWarning(DBG_IRQ, "Spurious interrupt");
        return FALSE;
    }

    // schedule deferred work
    WdfInterruptQueueDpcForIsr(Interrupt);
    return TRUE;
}

VOID EvtInterruptDpc(IN WDFINTERRUPT interrupt, IN WDFOBJECT device)
// Deferred interrupt service handler
{
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(interrupt);

    // dma engine interrupt pending?
    TraceVerbose(DBG_IRQ, "channelIrqPending=0x%08X", irq->channelIrqPending);
    for (UINT dir = H2C; dir < 2; dir++) { // 0=H2C, 1=C2H
        for (UINT channel = 0; channel < ADMA_MAX_NUM_CHANNELS; channel++) {
            ADMA_ENGINE* engine = &irq->adma->engines[channel][dir];
            if (engine->enabled && (irq->channelIrqPending & engine->irqBitMask)) {
                TraceInfo(DBG_IRQ, "%s_%u servicing interrupt", DirectionToString(dir), channel);
                ASSERT(engine->work != NULL);
                engine->work(engine);
            }
        }
    }

    // user event interrupt pending?
    TraceVerbose(DBG_IRQ, "userIrqPending=0x%08X", irq->userIrqPending);
    for (UINT i = 0; i < ADMA_MAX_USER_IRQ; ++i) {
        ADMA_EVENT* userEvent = &irq->adma->userEvents[i];
        if (irq->userIrqPending & BIT_N(i)) {
            if (userEvent->work != NULL) {
                userEvent->work(i, userEvent->userData);
            }
        }
    }

    // re-enable interrupts
    WdfInterruptAcquireLock(interrupt);
    irq->regs->channelIntEnableW1S = irq->channelIrqPending;
    irq->channelIrqPending = 0x0;

    // FIXME - Remove the user interrupt source condition before reenabling the user interrupt
    // This depends on user logic and how the interrupt has been triggered!
    // This reference driver puts the responsibility on the user-space application to remove the 
    // user event interrupt source condition.

    irq->regs->userIntEnableW1S = irq->userIrqPending;
    irq->userIrqPending = 0x0;
    WdfInterruptReleaseLock(interrupt);
    TraceVerbose(DBG_IRQ, "channel EN=0x%08X RQ=0x%08X PE=0x%08X",
                 irq->regs->channelIntEnable, irq->regs->channelIntRequest, irq->regs->channelIntPending);
    TraceVerbose(DBG_IRQ, "user EN=0x%08X RQ=0x%08X PE=0x%08X",
                 irq->regs->userIntEnable, irq->regs->userIntRequest, irq->regs->userIntPending);
    return;
}

// ====================== MSI-X interrupt callback functions ======================================

NTSTATUS EvtChannelInterruptEnable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    EngineEnableInterrupt(irq->engine);
    return STATUS_SUCCESS;
}

NTSTATUS EvtChannelInterruptDisable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    EngineDisableInterrupt(irq->engine);
    return STATUS_SUCCESS;
}

BOOLEAN EvtChannelInterruptIsr(IN WDFINTERRUPT Interrupt, IN ULONG MessageID)
// interrupt service routine - handles msi/msi-x interrupts only
{
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);

    EXPECT(irq != NULL);

    TraceVerbose(DBG_IRQ, "%s_%u interrupt occurred! messageId=%u",
                 DirectionToString(irq->engine->dir), irq->engine->channel, MessageID);

    EngineDisableInterrupt(irq->engine);

    return WdfInterruptQueueDpcForIsr(Interrupt);   // schedule deferred work
}

VOID EvtChannelInterruptDpc(IN WDFINTERRUPT interrupt, IN WDFOBJECT device)
// Deferred interrupt service handler
{
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(interrupt);

    // do engine specific work (either EngineProcessTransfer (MM) or EngineProcessRing (ST))
    irq->engine->work(irq->engine);

    // reenable interrupt for this dma engine
    WdfInterruptAcquireLock(interrupt);
    EngineEnableInterrupt(irq->engine);
    WdfInterruptReleaseLock(interrupt);
}

NTSTATUS EvtUserInterruptEnable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    irq->regs->userIntEnableW1S = BIT_N(irq->eventId); // message id and event id are same
    TraceInfo(DBG_IRQ, "event_%u enabled interrupt", irq->eventId);
    return STATUS_SUCCESS;
}

NTSTATUS EvtUserInterruptDisable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE device) {
    UNREFERENCED_PARAMETER(device);
    PIRQ_CONTEXT irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    irq->regs->userIntEnableW1C = BIT_N(irq->eventId); // message id and event id are same
    TraceInfo(DBG_IRQ, "event_%u disabled interrupt", irq->eventId);
    return STATUS_SUCCESS;
}

BOOLEAN EvtUserInterruptIsr(IN WDFINTERRUPT Interrupt, IN ULONG MessageID) {
    TraceInfo(DBG_IRQ, "event_%u occurred!", MessageID);
    IRQ_CONTEXT* irq = GetIrqContext(Interrupt);
    EXPECT(irq != NULL);
    EXPECT(irq->regs != NULL);
    // disable user event interrupt
    irq->regs->userIntEnableW1C = BIT_N(MessageID); // message id and event id are same
    return WdfInterruptQueueDpcForIsr(Interrupt); // schedule deferred work;
}

VOID EvtUserInterruptDpc(IN WDFINTERRUPT interrupt, IN WDFOBJECT device)
// Deferred interrupt service handler
{
    UNREFERENCED_PARAMETER(device);
    IRQ_CONTEXT* irq = GetIrqContext(interrupt);
    EXPECT(irq != NULL);
    ADMA_EVENT* userEvent = &irq->adma->userEvents[irq->eventId];
    EXPECT(userEvent != NULL);

    // message id and event id are same
    if (userEvent->work != NULL) {
        TraceInfo(DBG_IRQ, "event_%d executing work handler", irq->eventId);
        userEvent->work(irq->eventId, userEvent->userData);
    }

    // reenable interrupt
    WdfInterruptAcquireLock(interrupt);
    irq->regs->userIntEnableW1S = BIT_N(irq->eventId);
    WdfInterruptReleaseLock(interrupt);
}

// ====================== internal api implementation ==============================================

NTSTATUS SetupInterrupts(IN PADMA_DEVICE adma,
                         IN WDFCMRESLIST ResourcesRaw,
                         IN WDFCMRESLIST ResourcesTranslated) {

    NTSTATUS status = STATUS_SUCCESS;
    ULONG numIrqResources = 0;
    USHORT numMsiVectors = 0; // only for multi-message MSI, not MSI-X!

    status = CountInterruptResources(ResourcesTranslated, &numIrqResources);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "CountInterruptResources failed: %!STATUS!", status);
    }
    status = GetNumMsiVectors(adma->wdfDevice, &numMsiVectors);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "GetNumMsiVectors failed: %!STATUS!", status);
    }

    TraceVerbose(DBG_INIT, "adma->numIrqResources=%u numMsiVectors=%u", numIrqResources, numMsiVectors);
    if (numIrqResources >= ADMA_MAX_NUM_IRQ) { // msi-x
        status = SetupMsixInterrupts(adma, ResourcesRaw, ResourcesTranslated);
    } else if (numMsiVectors >= ADMA_MAX_NUM_IRQ) { //multi-message MSI with enough contiguous vectors
        status = SetupMultiMsiInterrupts(adma, ResourcesRaw, ResourcesTranslated, numMsiVectors);
    } else { // Line or single-message MSI
        status = SetupSingleInterrupt(adma, ResourcesRaw, ResourcesTranslated);
    }
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "Setting up interrupts failed: %!STATUS!", status);
    }
    return status;
}

// ============================== public api implementation =======================================

NTSTATUS ADMA_UserIsrRegister(PADMA_DEVICE adma, ULONG eventId, PFN_ADMA_USER_WORK handler,
                              void* userData) {
    if (eventId >= ADMA_MAX_USER_IRQ) {
        TraceError(DBG_INIT, "Invalid index! %u", eventId);
        return STATUS_INVALID_PARAMETER;
    }

    adma->userEvents[eventId].work = handler;
    adma->userEvents[eventId].userData = userData;
    return STATUS_SUCCESS;
}

NTSTATUS ADMA_UserIsrEnable(PADMA_DEVICE adma, ULONG eventId) {
    EXPECT(adma != NULL);

    if (eventId >= ADMA_MAX_USER_IRQ) {
        TraceError(DBG_INIT, "Invalid index! %u", eventId);
        return STATUS_INVALID_PARAMETER;
    }
    adma->interruptRegs->userIntEnableW1S = BIT_N(eventId);
    return STATUS_SUCCESS;
}

NTSTATUS ADMA_UserIsrDisable(PADMA_DEVICE adma, ULONG eventId) {
    EXPECT(adma != NULL);

    if (eventId >= ADMA_MAX_USER_IRQ) {
        TraceError(DBG_INIT, "Invalid index! %u", eventId);
        return STATUS_INVALID_PARAMETER;
    }
    adma->interruptRegs->userIntEnableW1C = BIT_N(eventId);
    return STATUS_SUCCESS;
}

