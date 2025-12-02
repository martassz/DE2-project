/*---------------------------------------------------------------------------/
/  Petit FatFs - Configuration file  R0.03a
/---------------------------------------------------------------------------*/

#ifndef PFCONF_DEF
#define PFCONF_DEF 8088	/* Revision ID */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define PF_USE_READ		1	/* Enable pf_read() function */
#define PF_USE_DIR		0	/* Enable pf_opendir() and pf_readdir() function */
#define PF_USE_LSEEK	1	/* Enable pf_lseek() function */
#define PF_USE_WRITE	1	/* Enable pf_write() function */

#define PF_FS_FAT12		0	/* Enable FAT12 */
#define PF_FS_FAT16		0	/* Enable FAT16 */
#define PF_FS_FAT32		1	/* Enable FAT32 */


/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define PF_USE_LCC		0	/* Allow lower case characters for path name */
#define PF_CODE_PAGE	437
/* Setting PF_CODE_PAGE to 0 disables code conversion function and
/  switching PF_USE_LCC to 1 is not allowed. */


/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#endif