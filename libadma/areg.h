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

// ========================= declarations =================================================

#define LIMIT_TO_64(x)      (x & 0xFFFFFFFFFFFFFFFFULL)
#define LIMIT_TO_32(x)      (x & 0x00000000FFFFFFFFUL)
#define LIMIT_TO_16(x)      (x & 0x000000000000FFFFUL)

#define CLR_MASK(x, mask)   ((x) &= ~(mask));
#define SET_MASK(x, mask)   ((x) |=  (mask));

#define BIT_N(n)            (1 << n)

//  block identification bits
#define ADMA_ID_MASK        (0xFFF00000UL)
#define ADMA_ID             (0x1FC00000UL)
#define BLOCK_ID_MASK       (0xFFFF0000UL)
#define IRQ_BLOCK_ID        (ADMA_ID | 0x20000UL)
#define CONFIG_BLOCK_ID     (ADMA_ID | 0x30000UL)
#define ADMA_ID_ST_BIT      (BIT_N(15))

// Block module offsets
#define BLOCK_OFFSET        (0x100)//(0x1000UL)
#define IRQ_BLOCK_OFFSET    (2 * BLOCK_OFFSET)
#define CONFIG_BLOCK_OFFSET 0 // change by zhuce (3 * BLOCK_OFFSET)
#define SGDMA_BLOCK_OFFSET  (0x0)//(4 * BLOCK_OFFSET)
#define SGDMA_COMMON_BLOCK_OFFSET (6 * BLOCK_OFFSET)
#define ENGINE_OFFSET       (0x100UL)//for adma current no effect//(0x100UL)


#define A2P_TRANS_TBL_OFFSET				(0x1000)
#define ADMA_IRQ_REGS_OFFSET				(0x40)
//bits of the SGDMA engine control register
#define ADMA_CTRL_RUN_BIT                   (BIT_N(0))
#define ADMA_CTRL_IE_DESC_STOPPED           (BIT_N(1))
#define ADMA_CTRL_IE_DESC_COMPLETED         (BIT_N(2))
#define ADMA_CTRL_IE_ALIGNMENT_MISMATCH     (BIT_N(3))
#define ADMA_CTRL_IE_MAGIC_STOPPED          (BIT_N(4))
#define ADMA_CTRL_IE_INVALID_LENGTH         (BIT_N(5))
#define ADMA_CTRL_IE_IDLE_STOPPED           (BIT_N(6))
#define ADMA_CTRL_IE_READ_ERROR             (0x1f << 9)
#define ADMA_CTRL_IE_WRITE_ERROR            (0x1f << 14)
#define ADMA_CTRL_IE_DESCRIPTOR_ERROR       (0x1f << 19)
#define ADMA_CTRL_NON_INCR_ADDR             (BIT_N(25))
#define ADMA_CTRL_POLL_MODE                 (BIT_N(26))
#define ADMA_CTRL_RST                       (BIT_N(31))

#define ADMA_CTRL_IE_ALL (ADMA_CTRL_IE_DESC_STOPPED | ADMA_CTRL_IE_DESC_COMPLETED \
                          | ADMA_CTRL_IE_ALIGNMENT_MISMATCH | ADMA_CTRL_IE_MAGIC_STOPPED \
                          | ADMA_CTRL_IE_INVALID_LENGTH | ADMA_CTRL_IE_READ_ERROR | ADMA_CTRL_IE_WRITE_ERROR\
                          | ADMA_CTRL_IE_DESCRIPTOR_ERROR)

// bits of the SGDMA engine status register
#define ADMA_BUSY_BIT                       (BIT_N(0))
#define ADMA_DESCRIPTOR_STOPPED_BIT         (BIT_N(1))
#define ADMA_DESCRIPTOR_COMPLETED_BIT       (BIT_N(2))
#define ADMA_ALIGN_MISMATCH_BIT             (BIT_N(3))
#define ADMA_MAGIC_STOPPED_BIT              (BIT_N(4))
#define ADMA_FETCH_STOPPED_BIT              (BIT_N(5))
#define ADMA_IDLE_STOPPED_BIT               (BIT_N(6))
#define ADMA_STAT_READ_ERROR                (0x1fUL * BIT_N(9))
#define ADMA_STAT_DESCRIPTOR_ERROR          (0x1fUL * BIT_N(19))

#define ADMA_ENGINE_STOPPED_OK              (0UL)
#define ADMA_STAT_EXPECTED_ZERO             (ADMA_BUSY_BIT | ADMA_MAGIC_STOPPED_BIT | \
                                             ADMA_FETCH_STOPPED_BIT | ADMA_ALIGN_MISMATCH_BIT | \
                                             ADMA_STAT_READ_ERROR | ADMA_STAT_DESCRIPTOR_ERROR)


// bits of the SGDMA descriptor control field
#define ADMA_DESC_STOP_BIT                  (BIT_N(0))
#define ADMA_DESC_COMPLETED_BIT             (BIT_N(1))
#define ADMA_DESC_EOP_BIT                   (BIT_N(4))

#define ADMA_RESULT_EOP_BIT                 (BIT_N(0))

// Engine performance control register bits
#define ADMA_PERF_RUN                       BIT_N(0)
#define ADMA_PERF_CLEAR                     BIT_N(1)
#define ADMA_PERF_AUTO                      BIT_N(2)

