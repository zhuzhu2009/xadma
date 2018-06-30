#ifndef _ALTERA_DMA_CMD_H
#define _ALTERA_DMA_CMD_H

#define ALTERA_DMA_DRIVER_VERSION "2.02"

#define ALTERA_DMA_DID 0xE003
#define ALTERA_DMA_VID 0x1172

#define ALTERA_CMD_START_DMA            1
#define ALTERA_CMD_ENA_DIS_READ         2
#define ALTERA_CMD_ENA_DIS_WRITE        3
#define ALTERA_CMD_ENA_DIS_SIMUL        4
#define ALTERA_CMD_MODIFY_NUM_DWORDS    5
#define ALTERA_CMD_MODIFY_NUM_DESC      6
#define ALTERA_CMD_ONCHIP_OFFCHIP	7
#define ALTERA_LOOP                     8
#define ALTERA_CMD_READ_STATUS          9
#define ALTERA_EXIT                     10
#define ALTERA_CMD_WAIT_DMA             11 
#define ALTERA_CMD_ALLOC_RP_BUFFER      12
#define ALTERA_CMD_RAND			13

#include <linux/ioctl.h>

#define ALTERA_IOC_MAGIC   0x66
#define ALTERA_IOCX_WAIT             _IO(ALTERA_IOC_MAGIC, ALTERA_CMD_WAIT_DMA)
#define ALTERA_IOCX_START            _IO(ALTERA_IOC_MAGIC, ALTERA_CMD_START_DMA)
#define ALTERA_IOCX_ALLOC_RP_BUFFER  _IO(ALTERA_IOC_MAGIC, ALTERA_CMD_ALLOC_RP_BUFFER)

#ifndef __KERNEL__

#include <sys/ioctl.h>

#endif

struct dma_cmd {
    int cmd;
    int usr_buf_size;
    char *buf;
};

struct dma_status {
    char run_write;
    char run_read;
    char run_simul;
    int length_transfer;
    int altera_dma_num_dwords;
    int altera_dma_descriptor_num;
    struct timeval write_time;
    struct timeval read_time;
    struct timeval simul_time;
    char pass_read;
    char pass_write;
    char pass_simul;
    char read_eplast_timeout;
    char write_eplast_timeout;
    int offset;
    char onchip;
    char rand;
};

#endif /* _ALTERA_DMA_CMD_H */
