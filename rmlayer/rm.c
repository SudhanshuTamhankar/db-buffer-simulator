#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * rm_internal.h includes rm.h, which (thanks to our Makefile)
 * will find pf.h. By including this one file, we get everything.
 */
#include "rm_internal.h"
/*
 * =================================================================
 * RM File Management Functions
 * =================================================================
 */

/*
 * RM_Init
 * Initializes the RM layer.
 */
void RM_Init() {
  /*
   * We must initialize the underlying PF layer.
   * Note: This means the user of the RM layer
   * does NOT need to call PF_Init() themselves.
   */
  PF_Init();
}

/*
 * RM_CreateFile
 * Creates a new file named fname.
 */
int RM_CreateFile(char *fname) {
  /*
   * We just let the PF layer create the file.
   * It will return PFE_OK or an error code.
   */
  return PF_CreateFile(fname);
}

/*
 * RM_DestroyFile
 * Destroys the file named fname.
 */
int RM_DestroyFile(char *fname) {
  /*
   * We just let the PF layer destroy the file.
   */
  return PF_DestroyFile(fname);
}

/*
 * RM_OpenFile
 * Opens the file named fname and prepares it for RM operations.
 */
int RM_OpenFile(char *fname, RM_FileHandle *fh) {
  int pf_fd;
  int pf_err;

  /*
   * 1. Open the file using the PF layer.
   * 2. Store the PF file descriptor in our RM_FileHandle.
   */
    pf_fd = PF_OpenFile(fname);
    if (pf_fd < 0) { // PF_OpenFile returns < 0 on error
        return pf_fd; // Return the PF error code
    }

    fh->pf_fd = pf_fd;
  return PFE_OK;
}

/*
 * RM_CloseFile
 * Closes the file associated with the RM_FileHandle.
 */
int RM_CloseFile(RM_FileHandle *fh) {
  /*
   * 1. Close the file using the PF layer.
   * 2. Invalidate the handle (optional, but good practice).
   */
  int pf_err = PF_CloseFile(fh->pf_fd);
  if (pf_err != PFE_OK) {
    return pf_err;
  }

  fh->pf_fd = -1; // Invalidate
  return PFE_OK;
}

/*
 * =================================================================
 * Internal RM Helper Functions
 * =================================================================
 */

/*
 * RM_InitPage
 * Initializes a new, empty page with the slotted-page layout.
 * The page must already be pinned (fixed) in the buffer.
 */
int RM_InitPage(char *pageBuf) {
    
    // 1. Set up the header for an empty page
    RM_PageHeader pageHeader;
    pageHeader.numSlots = 0;
    // Free space starts at the end of the page
    pageHeader.freeSpaceOffset = PF_PAGE_SIZE; 

    // 2. Copy the header into the very beginning of the page
    memcpy(pageBuf, &pageHeader, sizeof(RM_PageHeader));

    return PFE_OK;
}


/*
 * RM_FindFreePage
 * Scans the file for a page with at least `record_len` bytes of free space.
 * If no such page exists, it allocates a new page and returns its number.
 */
int RM_FindFreePage(RM_FileHandle *fh, int record_len, int *pageNum) {
    int pf_err;
    char *pageBuf;
    int currentPageNum = -1; // Start scan from the beginning
    
    // We need space for the record data + one new slot
    int requiredSpace = record_len + sizeof(RM_Slot); 

    // 1. Scan all existing pages using PF_GetNextPage
    while ((pf_err = PF_GetNextPage(fh->pf_fd, &currentPageNum, &pageBuf)) == PFE_OK) {
        RM_PageHeader *pageHeader = (RM_PageHeader *)pageBuf;

        // Calculate free space on this page
        // Free space = (start of record data) - (end of slot directory)
        int freeSpace = pageHeader->freeSpaceOffset - 
                        (sizeof(RM_PageHeader) + (pageHeader->numSlots * sizeof(RM_Slot)));

        // Unfix the page (we are only reading)
        int unfix_err = PF_UnfixPage(fh->pf_fd, currentPageNum, FALSE);
        if (unfix_err != PFE_OK) {
            return unfix_err;
        }

        // Check if we have enough space
        if (freeSpace >= requiredSpace) {
            *pageNum = currentPageNum;
            return PFE_OK; // Found a page!
        }
    }

    // 2. Check if the loop ended for a reason other than EOF
    if (pf_err != PFE_EOF) {
        return pf_err; // An error occurred during scan
    }

    // 3. No suitable page was found (we hit PFE_EOF). Allocate a new page.
    pf_err = PF_AllocPage(fh->pf_fd, pageNum, &pageBuf);
    if (pf_err != PFE_OK) {
        return pf_err;
    }
    
    // 4. Initialize the new page with our slotted-page header
    RM_InitPage(pageBuf);

    // 5. Unfix the newly allocated page, marking it dirty
    pf_err = PF_UnfixPage(fh->pf_fd, *pageNum, TRUE);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    return PFE_OK;
}


