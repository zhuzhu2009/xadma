/**************************************************************************

    AVStream Simulated Hardware Sample

    Copyright (c) 2001, Microsoft Corporation.

    File:

        hwsim.cpp

    Abstract:
        
        This file is the hardware simulation header.  

        The simulation fakes "DMA" transfers, scatter gather mapping handling, 
        ISR's, etc...  The ISR routine in here will be called when an ISR 
        would be generated by the fake hardware and it will directly call into 
        the device level ISR for more accurate simulation.

    History:

        created 3/9/2001

**************************************************************************/

//#define ALTERA_ARRIA10
#define ALTERA_CYCLONE4

#if defined(ALTERA_ARRIA10)
// a10 pcie dma
#define HW_MAX_DESCRIPTOR_NUM	(128UL)
#define ADMA_DESCRIPTOR_OFFSET	(0x200)
#define ADMA_ONE_DESCRIPTOR_MAX_TRANS_SIZE (1024UL * 1024UL - 4UL)
#define HW_MAX_TRANSFER_SIZE  (HW_MAX_DESCRIPTOR_NUM * PAGE_SIZE)//(ADMA_MAX_DESCRIPTOR_NUM * ADMA_ONE_DESCRIPTOR_MAX_TRANS_SIZE)
#define ADMA_RD_DTS_ADDR		(0x80000000UL)
#define ADMA_WR_DTS_ADDR		(0x80002000UL)
#define ADMA_DIR_REG_OFFSET		(0x100)

#define FRAME_BUFFER_NUM			5
#define FRAME_BUFFER_REG_ADDR(x)	(0x5000 + x * 0x20)

#pragma pack(1)

// a10 pcie dma
/// H2C/C2H SGDMA Registers (H2C: 0x0, C2H:0x100)
typedef struct {
	UINT32 rcStatusDescLo;//0x00
	UINT32 rcStatusDescHi;
	UINT32 epDescFifoLo;
	UINT32 epDescFifoHi;
	UINT32 dmaLastPtr;
	UINT32 tableSize;
	UINT32 control;
	UINT32 reserved1[57];//0xFC
} ADMA_SGDMA_REGS, *PADMA_SGDMA_REGS;

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
} ADMA_DESCRIPTOR, *PADMA_DESCRIPTOR;

/// Result buffer of the streaming DMA operation. 
/// The ADMA IP core writes the result of the DMA transfer to the host memory
typedef struct {//start from RC Read Descriptor Base or RC Write Descriptor Base registers
	UINT32 status[ADMA_MAX_DESCRIPTOR_NUM];
} ADMA_RESULT, *PADMA_RESULT;

#pragma pack()

#elif defined(ALTERA_CYCLONE4)
#define HW_MAX_DESCRIPTOR_NUM				(128UL)
#define HW_MAX_TRANSFER_SIZE  (HW_MAX_DESCRIPTOR_NUM * PAGE_SIZE)

// c4 sgdma dispatcher
#define SGDMA_DESCRIPTOR_REG_OFFSET			(0x4080)
#define SGDMA_CSR_REG_OFFSET				(0x40a0)
#define SGDMA_RESPONSE_REG_OFFSET			(0x40c0)
#define FRAME_BUFFER_REG_ADDR				(0x4040)
#define CLOCK_VIDEO_REG_ADDR				(0x4000)

// masks and offsets for the read and write strides
#define DESCRIPTOR_READ_STRIDE_MASK                      (0xFFFF)
#define DESCRIPTOR_READ_STRIDE_OFFSET                    (0)
#define DESCRIPTOR_WRITE_STRIDE_MASK                     (0xFFFF0000)
#define DESCRIPTOR_WRITE_STRIDE_OFFSET                   (16)

