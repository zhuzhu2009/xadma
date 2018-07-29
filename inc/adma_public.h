/*
* ADMA Driver public API
* ===============================
*
* Copyright 2017 Xilinx Inc.
* Copyright 2010-2012 Sidebranch
* Copyright 2010-2012 Leon Woestenberg <leon@sidebranch.com>
*
* Maintainer:
* -----------
* Alexander Hornburg <alexander.hornburg@xilinx.com>
*
* Description:
* ------------
* This is a sample driver for the Xilinx Inc. 'DMA/Bridge Subsystem for PCIe v3.0' (ADMA) IP.
*
*/

#ifndef __ADMA_WINDOWS_H__
#define __ADMA_WINDOWS_H__

//use xdma as public
// 74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d
DEFINE_GUID(GUID_DEVINTERFACE_ADMA, //==GUID_DEVINTERFACE_XDMA
            0x74c7e4a9, 0x6d5d, 0x4a70, 0xbc, 0x0d, 0x20, 0x69, 0x1d, 0xff, 0x9e, 0x9d);


#define	ADMA_FILE_H2C_0		L"\\ah2c_0"
#define	ADMA_FILE_C2H_0		L"\\ac2h_0"
#define	ADMA_FILE_USER		L"\\user"
#define	ADMA_FILE_CONTROL	L"\\control"

#define ADMA_IOCTL_CODE_OFFSET		(0x20)//offset to xdma
#define ADMA_IOCTL(index) CTL_CODE(FILE_DEVICE_UNKNOWN, index + ADMA_IOCTL_CODE_OFFSET, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_ADMA_GET_VERSION  ADMA_IOCTL(0x0)
#define IOCTL_ADMA_PERF_START   ADMA_IOCTL(0x1)
#define IOCTL_ADMA_PERF_STOP    ADMA_IOCTL(0x2)
#define IOCTL_ADMA_PERF_GET     ADMA_IOCTL(0x3)
#define IOCTL_ADMA_ADDRMODE_GET ADMA_IOCTL(0x4)
#define IOCTL_ADMA_ADDRMODE_SET ADMA_IOCTL(0x5)

// structure for IOCTL_ADMA_PERF_GET
typedef struct {
    UINT64 clockCycleCount;
    UINT64 dataCycleCount;
    UINT64 pendingCount;
}ADMA_PERF_DATA;

#endif/*__ADMA_WINDOWS_H__*/

