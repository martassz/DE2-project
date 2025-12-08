/*-----------------------------------------------------------------------*/
/* Low level disk I/O module interface for Petit FatFs                   */
/*-----------------------------------------------------------------------*/

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

#include "pff.h"

/**
 * @file diskio.h
 * @brief Low level disk I/O module for SD Card (SPI).
 *
 * Defines the interface for communicating with the SD card via SPI.
 * Based on the original implementation by ChaN.
 *
 * @author ChaN (Original Author)
 * @author Team DE2-Project (Port/Modifications)
 * @date 2025
 *
 * @addtogroup pff_driver
 * @{
 */

/** @brief Status of Disk Functions */
typedef BYTE DSTATUS;

/** @brief Results of Disk Functions */
typedef enum {
    RES_OK = 0,     /**< 0: Function succeeded */
    RES_ERROR,      /**< 1: Disk error */
    RES_NOTRDY,     /**< 2: Not ready */
    RES_PARERR      /**< 3: Invalid parameter */
} DRESULT;

/*---------------------------------------*/
/* Prototypes for disk control functions */

/**
 * @brief  Initialize the Disk Drive (SD Card).
 * @return Status code (0 = OK, STA_NOINIT = Failed).
 */
DSTATUS disk_initialize(void);

/**
 * @brief  Read partial sector from the disk.
 * @param  buff   Pointer to the read buffer.
 * @param  sector Sector number (LBA).
 * @param  offset Offset in the sector.
 * @param  count  Number of bytes to read.
 * @return Result code (DRESULT).
 */
DRESULT disk_readp(BYTE* buff, DWORD sector, UINT offset, UINT count);

/**
 * @brief  Write partial sector to the disk.
 * @param  buff Pointer to the data to be written.
 * @param  sc   Sector number (LBA).
 * @return Result code (DRESULT).
 */
DRESULT disk_writep(const BYTE* buff, DWORD sc);

/* Disk Status Bits */
#define STA_NOINIT      0x01    /**< Drive not initialized */
#define STA_NODISK      0x02    /**< No medium in the drive */

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* _DISKIO_DEFINED */