// masks and offsets for the bits in the descriptor control field
#define DESCRIPTOR_CONTROL_TRANSMIT_CHANNEL_MASK         (0xFF)
#define DESCRIPTOR_CONTROL_TRANSMIT_CHANNEL_OFFSET       (0)
#define DESCRIPTOR_CONTROL_GENERATE_SOP_MASK             (1UL<<8)
#define DESCRIPTOR_CONTROL_GENERATE_SOP_OFFSET           (8)
#define DESCRIPTOR_CONTROL_GENERATE_EOP_MASK             (1UL<<9)
#define DESCRIPTOR_CONTROL_GENERATE_EOP_OFFSET           (9)
#define DESCRIPTOR_CONTROL_PARK_READS_MASK               (1UL<<10)
#define DESCRIPTOR_CONTROL_PARK_READS_OFFSET             (10)
#define DESCRIPTOR_CONTROL_PARK_WRITES_MASK              (1UL<<11)
#define DESCRIPTOR_CONTROL_PARK_WRITES_OFFSET            (11)
#define DESCRIPTOR_CONTROL_END_ON_EOP_MASK               (1UL<<12)
#define DESCRIPTOR_CONTROL_END_ON_EOP_OFFSET             (12)
#define DESCRIPTOR_CONTROL_END_ON_EOP_LEN_MASK           (1UL<<13)
#define DESCRIPTOR_CONTROL_END_ON_EOP_LEN_OFFSET         (13)
#define DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_MASK    (1UL<<14)
#define DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_OFFSET  (14)
#define DESCRIPTOR_CONTROL_EARLY_TERMINATION_IRQ_MASK    (1UL<<15)
#define DESCRIPTOR_CONTROL_EARLY_TERMINATION_IRQ_OFFSET  (15)
#define DESCRIPTOR_CONTROL_ERROR_IRQ_MASK                (0xFF<<16)  // the read master will use this as the transmit error, the dispatcher will use this to generate an interrupt if any of the error bits are asserted by the write master
#define DESCRIPTOR_CONTROL_ERROR_IRQ_OFFSET              (16)
#define DESCRIPTOR_CONTROL_EARLY_DONE_ENABLE_MASK        (1UL<<24)
#define DESCRIPTOR_CONTROL_EARLY_DONE_ENABLE_OFFSET      (24)
#define DESCRIPTOR_CONTROL_GO_MASK                       (1UL<<31)  // at a minimum you always have to write '1' to this bit as it commits the descriptor to the dispatcher
#define DESCRIPTOR_CONTROL_GO_OFFSET                     (31)

// masks for the status register bits
#define CSR_BUSY_MASK                           (1)
#define CSR_BUSY_OFFSET                         (0)
#define CSR_DESCRIPTOR_BUFFER_EMPTY_MASK        (1<<1)
#define CSR_DESCRIPTOR_BUFFER_EMPTY_OFFSET      (1)
#define CSR_DESCRIPTOR_BUFFER_FULL_MASK         (1<<2)
#define CSR_DESCRIPTOR_BUFFER_FULL_OFFSET       (2)
#define CSR_RESPONSE_BUFFER_EMPTY_MASK          (1<<3)
#define CSR_RESPONSE_BUFFER_EMPTY_OFFSET        (3)
#define CSR_RESPONSE_BUFFER_FULL_MASK           (1<<4)
#define CSR_RESPONSE_BUFFER_FULL_OFFSET         (4)
#define CSR_STOP_STATE_MASK                     (1<<5)
#define CSR_STOP_STATE_OFFSET                   (5)
#define CSR_RESET_STATE_MASK                    (1<<6)
#define CSR_RESET_STATE_OFFSET                  (6)
#define CSR_STOPPED_ON_ERROR_MASK               (1<<7)
#define CSR_STOPPED_ON_ERROR_OFFSET             (7)
#define CSR_STOPPED_ON_EARLY_TERMINATION_MASK   (1<<8)
#define CSR_STOPPED_ON_EARLY_TERMINATION_OFFSET (8)
#define CSR_IRQ_SET_MASK                        (1<<9)
#define CSR_IRQ_SET_OFFSET                      (9)

// masks for the control register bits
#define CSR_STOP_MASK                           (1)
#define CSR_STOP_OFFSET                         (0)
#define CSR_RESET_MASK                          (1<<1)
#define CSR_RESET_OFFSET                        (1)
#define CSR_STOP_ON_ERROR_MASK                  (1<<2)
#define CSR_STOP_ON_ERROR_OFFSET                (2)
#define CSR_STOP_ON_EARLY_TERMINATION_MASK      (1<<3)
#define CSR_STOP_ON_EARLY_TERMINATION_OFFSET    (3)
#define CSR_GLOBAL_INTERRUPT_MASK               (1<<4)
#define CSR_GLOBAL_INTERRUPT_OFFSET             (4)
#define CSR_STOP_DESCRIPTORS_MASK               (1<<5)
#define CSR_STOP_DESCRIPTORS_OFFSET             (5)

#define CONTROL_GO_MASK			(1)

