/*---------------------------------------------------------------------------/
/  Petit FatFs - Configuration file  R0.03a
/---------------------------------------------------------------------------*/

#ifndef PFCONF_DEF
#define PFCONF_DEF 8088 /* Revision ID */

/**
 * @file pffconf.h
 * @brief Configuration options for Petit FatFs module.
 * @addtogroup pff_driver
 * @{
 */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

/** @brief Enable pf_read() function */
#define PF_USE_READ     1

/** @brief Enable pf_opendir() and pf_readdir() function */
#define PF_USE_DIR      0

/** @brief Enable pf_lseek() function */
#define PF_USE_LSEEK    1

/** @brief Enable pf_write() function */
#define PF_USE_WRITE    1

/*---------------------------------------------------------------------------/
/ File System Configurations
/---------------------------------------------------------------------------*/

/** @brief Enable FAT12 support */
#define PF_FS_FAT12     0

/** @brief Enable FAT16 support */
#define PF_FS_FAT16     0

/** @brief Enable FAT32 support */
#define PF_FS_FAT32     1


/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

/** @brief Allow lower case characters for path name */
#define PF_USE_LCC      0

/** @brief Character code page (437 = U.S.) */
#define PF_CODE_PAGE    437

/** @} */
#endif /* PFCONF_DEF */