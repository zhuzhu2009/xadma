/*
* ADMA Device
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
*   [2] Windows Dev - Using Automatic Synchronization - https://msdn.microsoft.com/en-us/windows/hardware/drivers/wdf/using-automatic-synchronization
*/

// ====================== include dependancies ========================================================

#include <ntddk.h>
#include <initguid.h> // required for GUID definitions
#include <wdmguid.h> // required for WMILIB_CONTEXT

#include "adevice.h"
#include "ainterrupt.h"
#include "adma_engine.h"
#include "xdma_public.h"

#include "trace.h"
#ifdef DBG
// The trace message header (.tmh) file must be included in a source file before any WPP macro 
// calls and after defining a WPP_CONTROL_GUIDS macro (defined in trace.h). see trace.h
#include "adevice.tmh"
#endif

// ====================== Function declarations ========================================================

// declare following functions as pageable code
#ifdef ALLOC_PRAGMA

#endif

// WDK 10 static code analysis feature expects to target Windows 10 and thus recommends not to
// use MmMapIoSpace and use MmMapIoSpaceEx instead. However this function is not available pre 
// Win 10. Thus disable this warning. 
// see https://social.msdn.microsoft.com/Forums/en-US/f8a3fb63-10de-481c-b629-8b5f3d254c5e/unexpected-code-analysis-behavior?forum=wdk
#pragma warning (disable : 30029) 

// ====================== constants ========================================================

// Version constants for the ADMA IP core
typedef enum ADMA_IP_VERSION_T {
    v2015_4 = 1,
    v2016_1 = 2,
    v2016_2 = 3,
    v2016_3 = 4,
    v2016_4 = 5,
    v2017_1 = 6,
    v2017_2 = 7,
    v2017_3 = 8
} ADMA_IP_VERSION;

// ====================== static functions ========================================================

// Get the ADMA IP core version
static ADMA_IP_VERSION GetVersion(IN OUT PADMA_DEVICE adma) {
    ADMA_IP_VERSION version = adma->configRegs->identifier & 0x000000ffUL;
    TraceVerbose(DBG_INIT, "version is 0x%x", version);
    return version;
}

// Initialize the ADMA_DEVICE structure with default values
static void DeviceDefaultInitialize(IN PADMA_DEVICE adma) {
    ASSERT(adma != NULL);

    // bars
    adma->numBars = 0;
    for (UINT32 i = 0; i < ADMA_MAX_NUM_BARS; i++) {
        adma->bar[i] = NULL;
        adma->barLength[i] = 0;
    }
    adma->configBarIdx = 1;//change by zhuce according to UG-01145 Page149
    adma->userBarIdx = -1;
    adma->bypassBarIdx = -1;

    // registers 
    adma->configRegs = NULL;
    adma->interruptRegs = NULL;
    adma->sgdmaRegs = NULL;

    // engines
    for (UINT dir = H2C; dir < 2; dir++) { // 0=H2C, 1=C2H
        for (ULONG ch = 0; ch < ADMA_MAX_NUM_CHANNELS; ch++) {
            adma->engines[ch][dir].enabled = FALSE;
            adma->engines[ch][dir].poll = FALSE;
        }
    }

    // interrupts - nothing to do

    // user events
    for (int i = 0; i < ADMA_MAX_USER_IRQ; i++) {
        adma->userEvents[i].work = NULL;
        adma->userEvents[i].userData = NULL;
    }
}