#define CLOCK_VIDEO_STATUS_OVERFLOW_MASK		(1<<9)

/*
Descriptor formats:

Standard Format:

Offset         |             3                 2                 1                   0
--------------------------------------------------------------------------------------
0x0           |                              Read Address[31..0]
0x4           |                              Write Address[31..0]
0x8           |                                Length[31..0]
0xC           |                                Control[31..0]

Extended Format:

Offset         |               3                           2                           1                          0
-------------------------------------------------------------------------------------------------------------------
0x0           |                                                  Read Address[31..0]
0x4           |                                                  Write Address[31..0]
0x8           |                                                    Length[31..0]
0xC           |      Write Burst Count[7..0]  |  Read Burst Count[7..0]  |           Sequence Number[15..0]
0x10          |                      Write Stride[15..0]                 |              Read Stride[15..0]
0x14          |                                                      <reserved>
0x18          |                                                      <reserved>
0x1C          |                                                     Control[31..0]

Note:  The control register moves from offset 0xC to 0x1C depending on the format used

*/

#pragma pack(1)
// c4 sgdma dispatcher, for c4, descriptor is located on ep memory not in host memory
// use this structure if you haven't enabled the enhanced features
typedef struct {
	UINT32 readAddress;
	UINT32 writeAddress;
	UINT32 transferLength;
	UINT32 control;
} SGDMA_STANDARD_DESCRIPTOR, *PSGDMA_STANDARD_DESCRIPTOR;

// use ths structure if you have enabled the enhanced features (only the elements enabled in hardware will be used)
typedef struct {
	UINT32 readAddress;
	UINT32 writeAddress;
	UINT32 transferLength;
	UINT32 snAndRwBurst;
	UINT32 rwStride;
	UINT32 readAddressHi;
	UINT32 writeAddressHi;
	UINT32 control;
} SGDMA_EXTEND_DESCRIPTOR, *PSGDMA_EXTEND_DESCRIPTOR;

// this struct should only be used if response information is enabled
typedef struct {
	UINT32 actualBytesTransferred;
	UINT32 status;
} SGDMA_RESPONSE, *PSGDMA_RESPONSE;
/*
Enhanced features off:

Bytes     Access Type     Description
-----     -----------     -----------
0-3       R/Clr           Status(1)
4-7       R/W             Control(2)
8-12      R               Descriptor Fill Level(write fill level[15:0], read fill level[15:0])
13-15     R               Response Fill Level[15:0]
16-31     N/A             <Reserved>


Enhanced features on:

Bytes     Access Type     Description
-----     -----------     -----------
0-3       R/Clr           Status(1)
4-7       R/W             Control(2)
8-12      R               Descriptor Fill Level (write fill level[15:0], read fill level[15:0])
13-15     R               Response Fill Level[15:0]
16-20     R               Sequence Number (write sequence number[15:0], read sequence number[15:0])
21-31     N/A             <Reserved>

(1)  Writing a '1' to the interrupt bit of the status register clears the interrupt bit (when applicable), all other bits are unaffected by writes
(2)  Writing to the software reset bit will clear the entire register (as well as all the registers for the entire SGDMA)

Status Register:

Bits      Description
----      -----------
0         Busy
1         Descriptor Buffer Empty
2         Descriptor Buffer Full
3         Response Buffer Empty
4         Response Buffer Full
5         Stop State
6         Reset State
7         Stopped on Error
8         Stopped on Early Termination
9         IRQ
10-31     <Reserved>

Control Register:

Bits      Description
----      -----------
0         Stop (will also be set if a stop on error/early termination condition occurs)
1         Software Reset
2         Stop on Error
3         Stop on Early Termination
4         Global Interrupt Enable Mask
5         Stop dispatcher (stops the dispatcher from issuing more read/write commands)
6-31      <Reserved>
*/
// use this structure if you haven't enabled the enhanced features
typedef struct {
	UINT32 status;
	UINT32 control;
	UINT32 rwFillLevel;
	UINT32 squenceNum;
	UINT32 reserved[3];
} SGDMA_CSR, *PSGDMA_CSR;

/// Altera VIP Frame Buffer II IP Registers
typedef struct {
	UINT32 control;//0x00
	UINT32 status;
	UINT32 interrupt;
	UINT32 frameCount;
	UINT32 dropRepeatCount;
	UINT32 frameInfo;
	UINT32 frameStartAddr;
	UINT32 frameReader;//0xFC
	UINT32 misc;
	UINT32 lockEn;
	UINT32 inputFrameRate;
	UINT32 outputFrameRate;
} FRAME_BUFFER_REGS, *PFRAME_BUFFER_REGS;