/*
 * =================================================================
 * RM Record Management Functions
 * =================================================================
 */

/*
 * RM_InsertRec
 * Inserts a new record into the file.
 * The RID (page and slot) is returned via the `rid` parameter.
 */
int RM_InsertRec(RM_FileHandle *fh, char *record_data, int record_len, RID *rid) {
    int pf_err;
    int pageNum;
    char *pageBuf;
    RM_PageHeader *pageHeader;
    RM_Slot *slot;

    // 1. Find a page with enough free space
    pf_err = RM_FindFreePage(fh, record_len, &pageNum);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    // 2. We have a suitable page (pageNum). Get it and fix it.
    pf_err = PF_GetThisPage(fh->pf_fd, pageNum, &pageBuf);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    // 3. Get the page header and find the location for the new slot
    pageHeader = (RM_PageHeader *)pageBuf;
    // The new slot is at the end of the current slot directory
    slot = (RM_Slot *)(pageBuf + sizeof(RM_PageHeader) + (pageHeader->numSlots * sizeof(RM_Slot)));

    // 4. Calculate the new record's location and copy the data
    // The record data is written at the *new* start of free space
    int newFreeSpaceOffset = pageHeader->freeSpaceOffset - record_len;
    char *recordLocation = pageBuf + newFreeSpaceOffset;
    memcpy(recordLocation, record_data, record_len);

    // 5. Update the slot with the record's info
    slot->offset = newFreeSpaceOffset;
    slot->length = record_len;

    // 6. Update the page header
    pageHeader->numSlots++;
    pageHeader->freeSpaceOffset = newFreeSpaceOffset;

    // 7. Update the output RID
    rid->pageNum = pageNum;
    rid->slotNum = pageHeader->numSlots - 1; // Slot numbers are 0-indexed

    // 8. Mark the page as dirty and unfix it
    pf_err = PF_UnfixPage(fh->pf_fd, pageNum, TRUE);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    return PFE_OK;
}
/*
 * RM_GetRec
 * Retrieves a specific record from the file given its RID.
 * The record data is copied into the `record_data` buffer.
 */
int RM_GetRec(RM_FileHandle *fh, const RID *rid, char *record_data) {
    int pf_err;
    char *pageBuf;
    RM_PageHeader *pageHeader;
    RM_Slot *slot;

    // 1. Get the correct page from the PF layer
    pf_err = PF_GetThisPage(fh->pf_fd, rid->pageNum, &pageBuf);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    // 2. Get the page header
    pageHeader = (RM_PageHeader *)pageBuf;

    // 3. Validate the slot number
    if (rid->slotNum < 0 || rid->slotNum >= pageHeader->numSlots) {
        // Unfix the page before returning an error
        PF_UnfixPage(fh->pf_fd, rid->pageNum, FALSE);
        return -1; // TODO: Define RM-specific error codes (e.g., RM_INVALID_RID)
    }

    // 4. Get the specific slot
    slot = (RM_Slot *)(pageBuf + sizeof(RM_PageHeader) + (rid->slotNum * sizeof(RM_Slot)));

    // 5. Check if the slot is valid (not deleted)
    if (slot->offset == -1) {
        // This slot is empty (record was deleted)
        // Unfix the page before returning an error
        PF_UnfixPage(fh->pf_fd, rid->pageNum, FALSE);
        return -2; // TODO: Define RM-specific error codes (e.g., RM_RECORD_DELETED)
    }

    // 6. Slot is valid, find the record data and copy it
    char *recordLocation = pageBuf + slot->offset;
    memcpy(record_data, recordLocation, slot->length);

    // 7. Unfix the page (no modifications were made)
    pf_err = PF_UnfixPage(fh->pf_fd, rid->pageNum, FALSE);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    return PFE_OK;
}

/*
 * RM_DeleteRec
 * Deletes a record from the file given its RID.
 * This is done by marking the slot as empty ("tombstone").
 */