// Iterate through PCIe resources and map BARS into host memory
static NTSTATUS MapBARs(IN PADMA_DEVICE adma, IN WDFCMRESLIST ResourcesTranslated) {

    const ULONG numResources = WdfCmResourceListGetCount(ResourcesTranslated);
    TraceVerbose(DBG_INIT, "# PCIe resources = %d", numResources);

    for (UINT i = 0; i < numResources; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (!resource) {
            TraceError(DBG_INIT, "WdfResourceCmGetDescriptor() fails");
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

		TraceInfo(DBG_INIT, "MM BAR %d type %d", i, resource->Type);

        if (resource->Type == CmResourceTypeMemory) {
            adma->barLength[adma->numBars] = resource->u.Memory.Length;
            adma->bar[adma->numBars] = MmMapIoSpace(resource->u.Memory.Start,
                                                    resource->u.Memory.Length, MmNonCached);
            if (adma->bar[adma->numBars] == NULL) {
                TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", adma->numBars);
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
            TraceInfo(DBG_INIT, "MM BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
                      adma->numBars, resource->u.Memory.Start.QuadPart,
                      resource->u.Memory.Length, adma->bar[adma->numBars]);
            adma->numBars++;
		} else if (resource->Type == CmResourceTypeMemoryLarge) {
			if (resource->Flags & CM_RESOURCE_MEMORY_LARGE_40) {
#if 0
				adma->barLength[adma->numBars] = resource->u.Memory40.Length40 << 8;
				adma->bar[adma->numBars] = MmMapIoSpace(resource->u.Memory40.Start,
														resource->u.Memory40.Length40 << 8, MmNonCached);
#else
				PHYSICAL_ADDRESS start;
				start.QuadPart = resource->u.Memory40.Start.QuadPart;		
				adma->barLength[adma->numBars] = 0x10000;//we only use part of it because customer design is ugly
				//start.QuadPart += 0x80000000LL;//offset to 2GB, customer design is stupid, I'll use only 2GB~2GB+64KB(0x10000) of 4GB
				adma->bar[adma->numBars] = MmMapIoSpace(start,
					0x10000, MmNonCached);
#endif
				if (adma->bar[adma->numBars] == NULL) {
					TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", adma->numBars);
					return STATUS_DEVICE_CONFIGURATION_ERROR;
				}
				TraceInfo(DBG_INIT, "MM40 BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
					adma->numBars, resource->u.Memory40.Start.QuadPart,
					resource->u.Memory40.Length40, adma->bar[adma->numBars]);
			} else if (resource->Flags & CM_RESOURCE_MEMORY_LARGE_48) {
				adma->barLength[adma->numBars] = resource->u.Memory48.Length48 << 16;
#if 0
				adma->bar[adma->numBars] = MmMapIoSpace(resource->u.Memory48.Start,
														resource->u.Memory48.Length48 << 16, MmNonCached);
#endif
				if (adma->bar[adma->numBars] == NULL) {
					TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", adma->numBars);
					return STATUS_DEVICE_CONFIGURATION_ERROR;
				}
				TraceInfo(DBG_INIT, "MM48 BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
					adma->numBars, resource->u.Memory48.Start.QuadPart,
					resource->u.Memory48.Length48 << 16, adma->bar[adma->numBars]);
			} else if (resource->Flags & CM_RESOURCE_MEMORY_LARGE_64) {
				adma->barLength[adma->numBars] = resource->u.Memory64.Length64/* << 32*/;
#if 0
				adma->bar[adma->numBars] = MmMapIoSpace(resource->u.Memory64.Start,
														resource->u.Memory64.Length64 << 32, MmNonCached);
#endif
				if (adma->bar[adma->numBars] == NULL) {
					TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", adma->numBars);
					return STATUS_DEVICE_CONFIGURATION_ERROR;
				}
				TraceInfo(DBG_INIT, "MM64 BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
					adma->numBars, resource->u.Memory64.Start.QuadPart,
					resource->u.Memory64.Length64/* << 32*/, adma->bar[adma->numBars]);
			}

			adma->numBars++;
		}
    }
    return STATUS_SUCCESS;
}

// Is the BAR at index 'idx' the config BAR?
static BOOLEAN IsConfigBAR(IN PADMA_DEVICE adma, IN UINT idx) {

    ADMA_IRQ_REGS* irqRegs = (ADMA_IRQ_REGS*)((PUCHAR)adma->bar[idx] + IRQ_BLOCK_OFFSET);
    ADMA_CONFIG_REGS* cfgRegs = (ADMA_CONFIG_REGS*)((PUCHAR)adma->bar[idx] + CONFIG_BLOCK_OFFSET);

    UINT32 interruptID = irqRegs->identifier & ADMA_ID_MASK;
    UINT32 configID = cfgRegs->identifier & ADMA_ID_MASK;

    return ((interruptID == ADMA_ID) && (configID == ADMA_ID)) ? TRUE : FALSE;
}

// Identify which BAR is the config BAR
static UINT FindConfigBAR(IN PADMA_DEVICE adma) {
    for (UINT i = 0; i < adma->numBars; ++i) {
        if (IsConfigBAR(adma, i)) {
            TraceInfo(DBG_INIT, "config BAR is %u", i);
            return i;
        }
    }
    return adma->numBars; //not found - return past-the-end index
}

// Identify all BARs
static NTSTATUS IdentifyBars(IN PADMA_DEVICE adma) {
#if 0 //add by zhuce
    // find DMA config BAR (usually BAR1, see section 'Target Bridge' in [1]) 
    adma->configBarIdx = FindConfigBAR(adma);
    if (adma->configBarIdx == adma->numBars) {
        TraceError(DBG_INIT, "findConfigBar() failed: bar is %d", adma->configBarIdx);
        return STATUS_DRIVER_INTERNAL_ERROR;
    }
    // if config bar is bar0 then user bar doesnt exit
    adma->userBarIdx = adma->configBarIdx == 1 ? 0 : -1;

    // if config bar is not the last bar then bypass bar exists
    adma->bypassBarIdx = adma->numBars - adma->configBarIdx == 2 ? adma->numBars - 1 : -1;

    TraceInfo(DBG_INIT, "%!FUNC!, BAR index: user=%d, control=%d, bypass=%d",
              adma->userBarIdx, adma->configBarIdx, adma->bypassBarIdx);
#endif
	adma->userBarIdx = 1; //add by zhuce
	adma->configBarIdx = 0; //add by zhuce
	adma->bypassBarIdx = -1; //add by zhuce

    return STATUS_SUCCESS;
}

// Get the config, interrupt and sgdma module register offsets
static void GetRegisterModules(IN PADMA_DEVICE adma) {
    PUCHAR configBarAddr = (PUCHAR)adma->bar[adma->configBarIdx];
    adma->configRegs = (ADMA_CONFIG_REGS*)(configBarAddr + CONFIG_BLOCK_OFFSET);
#if 0 // add by zhuce
    adma->interruptRegs = (ADMA_IRQ_REGS*)(configBarAddr + IRQ_BLOCK_OFFSET);
    adma->sgdmaRegs = (ADMA_SGDMA_COMMON_REGS*)(configBarAddr + SGDMA_COMMON_BLOCK_OFFSET);
#endif
}

// ====================== API functions ========================================

NTSTATUS ADMA_DeviceOpen(WDFDEVICE wdfDevice,
                         PADMA_DEVICE adma,
                         WDFCMRESLIST ResourcesRaw,
                         WDFCMRESLIST ResourcesTranslated) {

	UNREFERENCED_PARAMETER(ResourcesRaw);

	NTSTATUS status = STATUS_INTERNAL_ERROR;

    DeviceDefaultInitialize(adma);

    adma->wdfDevice = wdfDevice;

    // map PCIe BARs to host memory
    status = MapBARs(adma, ResourcesTranslated);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "MapBARs() failed! %!STATUS!", status);
        return status;
    }

	status = STATUS_SUCCESS;

    // identify BAR configuration - user(optional), config, bypass(optional)
    status = IdentifyBars(adma);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "IdentifyBars() failed! %!STATUS!", status);
        return status;
    }

    // get the module offsets in config BAR
    GetRegisterModules(adma);