/// Altera Clock Video Input IP Registers
typedef struct {
	UINT32 control;//0x00
	UINT32 status;
	UINT32 interrupt;
	UINT32 userWord;
	UINT32 activeSampleCnt;
	UINT32 f0ActiveLineCnt;
	UINT32 f1ActiveLineCnt;
	UINT32 totalSampleCnt;//0xFC
	UINT32 f0TotalLineCnt;
	UINT32 f1TotalLineCnt;
	UINT32 standard;
	UINT32 reserved;
	UINT32 colorPattern;
	UINT32 ancillaryPacket;
} CLOCK_VIDEO_REGS, *PCLOCK_VIDEO_REGS;

#pragma pack()
#else
#error "Please define FPGA type"
#endif

//
// SCATTER_GATHER_MAPPINGS_MAX:
//
// The maximum number of entries in the hardware's scatter/gather list.  I
// am making this so large for a few reasons:
//
//     1) we're faking this with uncompressed surfaces -- 
//            these are large buffers which will map to a lot of s/g entries
//     2) the fake hardware implementation requires at least one frame's
//            worth of s/g entries to generate a frame
//
#define SCATTER_GATHER_MAPPINGS_MAX HW_MAX_DESCRIPTOR_NUM //128

//
// SCATTER_GATHER_ENTRY:
//
// This structure is used to keep the scatter gather table for the fake
// hardware as a doubly linked list.
//
typedef struct _SCATTER_GATHER_ENTRY {

    LIST_ENTRY ListEntry;
    PKSSTREAM_POINTER CloneEntry;
    PUCHAR Virtual;
    ULONG ByteCount;

} SCATTER_GATHER_ENTRY, *PSCATTER_GATHER_ENTRY;

//
// CHardwareSimulation:
//
// The hardware simulation class.
//
class CHardwareSimulation {

private:

    //
    // The image synthesizer.  This is a piece of code which actually draws
    // the requested images.
    //
    CImageSynthesizer *m_ImageSynth;

    //
    // The synthesis buffer.  This is a private buffer we use to generate the
    // capture image in.  The fake "scatter / gather" mappings are filled
    // in from this buffer during each interrupt.
    //
    PUCHAR m_SynthesisBuffer;

    //
    // Key information regarding the frames we generate.
    //
    LONGLONG m_TimePerFrame;
    ULONG m_Width;
    ULONG m_Height;
    ULONG m_ImageSize;

    //
    // Scatter gather mappings for the simulated hardware.
    //
    KSPIN_LOCK m_ListLock;
    LIST_ENTRY m_ScatterGatherMappings;

    //
    // Lookaside for memory for the scatter / gather entries on the scatter /
    // gather list.
    //
    NPAGED_LOOKASIDE_LIST m_ScatterGatherLookaside;

    //
    // The current state of the fake hardware.
    //
    HARDWARE_STATE m_HardwareState;

    //
    // The pause / stop hardware flag and event.
    //
    BOOLEAN m_StopHardware;
    KEVENT m_HardwareEvent;

    //
    // Maximum number of scatter / gather mappins in the s/g table of the
    // fake hardware.
    //
    ULONG m_ScatterGatherMappingsMax;

    //
    // Number of scatter / gather mappings that have been completed (total)
    // since the start of the hardware or any reset.
    //
    ULONG m_NumMappingsCompleted;

    //
    // Number of scatter / gather mappings that are queued for this hardware.
    //
    ULONG m_ScatterGatherMappingsQueued;
    ULONG m_ScatterGatherBytesQueued;

    //
    // Number of frames skipped due to lack of scatter / gather mappings.
    //
    ULONG m_NumFramesSkipped;

    //
    // The "Interrupt Time".  Number of "fake" interrupts that have occurred
    // since the hardware was started.
    // 
    ULONG m_InterruptTime;

    //
    // The system time at start.
    //
    LARGE_INTEGER m_StartTime;
    
    //
    // The DPC used to "fake" ISR
    //
    KDPC m_IsrFakeDpc;
    KTIMER m_IsrTimer;

    //
    // The hardware sink that will be used for interrupt notifications.
    //
    IHardwareSink *m_HardwareSink;

