/* rm.h: Public Interface for the Record Management (RM) Layer */
#ifndef RM_H
#define RM_H

#include "../pflayer/pf.h" /* We depend on the PF Layer */

/*
 * RID: Record ID
 * Uniquely identifies a record within a file.
 * It contains the page number and the slot number.
 */
typedef struct {
  int pageNum; /* Page number in the file */
  int slotNum; /* Slot number within the page */
} RID;

/*
 * RM_FileHandle:
 * Used to access a file managed by the RM layer.
 */
typedef struct {
  int pf_fd; /* The PF layer's file descriptor */
  /* We might add file-level metadata here later */
} RM_FileHandle;

/*
 * =================================================================
 * Public API Functions
 * =================================================================
 */

/* Initialize the RM layer */
void RM_Init(void);

/* Create a new file */
int RM_CreateFile(char *fname);

/* Destroy a file */
int RM_DestroyFile(char *fname);

/* Open a file */
int RM_OpenFile(char *fname, RM_FileHandle *fh);

/* Close a file */
int RM_CloseFile(RM_FileHandle *fh);

/* Insert a new record */
int RM_InsertRec(RM_FileHandle *fh, char *record_data, int record_len, RID *rid);

/* Delete a record */
int RM_DeleteRec(RM_FileHandle *fh, const RID *rid);

/* Get a specific record */
int RM_GetRec(RM_FileHandle *fh, const RID *rid, char *record_data);

/* Get space utilization info */
int RM_GetSpaceUtilization(RM_FileHandle *fh, int *total_pages, int *total_record_bytes, int *total_wasted_bytes);

/*
 * =================================================================
 * Scan Functions
 * =================================================================
 */

/*
 * RM_ScanHandle:
 * Used to keep track of the state of a scan.
 */
typedef struct {
  RM_FileHandle *fh;   /* File handle for the file being scanned */
  int currentPageNum;  /* Page number of the current page */
  int currentSlotNum;  /* Slot number of the next record to return */
} RM_ScanHandle;

/*
 * RM_ScanOpen
 * Initializes a scan on the file.
 */
int RM_ScanOpen(RM_FileHandle *fh, RM_ScanHandle *sh);

/*
 * RM_GetNextRec
 * Retrieves the next valid record from the scan.
 * Returns RM_EOF when the scan is complete.
 */
int RM_GetNextRec(RM_ScanHandle *sh, char *record_data, RID *rid);

/*
 * RM_ScanClose
 * Finishes a scan.
 */
int RM_ScanClose(RM_ScanHandle *sh);

/*
 * =================================================================
 * RM-specific Error Codes
 * =================================================================
 */
#define RM_EOF -100 // End of file/scan
#define RM_INVALID_RID -101
#define RM_RECORD_DELETED -102

#endif /* RM_H */