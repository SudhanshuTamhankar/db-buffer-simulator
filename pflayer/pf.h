/* pf.h: Public Interface for the Paged File (PF) Layer */
#ifndef PF_H
#define PF_H

/* Standard includes */
#include <stdio.h> /* For NULL */

/************** Constants and Definitions *********************/
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Page size */
#define PF_PAGE_SIZE 4096

/* Page Replacement Strategies */
#define PF_LRU 0
#define PF_MRU 1

/************** Error Codes *********************************/
#define PFE_OK 0        /* OK */
#define PFE_NOMEM -1    /* no memory */
#define PFE_NOBUF -2    /* no buffer space */
#define PFE_PAGEFIXED -3    /* page already fixed in buffer */
#define PFE_PAGENOTINBUF -4 /* page to be unfixed is not in the buffer */
#define PFE_UNIX -5         /* unix error */
#define PFE_INCOMPLETEREAD -6 /* incomplete read of page from file */
#define PFE_INCOMPLETEWRITE -7 /* incomplete write of page to file */
#define PFE_HDRREAD -8      /* incomplete read of header from file */
#define PFE_HDRWRITE -9     /* incomplete write of header to file */
#define PFE_INVALIDPAGE -10 /* invalid page number */
#define PFE_FILEOPEN -11    /* file already open */
#define PFE_FTABFULL -12    /* file table is full */
#define PFE_FD -13          /* invalid file descriptor */
#define PFE_EOF -14         /* end of file */
#define PFE_PAGEFREE -15    /* page already free */
#define PFE_PAGEUNFIXED -16 /* page already unfixed */

/* Internal errors: report to TA */
#define PFE_PAGEINBUF -17     /* new page to be allocated already in buffer */
#define PFE_HASHNOTFOUND -18  /* hash table entry not found */
#define PFE_HASHPAGEEXIST -19 /* page already exist in hash table */

/***************** Extern Variables ***********************/
extern int PFerrno; /* error number of last error */

/***************** Public API ***********************/

/*
 * PF_Init:
 * Initializes the Paged File (PF) layer.
 * Must be called before any other PF function.
 */
void PF_Init(void);

/*
 * PF_CreateFile:
 * Creates a new paged file with the given name.
 */
int PF_CreateFile(char *fname);

/*
 * PF_DestroyFile:
 * Destroys the paged file with the given name.
 * The file must not be open.
 */
int PF_DestroyFile(char *fname);

/*
 * PF_OpenFile:
 * Opens the paged file with the given name.
 * Returns a file descriptor (fd) < 0 on error.
 */
int PF_OpenFile(char *fname);

/*
 * PF_CloseFile:
 * Closes the file associated with the given fd.
 * All pages must be unfixed.
 */
int PF_CloseFile(int fd);

/*
 * PF_GetFirstPage:
 * Gets the first valid (used) page from the file.
 * Returns PFE_EOF if the file is empty.
 */
int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_GetNextPage:
 * Gets the next valid (used)page after the one specified by *pagenum.
 * *pagenum is an input/output parameter.
 * Returns PFE_EOF if no more pages are found.
 */
int PF_GetNextPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_GetThisPage:
 * Gets the specific page with the given pagenum.
 */
int PF_GetThisPage(int fd, int pagenum, char **pagebuf);

/*
 * PF_AllocPage:
 * Allocates a new page in the file.
 * The page is fixed in the buffer.
 */
int PF_AllocPage(int fd, int *pagenum, char **pagebuf);

/*
 * PF_DisposePage:
 * Disposes of (deletes) a page from the file.
 * The page must not be fixed.
 */
int PF_DisposePage(int fd, int pagenum);

/*
 * PF_UnfixPage:
 * Unfixes a page from the buffer.
 * 'dirty' is TRUE if the page was modified, FALSE otherwise.
 */
int PF_UnfixPage(int fd, int pagenum, int dirty);

/*
 * PF_SetStrategy:
 * Sets the page replacement strategy (PF_LRU or PF_MRU).
 * Default is PF_LRU.
 */
void PF_SetStrategy(int strategy);

/*
 * PF_PrintError:
 * Prints a PF-specific error message based on PFerrno.
 */
void PF_PrintError(char *s);

/*
 * PF_PrintStats:
 * Prints the page access statistics.
 */
void PF_PrintStats(void);

/* NEW FUNCTIONS TO ADD */
void PF_ResetStats(void);
void PF_GetStats(long *logical_reads, long *physical_reads, long *physical_writes);

#endif /* PF_H */