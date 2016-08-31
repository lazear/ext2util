/*
ide.h

Header file containing definitions of IDE status bits and registers
Adapted from old code of my own origin (inspired by osdever.net) and
with xv6 inspired queueing
*/

#include "types.h"

#ifndef __baremetal_ide__
#define __baremetal_ide__

#define BLOCK_SIZE		1024
#define SECTOR_SIZE		512

#define B_BUSY	0x1		// buffer is locked by a process
#define B_VALID	0x2		// buffer has been read from disk
#define B_DIRTY	0x4		// buffer has been written to


/* Define IDE status bits */
#define IDE_BSY 		(1<<7)	// Drive is preparing to send/receive data
#define IDE_RDY 		(1<<6)	// Clear when drive is spun down, or after error
#define IDE_DF			(1<<5)	// Drive Fault error
#define IDE_ERR			(1<<0)	// Error has occured

#define IDE_IO			0x1F0	// Main IO port
#define IDE_DATA		0x0 	// R/W PIO data bytes
#define IDE_FEAT		0x1 	// ATAPI devices
#define IDE_SECN		0x2 	// # of sectors to R/W
#define IDE_LOW			0x3 	// CHS/LBA28/LBA48 specific
#define IDE_MID 		0x4
#define IDE_HIGH		0x5
#define IDE_HEAD		0x6 	// Select drive/heaad
#define IDE_CMD 		0x7 	// Command/status port
#define IDE_ALT			0x3F6	// alternate status
#define LBA_LOW(c)		((uint8_t) (c & 0xFF))
#define LBA_MID(c)		((uint8_t) (c >> 8) & 0xFF)
#define LBA_HIGH(c)		((uint8_t) (c >> 16) & 0xFF)
#define LBA_LAST(c)		((uint8_t) (c >> 24) & 0xF)

#define IDE_CMD_READ  (BLOCK_SIZE/SECTOR_SIZE == 1) ? 0x20 : 0xC4
#define IDE_CMD_WRITE (BLOCK_SIZE/SECTOR_SIZE == 1) ? 0x30 : 0xC5
#define IDE_CMD_READ_MUL  0xC4
#define IDE_CMD_WRITE_MUL 0xC5



#define MAX_OP_BLOCKS	16	// Max # of blocks any operation can write (8 KB)


typedef struct ide_buffer {
	uint8_t flags;				// buffer flags
	uint32_t dev;				// device number
	uint32_t block;				// block number
	struct ide_buffer* next;	// next block in queue
	struct ide_buffer* q;
	uint8_t data[BLOCK_SIZE];	// 1 disk sector of data
} buffer;

extern int IDE_STATUS;


#endif