int RM_DeleteRec(RM_FileHandle *fh, const RID *rid) {
    int pf_err;
    char *pageBuf;
    RM_PageHeader *pageHeader;
    RM_Slot *slot;

    // 1. Get the correct page from the PF layer
    pf_err = PF_GetThisPage(fh->pf_fd, rid->pageNum, &pageBuf);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    // 2. Get the page header
    pageHeader = (RM_PageHeader *)pageBuf;

    // 3. Validate the slot number
    if (rid->slotNum < 0 || rid->slotNum >= pageHeader->numSlots) {
        // Unfix the page before returning an error
        PF_UnfixPage(fh->pf_fd, rid->pageNum, FALSE);
        return -1; // TODO: Define RM_INVALID_RID
    }

    // 4. Get the specific slot
    slot = (RM_Slot *)(pageBuf + sizeof(RM_PageHeader) + (rid->slotNum * sizeof(RM_Slot)));

    // 5. Check if it's already deleted
    if (slot->offset == -1) {
        // Unfix the page before returning an error
        PF_UnfixPage(fh->pf_fd, rid->pageNum, FALSE);
        return -2; // TODO: Define RM_RECORD_DELETED
    }

    // 6. Mark the slot as deleted (tombstone)
    // We set the offset to -1 to indicate it's free.
    // Note: We are NOT reclaiming the data space yet.
    // This creates "holes" in the page, which is normal.
    // A full implementation might have a "compact" function.
    slot->offset = -1;
    // slot->length could also be set to 0, but offset=-1 is sufficient

    // 7. Mark the page as dirty and unfix it
    pf_err = PF_UnfixPage(fh->pf_fd, rid->pageNum, TRUE);
    if (pf_err != PFE_OK) {
        return pf_err;
    }

    return PFE_OK;
}
/*
 * =================================================================
 * RM Scan Functions
 * =================================================================
 */

/*
 * RM_ScanOpen
 * Initializes a scan on the file.
 */
int RM_ScanOpen(RM_FileHandle *fh, RM_ScanHandle *sh) {
    // 1. Store the file handle
    sh->fh = fh;
    
    // 2. Start the scan before the first page
    sh->currentPageNum = -1; 
    
    // 3. Start the scan before the first slot
    sh->currentSlotNum = -1;

    return PFE_OK;
}

/*
 * RM_GetNextRec
 * Retrieves the next valid record from the scan.
 */
int RM_GetNextRec(RM_ScanHandle *sh, char *record_data, RID *rid) {
    int pf_err;
    char *pageBuf;
    RM_PageHeader *pageHeader;
    RM_Slot *slot;

    // We loop indefinitely, breaking out when we find a record or hit EOF
    while (TRUE) {
        
        char *pageBuf;
        RM_PageHeader *pageHeader;
        int pf_err;

        // 1. Get the current page buffer
        if (sh->currentPageNum == -1) { 
            // This is the first call. Get the first page.
            pf_err = PF_GetNextPage(sh->fh->pf_fd, &sh->currentPageNum, &pageBuf);
            sh->currentSlotNum = 0; // Start scan from slot 0
        } else {
            // We are in the middle of a scan. Re-get the *current* page.
            pf_err = PF_GetThisPage(sh->fh->pf_fd, sh->currentPageNum, &pageBuf);
        }

        // Check if we're at the end of the file or hit an error
        if (pf_err == PFE_EOF) {
            return RM_EOF;
        }
        if (pf_err != PFE_OK) {
            return pf_err;
        }

        // We have a valid page, get its header
        pageHeader = (RM_PageHeader *)pageBuf;

        // 2. Scan slots *on this page*, starting from our saved slot number
        while (sh->currentSlotNum < pageHeader->numSlots) {
            slot = (RM_Slot *)(pageBuf + sizeof(RM_PageHeader) + (sh->currentSlotNum * sizeof(RM_Slot)));

            // Check if this slot is valid (not deleted)
            if (slot->offset != -1) {
                // 3. Found one!
                char *recordLocation = pageBuf + slot->offset;
                memcpy(record_data, recordLocation, slot->length);
                rid->pageNum = sh->currentPageNum;
                rid->slotNum = sh->currentSlotNum;

                // 4. Unfix page and return
                PF_UnfixPage(sh->fh->pf_fd, sh->currentPageNum, FALSE);
                
                // 5. Save our state for the *next* call
                sh->currentSlotNum++; // Next time, start at the *next* slot
                return PFE_OK; // Return with state saved
            }
            
            // 6. Slot was empty, try the next one
            sh->currentSlotNum++;
        }

        // 7. If we're here, we scanned all slots on this page.
        // Unfix the page and prepare to get the next one.
        PF_UnfixPage(sh->fh->pf_fd, sh->currentPageNum, FALSE);
        
        // This will advance sh->currentPageNum to the next page number
        // The loop will then restart
        pf_err = PF_GetNextPage(sh->fh->pf_fd, &sh->currentPageNum, &pageBuf);
        
        // Check if that was the last page
        if (pf_err == PFE_EOF) {
            return RM_EOF;
        }
        if (pf_err != PFE_OK) {
            return pf_err;
        }

        // We found a new page, but we must unfix it immediately.
        // The loop will start over, `PF_GetThisPage` will fetch it,
        // and we'll scan its slots.
        PF_UnfixPage(sh->fh->pf_fd, sh->currentPageNum, FALSE);
        
        // We need to decrement currentPageNum because GetNextPage
        // *already* advanced it, but the loop needs the *previous*
        // page number to advance *from* for the *next* GetNextPage call.
        // This is a subtle part of the PF_GetNextPage iterator.
        sh->currentPageNum--;

        // Reset slot to 0 so we scan the new page from the beginning
        sh->currentSlotNum = 0;
    }
}


