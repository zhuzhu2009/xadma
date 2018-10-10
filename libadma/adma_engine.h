/*
* ADMA Scatter-Gather DMA Engines
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
#include <ntintsafe.h>
#include <wdf.h>
#include "areg.h"
#include "adma_public.h"

// ========================= constants ============================================================

#define ADMA_MAX_NUM_CHANNELS   (1)
#define ADMA_NUM_DIRECTIONS     (2)
#define ADMA_MAX_CHAN_IRQ       (ADMA_NUM_DIRECTIONS * ADMA_MAX_NUM_CHANNELS)
#define ADMA_RING_NUM_BLOCKS    (258U)
#define ADMA_RING_BLOCK_SIZE    (PAGE_SIZE)
//#define ADMA_MAX_TRANSFER_SIZE  (8UL * 1024UL * 1024UL)
#define ADMA_MAX_DESCRIPTOR_NUM	(128UL)
#define ADMA_DESCRIPTOR_OFFSET	(0x200)
#define ADMA_ONE_DESCRIPTOR_MAX_TRANS_SIZE (1024UL * 1024UL - 4UL)
#define ADMA_MAX_TRANSFER_SIZE  (ADMA_MAX_DESCRIPTOR_NUM * PAGE_SIZE)//(ADMA_MAX_DESCRIPTOR_NUM * ADMA_ONE_DESCRIPTOR_MAX_TRANS_SIZE)

// ========================= forward declarations =================================================

struct ADMA_DEVICE_T; 
typedef struct ADMA_DEVICE_T ADMA_DEVICE;
struct ADMA_ENGINE_T;

// ========================= type declarations ====================================================

/// Direction of the DMA transfer/engine
typedef enum DirToDev_t {
    H2C = 0, // Host-to-Card - write to device
    C2H = 1  // Card-to-Host - read from device
} DirToDev;

/// Ring buffer abstraction for streaming DMA
typedef struct ADMA_RING_T {
    WDFCOMMONBUFFER results;
    PMDL mdl[ADMA_RING_NUM_BLOCKS]; // memory descriptor list - host side
    CHAR dmaTransferContext[DMA_TRANSFER_CONTEXT_SIZE_V1];
    UINT head;
    UINT tail;
    WDFSPINLOCK lock;
    KEVENT completionSignal;
}ADMA_RING, *PADMA_RING;

/// engine specific work to perform after dma transfer completion is detected
typedef VOID(*PFN_ADMA_ENGINE_WORK)(IN struct ADMA_ENGINE_T *engine);

/// Engine address mode. 
/// Determines how the DMA engine interprets the device address (destination address on H2C and 
/// source address on C2H).
/// When AddressMode_Contiguous is chosen, the device address only needs to be set for the first 
/// descriptor and the engine assumes that all subsequent descriptors device addresses follow
/// sequentially. 
/// When AddressMode_Fixed is selected, the device addresses of each descriptor must be explicitly
/// set.
typedef enum AddressMode_T {
    AddressMode_Contiguous,  // incremental
    AddressMode_Fixed,       // non-incremental
} AddressMode;

typedef enum EngineType_t {
    EngineType_MM,      // Memory Mapped
    EngineType_ST,      // Streaming
} EngineType;

/// DMA engine abstraction
typedef struct ADMA_ENGINE_T {

    ADMA_DEVICE *parentDevice; // the adma device to which this engine belongs

    // register access
    volatile ADMA_ENGINE_REGS *regs; // control regs
    volatile ADMA_SGDMA_REGS *sgdma;

	volatile ADMA_MODULAR_SGDMA_CSR *modSgdmaCsr;
	volatile ADMA_MODULAR_SGDMA_STANDARD_DESCRIPTOR *modSgdmaStdDes;
	volatile ADMA_MODULAR_SGDMA_EXTEND_DESCRIPTOR *modSgdmaExtDes;
	volatile ADMA_MODULAR_SGDMA_RESPONSE *modSgdmaResponse;

    // engine configuration
    UINT32 irqBitMask;
    UINT32 alignAddr;
    UINT32 alignLength;
    UINT32 alignAddrBits;
    DWORD channel;
    DirToDev dir;               // data flow direction (H2C or C2H)
    BOOLEAN enabled;
    EngineType type;            // MemoryMapped or Streaming
    AddressMode addressMode;    // incremental (contiguous) or non-incremental (fixed)

    // dma transfer related
    WDFCOMMONBUFFER descBuffer;
    WDFDMATRANSACTION dmaTransaction;
    PFN_ADMA_ENGINE_WORK work; // engine work for interrupt processing

    // specific to streaming interface
    ADMA_RING ring;

    // specific to poll mode
    ULONG poll;
    WDFCOMMONBUFFER pollWbBuffer; // buffer for holding poll mode descriptor writeback data
    ULONG numDescriptors; // keep count of descriptors in transfer for poll mode
} ADMA_ENGINE;

#pragma pack(1)

/// \brief Descriptor for a single contiguous memory block transfer.
///
/// Multiple descriptors are linked a 'next' pointer. An additional extra adjacent number gives the 
/// amount of subsequent contiguous descriptors. The descriptors are in root complex memory, and the
/// bytes in the 32-bit words must be in little-endian byte ordering.
typedef struct adma_descriptor_t {//offset 0x200 from RC Read Descriptor Base or RC Write Descriptor Base registers
    UINT32 srcAddrLo; // source address (low 32-bit)
    UINT32 srcAddrHi; // source address (high 32-bit)
    UINT32 dstAddrLo; // destination address (low 32-bit)
    UINT32 dstAddrHi; // destination address (high 32-bit)
                      // next descriptor in the single-linked list of descriptors, 
                      // this is the bus address of the next descriptor in the root complex memory.
	UINT32 control;
	UINT32 reserved[3];//padding total 32bytes
} ADMA_DESCRIPTOR;

/// Result buffer of the streaming DMA operation. 
/// The ADMA IP core writes the result of the DMA transfer to the host memory
typedef struct {//start from RC Read Descriptor Base or RC Write Descriptor Base registers
    UINT32 status[ADMA_MAX_DESCRIPTOR_NUM];
} ADMA_RESULT;

/// \brief Structure for polled mode descriptor writeback
///
/// ADMA IP core writes number of completed descriptors to this memory, which the driver can then
/// poll to detect transfer completion
typedef struct {
    UINT32 completedDescCount;
    UINT32 reserved_1[7];
} ADMA_POLL_WB;

#pragma pack()

// ========================= function declarations ================================================

struct ADMA_DEVICE_T;
typedef struct ADMA_DEVICE_T* PADMA_DEVICE;

/// Initialize an ADMA_ENGINE for each engine configured in HW
NTSTATUS ProbeEngines(IN PADMA_DEVICE adma);

// Assign interrupt add by zhuce
NTSTATUS EnginesAssignInterrupt(IN PADMA_DEVICE adma);

/// Start the DMA engine
/// The transfer descriptors should be initialized and bound to HW before calling this function
VOID EngineStart(IN ADMA_ENGINE *engine);

/// Stop the DMA engine
VOID EngineStop(IN ADMA_ENGINE *engine);

/// Configure the streaming ring buffer and start the cyclic DMA transfer
VOID EngineRingSetup(IN ADMA_ENGINE *engine);

/// Reset the streaming ring buffer and stop the cyclic DMA transfer
VOID EngineRingTeardown(IN ADMA_ENGINE *engine);

/// Poll the write-back buffer for DMA transfer completion
NTSTATUS EnginePollTransfer(IN ADMA_ENGINE* engine);

/// Poll the write-back buffer for DMA transfer completion
NTSTATUS EnginePollRing(IN ADMA_ENGINE* engine);

/// enable the engines interrupt
VOID EngineEnableInterrupt(IN ADMA_ENGINE* engine);

/// disable the engines interrupt
VOID EngineDisableInterrupt(IN ADMA_ENGINE* engine);

/// Arm the performance counters of the ADMA engine
/// Automatically stops when a dma descriptor with the stop bit is completed
VOID EngineStartPerf(IN ADMA_ENGINE* engine);

/// Get the performance counters 
VOID EngineGetPerf(IN ADMA_ENGINE* engine, OUT ADMA_PERF_DATA* perfData);

/// Stringify the Engine direction (H2C/C2H)
char* DirectionToString(DirToDev dir);

/// Copy data from the ring buffer directly into a WDFMEMORY object
NTSTATUS EngineRingCopyBytesToMemory(IN ADMA_ENGINE *engine, WDFMEMORY outputMem,
                                     size_t length, LARGE_INTEGER timeout, size_t* bytesRead);