#define ADMA_RD_DTS_ADDR					(0x80000000UL)//add by zc
#define ADMA_WR_DTS_ADDR					(0x80002000UL)//add by zc

//Cyclone IV
#define MODULAR_SGDMA_DESCRIPTOR_REG_OFFSET			(0x6000020)
#define MODULAR_SGDMA_CSR_REG_OFFSET				(0x6000000)
#define MODULAR_SGDMA_RESPONSE_REG_OFFSET			(0x40c0)
#define FRAME_BUFFER_REG_ADDR						(0x4040)
#define CLOCK_VIDEO_REG_ADDR						(0x4000)

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

#pragma pack(1)

/// H2C/C2H Channel Registers (H2C: 0x0, C2H: 0x1000)
/// To avoid Read-Modify-Write errors in concurrent systems, some registers can be set via 
/// additional mirror registers.
/// - W1S (write 1 to set)  - only the bits set will be set in the underlying register. all other
///                           bits are ignored.
/// - W1C (write 1 to clear)- only the bits set will be cleared in the underlying register. all other
///                           bits are ignored.
typedef struct {
    UINT32 identifier;
    UINT32 control;
    UINT32 controlW1S; // write 1 to set
    UINT32 controlW1C; // write 1 to clear
    UINT32 reserved_1[12];

    UINT32 status; // status register { 6'h10, 2'b0 } is 0x40
    UINT32 statusRC;
    UINT32 completedDescCount; // number of completed descriptors
    UINT32 alignments;
    UINT32 reserved_2[14];

    UINT32 pollModeWbLo;
    UINT32 pollModeWbHi;
    UINT32 intEnableMask;
    UINT32 intEnableMaskW1S;
    UINT32 intEnableMaskW1C;
    UINT32 reserved_3[9];

    UINT32 perfCtrl;
    UINT32 perfCycLo;
    UINT32 perfCycHi;
    UINT32 perfDatLo;
    UINT32 perfDatHi;
    UINT32 perfPndLo;
    UINT32 perfPndHi;

} ADMA_ENGINE_REGS;

/// IRQ Block Registers (0x2000)
typedef struct {
	UINT32 status;
	UINT32 reserved_1[3];
	UINT32 enable;
	UINT32 reserved_2[3];
} ADMA_IRQ_REGS;

/// Config Block Registers (0x3000)
typedef struct {
    UINT32 identifier;
    UINT32 busDev;
    UINT32 pcieMPS;
    UINT32 pcieMRRS;
    UINT32 systemId;
    UINT32 msiEnable;
    UINT32 pcieWidth;
    UINT32 pcieControl; // 0x1C
    UINT32 reserved_1[8];

    UINT32 userMPS;     // 0x40
    UINT32 userMRRS;
    UINT32 reserved_2[6];
        
    UINT32 writeFlushTimeout; // 0x60
} ADMA_CONFIG_REGS;

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
	/*
	UINT32 wrRcStatusDescLo;//0x100
	UINT32 wrRcStatusDescHi;
	UINT32 wrEpDescFifoLo;
	UINT32 wrEpDescFifoHi;
	UINT32 wrDmaLastPtr;
	UINT32 wrTableSize;
	UINT32 wrControl;
	UINT32 reserved2[57];//0x1FC
	*/
} ADMA_SGDMA_REGS, *PADMA_SGDMA_REGS;

/// SGDMA Common Registers (0x6000)
typedef struct {
    UINT32 identifier;          // 0x00
    UINT32 reserved_1[3];
    UINT32 control;             // 0x10
    UINT32 controlW1S;          // 0x14
    UINT32 controlW1C;          // 0x18
    UINT32 reserved_2;
    UINT32 creditModeEnable;    // 0x20
    UINT32 creditModeEnableW1S; // 0x24
    UINT32 creditModeEnableW1C; // 0x28
} ADMA_SGDMA_COMMON_REGS, *PADMA_SGDMA_COMMON_REGS;

typedef struct {
	UINT32 a2pAddrLo0;
	UINT32 a2pAddrHi0;
	UINT32 a2pAddrLo1;
	UINT32 a2pAddrHi1;
} ADMA_A2P_TRANS_TBL, *PADMA_A2P_TRANS_TBL;

// c4 sgdma dispatcher, for c4, descriptor is located on ep memory not in host memory
// use this structure if you haven't enabled the enhanced features
typedef struct {
	UINT32 readAddress;
	UINT32 writeAddress;
	UINT32 transferLength;
	UINT32 control;
} ADMA_MODULAR_SGDMA_STANDARD_DESCRIPTOR, *PADMA_MODULAR_SGDMA_STANDARD_DESCRIPTOR;

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
} ADMA_MODULAR_SGDMA_EXTEND_DESCRIPTOR, *PADMA_MODULAR_SGDMA_EXTEND_DESCRIPTOR;

// this struct should only be used if response information is enabled
typedef struct {
	UINT32 actualBytesTransferred;
	UINT32 status;
} ADMA_MODULAR_SGDMA_RESPONSE, *PADMA_MODULAR_SGDMA_RESPONSE;
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
} ADMA_MODULAR_SGDMA_CSR, *PADMA_MODULAR_SGDMA_CSR;

#pragma pack()


#define EXPECT(EXP) ASSERTMSG(#EXP, EXP)
#define ENSURES(EXP) ASSERTMSG(#EXP, EXP)