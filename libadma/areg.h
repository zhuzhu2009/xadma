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
    UINT32 identifier;
    UINT32 userIntEnable;
    UINT32 userIntEnableW1S;
    UINT32 userIntEnableW1C;
    UINT32 channelIntEnable;
    UINT32 channelIntEnableW1S;
    UINT32 channelIntEnableW1C;
    UINT32 reserved_1[9];

    UINT32 userIntRequest;
    UINT32 channelIntRequest;
    UINT32 userIntPending;
    UINT32 channelIntPending;
    UINT32 reserved_2[12];

    UINT32 userVector[4];
    UINT32 reserved_3[4];
    UINT32 channelVector[2];

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

#pragma pack()


#define EXPECT(EXP) ASSERTMSG(#EXP, EXP)
#define ENSURES(EXP) ASSERTMSG(#EXP, EXP)