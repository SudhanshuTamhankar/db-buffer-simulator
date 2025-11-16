/* rm_internal.h: Internal definitions for the RM Layer */
#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include "rm.h"

/*
 * =================================================================
 * Page Layout Definitions
 * =================================================================
 */

/*
 * RM_PageHeader: Sits at the beginning of each page.
 */
typedef struct {
  int numSlots;            /* Number of slots on this page (used or unused) */
  int freeSpaceOffset;     /* Offset from the start of the page to the
                              beginning of the free space (growing backward
                              from the end of the page) */
} RM_PageHeader;

/*
 * RM_Slot: An entry in the slot directory.
 * The slot directory (an array of these) starts immediately
 * after the RM_PageHeader.
 */
typedef struct {
  int offset; /* Offset from the start of the page to the
                 start of the record. -1 if slot is free. */
  int length; /* Length of the record in bytes. */
} RM_Slot;

/*
 * =================================================================
 * Useful Constants and Macros
 * =================================================================
 */

/* A page is full if the free space is less than a new slot + header */
#define RM_PAGE_FULL -1

/*
 * Magic number to check if a page is initialized.
 * (We can add this to the header later if we want)
 */

/*
 * Internal Function Prototypes (to be implemented in rm.c)
 */

/* Finds a page with enough free space for a new record */
int RM_FindFreePage(RM_FileHandle *fh, int record_len, int *pageNum);

/* Initializes a new page with the slotted-page layout */
int RM_InitPage(char *pageBuf);

#endif /* RM_INTERNAL_H */