    //
    // FillScatterGatherBuffers():
    //
    // This is called by the hardware simulation to fill a series of scatter /
    // gather buffers with synthesized data.
    //
    NTSTATUS
    FillScatterGatherBuffers (
        );

public:
#if defined(ALTERA_ARRIA10)
	// a10 pcie dma
	PADMA_SGDMA_REGS m_AdmaRdSgdmaReg;
	PADMA_SGDMA_REGS m_AdmaWrSgdmaReg;
	PADMA_DESCRIPTOR m_AdmaRdDescriptor;
	PADMA_DESCRIPTOR m_AdmaWrDescriptor;
	PADMA_RESULT m_AdmaRdResult;
	PADMA_RESULT m_AdmaWrResult;
	PFRAME_BUFFER_REGS m_FrameBufferReg[FRAME_BUFFER_NUM];
#elif defined(ALTERA_CYCLONE4)
	// c4 sgdma dispatcher
	PSGDMA_STANDARD_DESCRIPTOR m_SgdmaStandardDescriptor;
	PSGDMA_EXTEND_DESCRIPTOR m_SgdmaExtendDescriptor;
	PSGDMA_RESPONSE m_SgdmaResponse;
	PSGDMA_CSR m_SgdmaCsr;
	PFRAME_BUFFER_REGS m_FrameBufferReg;
	PCLOCK_VIDEO_REGS m_ClockVideoReg;
#else
#error "Please define FPGA type"
#endif

    LONG GetSkippedFrameCount()
    {
        return InterlockedExchange((LONG*)&this->m_NumFramesSkipped, this->m_NumFramesSkipped);
    }
    //
    // CHardwareSimulation():
    //
    // The hardware simulation constructor.  Since the new operator will
    // have zeroed the memory, only initialize non-NULL, non-0 fields. 
    //
    CHardwareSimulation (
        IN IHardwareSink *HardwareSink
        );

    //
    // ~CHardwareSimulation():
    //
    // The hardware simulation destructor.
    //
    ~CHardwareSimulation (
        )
    {
    }

    //
    // Cleanup():
    //
    // This is the free callback for the bagged hardware sim.  Not providing
    // one will call ExFreePool, which is not what we want for a constructed
    // C++ object.  This simply deletes the simulation.
    //
    static
    void
    Cleanup (
        IN CHardwareSimulation *HwSim
        )
    {
        delete HwSim;
    }

    //
    // FakeHardware():
    //
    // Called from the simulated interrupt.  First we fake the hardware's
    // actions (at DPC) then we call the "Interrupt service routine" on
    // the hardware sink.
    //
    void
    FakeHardware (
        );

    //
    // Start():
    //
    // "Start" the fake hardware.  This will start issuing interrupts and 
    // DPC's. 
    //
    // The frame rate, image size, and a synthesizer must be provided.
    //
    NTSTATUS
    Start (
        CImageSynthesizer *ImageSynth,
        IN LONGLONG TimePerFrame,
        IN ULONG Width,
        IN ULONG Height,
        IN ULONG ImageSize
        );

    //
    // Pause():
    //
    // "Pause" or "unpause" the fake hardware.  This will stop issuing 
    // interrupts or DPC's on a pause and restart them on an unpause.  Note
    // that this will not reset counters as a Stop() would.
    //
    NTSTATUS
    Pause (
        IN BOOLEAN Pausing
        );

    //
    // Stop():
    //
    // "Stop" the fake hardware.  This will stop issuing interrupts and
    // DPC's.
    //
    NTSTATUS
    Stop (
        );

    //
    // ProgramScatterGatherMappings():
    //
    // Program a series of scatter gather mappings into the fake hardware.
    //
    ULONG
    ProgramScatterGatherMappings (
        IN PKSSTREAM_POINTER Clone,
        IN PUCHAR *Buffer,
        IN PKSMAPPING Mappings,
        IN ULONG MappingsCount,
        IN ULONG MappingStride
        );

    //
    // Initialize():
    //
    // Initialize a piece of simulated hardware.
    //
    static 
    CHardwareSimulation *
    Initialize (
        IN KSOBJECT_BAG Bag,
        IN IHardwareSink *HardwareSink
        );

    //
    // ReadNumberOfMappingsCompleted():
    //
    // Read the number of mappings completed since the last hardware reset.
    //
    ULONG
    ReadNumberOfMappingsCompleted (
        );

};