/*
 * RM_ScanClose
 * Finishes a scan.
 */
int RM_ScanClose(RM_ScanHandle *sh) {
    // 1. Invalidate the scan handle (good practice)
    sh->fh = NULL;
    sh->currentPageNum = -1;
    sh->currentSlotNum = -1;
    
    // 2. There's nothing to "close" on the PF layer,
    // as PF_GetNextPage unfixes pages as it goes.
    return PFE_OK;
}
/*
 * =================================================================
 * RM Statistics Function
 * =================================================================
 */

/*
 * RM_GetSpaceUtilization
 * Scans the entire file to report on space usage.
 */
int RM_GetSpaceUtilization(RM_FileHandle *fh, int *total_pages, int *total_record_bytes, int *total_wasted_bytes) {
    int pf_err;
    char *pageBuf;
    int currentPageNum = -1;
    
    // Initialize output parameters
    *total_pages = 0;
    *total_record_bytes = 0;
    *total_wasted_bytes = 0;

    // 1. Scan all existing pages using PF_GetNextPage
    while ((pf_err = PF_GetNextPage(fh->pf_fd, &currentPageNum, &pageBuf)) == PFE_OK) {
        (*total_pages)++;
        
        RM_PageHeader *pageHeader = (RM_PageHeader *)pageBuf;
        int used_bytes_on_page = 0;
        int wasted_bytes_on_page = 0;

        // Calculate space used by header and slots
        int header_and_slot_space = sizeof(RM_PageHeader) + (pageHeader->numSlots * sizeof(RM_Slot));
        wasted_bytes_on_page += header_and_slot_space;

        // Calculate contiguous free space
        int free_space = pageHeader->freeSpaceOffset - header_and_slot_space;
        wasted_bytes_on_page += free_space;

        // 2. Scan all slots on this page
        for (int i = 0; i < pageHeader->numSlots; i++) {
            RM_Slot *slot = (RM_Slot *)(pageBuf + sizeof(RM_PageHeader) + (i * sizeof(RM_Slot)));

            if (slot->offset != -1) {
                // This is a valid record
                used_bytes_on_page += slot->length;
            } else {
                // This is a deleted record. The slot itself is already
                // counted, but the *data* it used to point to is now
                // "wasted" (a "hole").
                // Note: a simple implementation just counts the slot length.
                // A better one would track the hole size.
                // For now, we'll just track *record* bytes vs *other* bytes.
                // Let's refine:
                // wasted = (page size) - (bytes used by records)
            }
        }
        
        *total_record_bytes += used_bytes_on_page;
        
        // Total wasted = (Page Size) - (bytes used by *actual* records)
        *total_wasted_bytes += (PF_PAGE_SIZE - used_bytes_on_page);

        // Unfix the page
        int unfix_err = PF_UnfixPage(fh->pf_fd, currentPageNum, FALSE);
        if (unfix_err != PFE_OK) {
            return unfix_err;
        }
    }

    // Check if the loop ended for a reason other than EOF
    if (pf_err != PFE_EOF) {
        return pf_err; // An error occurred during scan
    }

    return PFE_OK;
}