/* //adma hasn't this feature, add by zhuce
    // Confirm ADMA IP core version matches this driver
    UINT version = GetVersion(adma);
    if (version != v2017_1) {
        TraceWarning(DBG_INIT, "Version mismatch! Expected 2017.1 (0x%x) but got (0x%x)",
                     v2017_1, version);
    }
*/
    status = SetupInterrupts(adma, ResourcesRaw, ResourcesTranslated);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "SetupInterrupts failed: %!STATUS!", status);
        return status;
    }

    // WDF DMA Enabler - at least 32 bytes alignment for decriport table Page81, Page53 4 bytes alignment
	WdfDeviceSetAlignmentRequirement(adma->wdfDevice, FILE_32_BYTE_ALIGNMENT); //add by zhuce 
    WDF_DMA_ENABLER_CONFIG dmaConfig;
    WDF_DMA_ENABLER_CONFIG_INIT(&dmaConfig, WdfDmaProfileScatterGather64Duplex, ADMA_MAX_TRANSFER_SIZE);
    status = WdfDmaEnablerCreate(adma->wdfDevice, &dmaConfig, WDF_NO_OBJECT_ATTRIBUTES, &adma->dmaEnabler);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, " WdfDmaEnablerCreate() failed: %!STATUS!", status);
        return status;
    }

    // Detect and initialize engines configured in HW IP 
    status = ProbeEngines(adma);
    if (!NT_SUCCESS(status)) {
        TraceError(DBG_INIT, "ProbeEngines failed: %!STATUS!", status);
        return status;
    }

    return status;
}

void ADMA_DeviceClose(PADMA_DEVICE adma) {

    // todo - stop every engine?

    // reset irq vectors?
    if (adma && adma->interruptRegs) {
        adma->interruptRegs->userVector[0] = 0;
        adma->interruptRegs->userVector[1] = 0;
        adma->interruptRegs->userVector[2] = 0;
        adma->interruptRegs->userVector[3] = 0;
        adma->interruptRegs->channelVector[0] = 0;
        adma->interruptRegs->channelVector[1] = 0;
    }

    // Unmap any I/O ports. Disconnecting from the interrupt will be done automatically by the framework.
    for (UINT i = 0; i < adma->numBars; i++) {
        if (adma->bar[i] != NULL) {
            TraceInfo(DBG_INIT, "Unmapping BAR%d, VA:(%p) Length %ul",
                      i, adma->bar[i], adma->barLength[i]);
            MmUnmapIoSpace(adma->bar[i], adma->barLength[i]);
            adma->bar[i] = NULL;
        }
    }
}



