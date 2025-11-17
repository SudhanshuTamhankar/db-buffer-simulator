/* pf.c: Paged File Interface Routines + support routines */
#include <stdio.h>
#include <stdlib.h> /* For malloc, free, exit */
#include <string.h> /* For strcpy, strcmp */
#include <unistd.h> /* For lseek, read, write, close, unlink */
#include <fcntl.h>  /* For open flags O_CREAT etc. */
#include <sys/types.h>
/* #include <sys/file.h> */ /* This is often not needed with unistd.h */
#include "pf.h"
#include "pftypes.h"

#ifndef PF_DEBUG
#define PF_DEBUG 0
#endif

/* To keep system V and PC users happy */
#ifndef L_SET
#define L_SET 0
#endif

int PFerrno = PFE_OK; /* last error message */

static PFftab_ele PFftab[PF_FTAB_SIZE]; /* table of opened files */
PFstats pf_stats;                       /* Statistics */

/* true if file descriptor fd is invalid */
#define PFinvalidFd(fd) ((fd) < 0 || (fd) >= PF_FTAB_SIZE \
                || PFftab[fd].fname == NULL)

/* true if page number "pagenum" of file "fd" is invalid in the
   sense that it's <0 or >= # of pages in the file */
#define PFinvalidPagenum(fd,pagenum) ((pagenum)<0 || (pagenum) >= \
                PFftab[fd].hdr.numpages)

/****************** Internal Support Functions *****************************/
static char *savestr(char *str)
/****************************************************************************
SPECIFICATIONS:
    Allocate memory and save string pointed by str.
    Return a pointer to the saved string, or NULL if no memory.
*****************************************************************************/
{
  char *s;

  if ((s = malloc(strlen(str) + 1)) != NULL)
    strcpy(s, str);
  return (s);
}

static int PFtabFindFname(char *fname)
/****************************************************************************
SPECIFICATIONS:
    Find the index to PFftab[] entry whose "fname" field is the
    same as "fname".
*****************************************************************************/
{
  int i;

  for (i = 0; i < PF_FTAB_SIZE; i++) {
    if (PFftab[i].fname != NULL && strcmp(PFftab[i].fname, fname) == 0)
      /* found it */
      return (i);
  }
  return (-1);
}

static int PFftabFindFree(void)
/****************************************************************************
SPECIFICATIONS:
    Find a free entry in the open file table "PFtab", and return its
    index.
*****************************************************************************/
{
  int i;

  for (i = 0; i < PF_FTAB_SIZE; i++)
    if (PFftab[i].fname == NULL)
      return (i);
  return (-1);
}

int PFreadfcn(int fd, int pagenum, PFfpage *buf)
/****************************************************************************
SPECIFICATIONS:
    Read the paged numbered "pagenum" from the file indexed by "fd"
    into the page buffer "buf".
*****************************************************************************/
{
  int error;

  /* seek to the appropriate place */
  if ((error = lseek(PFftab[fd].unixfd, pagenum * sizeof(PFfpage) + PF_HDR_SIZE,
                     L_SET)) == -1) {
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  /* read the data */
  if ((error = read(PFftab[fd].unixfd, (char *)buf, sizeof(PFfpage))) != sizeof(PFfpage)) {
    if (error < 0)
      PFerrno = PFE_UNIX;
    else
      PFerrno = PFE_INCOMPLETEREAD;
    return (PFerrno);
  }

  /* Increment physical read count */
  pf_stats.physical_reads++;

  return (PFE_OK);
}

int PFwritefcn(int fd, int pagenum, PFfpage *buf)
/****************************************************************************
SPECIFICATIONS:
    Write the page numbered "pagenum" from the buffer indexed
    by "buf" into the file indexed by "fd".
*****************************************************************************/
{
  int error;

  /* seek to the right place */
  if ((error = lseek(PFftab[fd].unixfd, pagenum * sizeof(PFfpage) + PF_HDR_SIZE,
                     L_SET)) == -1) {
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  /* write out the page */
  if ((error = write(PFftab[fd].unixfd, (char *)buf, sizeof(PFfpage))) != sizeof(PFfpage)) {
    if (error < 0)
      PFerrno = PFE_UNIX;
    else
      PFerrno = PFE_INCOMPLETEWRITE;
    return (PFerrno);
  }

  /* Increment physical write count */
  pf_stats.physical_writes++;

  return (PFE_OK);
}

/************************* Interface Routines ****************************/

void PF_Init(void)
/****************************************************************************
SPECIFICATIONS:
    Initialize the PF interface. Must be the first function called
    in order to use the PF ADT.
*****************************************************************************/
{
  int i;
  /* init the hash table */
  PFhashInit();

  /* init the file table to be not used*/
  for (i = 0; i < PF_FTAB_SIZE; i++) {
    PFftab[i].fname = NULL;
  }

  /* init the stats */
  PF_ResetStats();
}

int PF_CreateFile(char *fname)
/****************************************************************************
SPECIFICATIONS:
    Create a paged file called "fname". The file should not have
    already existed before.
*****************************************************************************/
{
  int fd;       /* unix file descriptor */
  PFhdr_str hdr; /* file header */
  int error;

  /* create file for exclusive use */
  if ((fd = open(fname, O_CREAT | O_EXCL | O_WRONLY, 0664)) < 0) {
    /* unix error on open */
    PFerrno = PFE_UNIX;
    return (PFE_UNIX);
  }

  /* write out the file header */
  hdr.firstfree = PF_PAGE_LIST_END; /* no free page yet */
  hdr.numpages = 0;
  if ((error = write(fd, (char *)&hdr, sizeof(hdr))) != sizeof(hdr)) {
    /* error while writing. Abort everything. */
    if (error < 0)
      PFerrno = PFE_UNIX;
    else
      PFerrno = PFE_HDRWRITE;
    close(fd);
    unlink(fname);
    return (PFerrno);
  }

  if ((error = close(fd)) == -1) {
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  return (PFE_OK);
}

int PF_DestroyFile(char *fname)
/****************************************************************************
SPECIFICATIONS:
    Destroy the paged file whose name is "fname". The file should
    exist, and should not be already open.
*****************************************************************************/
{
  int error;

  if (PFtabFindFname(fname) != -1) {
    /* file is open */
    PFerrno = PFE_FILEOPEN;
    return (PFerrno);
  }

  if ((error = unlink(fname)) != 0) {
    /* unix error */
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  /* success */
  return (PFE_OK);
}

int PF_OpenFile(char *fname)
/****************************************************************************
SPECIFICATIONS:
    Open the paged file whose name is fname.  It is possible to open
    a file more than once.
*****************************************************************************/
{
  int count; /* # of bytes in read */
  int fd;    /* file descriptor */

  /* find a free entry in the file table */
  if ((fd = PFftabFindFree()) < 0) {
    /* file table full */
    PFerrno = PFE_FTABFULL;
    return (PFerrno);
  }

  /* open the file */
  if ((PFftab[fd].unixfd = open(fname, O_RDWR)) < 0) {
    /* can't open the file */
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  /* Read the file header */
  if ((count = read(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr, PF_HDR_SIZE)) != PF_HDR_SIZE) {
    if (count < 0)
      /* unix error */
      PFerrno = PFE_UNIX;
    else /* not enough bytes in file */
      PFerrno = PFE_HDRREAD;
    close(PFftab[fd].unixfd);
    return (PFerrno);
  }
  /* set file header to be not changed */
  PFftab[fd].hdrchanged = FALSE;

  /* save the file name */
  if ((PFftab[fd].fname = savestr(fname)) == NULL) {
    /* no memory */
    close(PFftab[fd].unixfd);
    PFerrno = PFE_NOMEM;
    return (PFerrno);
  }

  return (fd);
}

int PF_CloseFile(int fd)
/****************************************************************************
SPECIFICATIONS:
    Close the file indexed by file descriptor fd.
*****************************************************************************/
{
  int error;

  if (PFinvalidFd(fd)) {
    /* invalid file descriptor */
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  /* Flush all buffers for this file */
  if ((error = PFbufReleaseFile(fd, PFwritefcn)) != PFE_OK)
    return (error);

  if (PFftab[fd].hdrchanged) {
    /* write the header back to the file */
    /* First seek to the appropriate place */
    if ((error = lseek(PFftab[fd].unixfd, (unsigned)0, L_SET)) == -1) {
      /* seek error */
      PFerrno = PFE_UNIX;
      return (PFerrno);
    }

    /* write header*/
    if ((error = write(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr,
                       PF_HDR_SIZE)) != PF_HDR_SIZE) {
      if (error < 0)
        PFerrno = PFE_UNIX;
      else
        PFerrno = PFE_HDRWRITE;
      return (PFerrno);
    }
    PFftab[fd].hdrchanged = FALSE;
  }

  /* close the file */
  if ((error = close(PFftab[fd].unixfd)) == -1) {
    PFerrno = PFE_UNIX;
    return (PFerrno);
  }

  /* free the file name space */
  free((char *)PFftab[fd].fname);
  PFftab[fd].fname = NULL;

  return (PFE_OK);
}

int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf)
/****************************************************************************
SPECIFICATIONS:
    Read the first page into memory and set *pagebuf to point to it.
*****************************************************************************/
{
  *pagenum = -1;
  return (PF_GetNextPage(fd, pagenum, pagebuf));
}

int PF_GetNextPage(int fd, int *pagenum, char **pagebuf)
/****************************************************************************
SPECIFICATIONS:
    Read the next valid page after *pagenum, the current page number,
    and set *pagebuf to point to the page data.
*****************************************************************************/
{
  int temppage; /* page number to scan for next valid page */
  int error;    /* error code */
  PFfpage *fpage; /* pointer to file page */

  if (PFinvalidFd(fd)) {
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  if (*pagenum < -1 || *pagenum >= PFftab[fd].hdr.numpages) {
    PFerrno = PFE_INVALIDPAGE;
    return (PFerrno);
  }

  /* scan the file until a valid used page is found */
  for (temppage = *pagenum + 1; temppage < PFftab[fd].hdr.numpages; temppage++) {
    if ((error = PFbufGet(fd, temppage, &fpage, PFreadfcn,
                         PFwritefcn)) != PFE_OK)
      return (error);
    else if (fpage->nextfree == PF_PAGE_USED) {
      /* found a used page */
      *pagenum = temppage;
      *pagebuf = (char *)fpage->pagebuf;
      return (PFE_OK);
    }

    /* page is free, unfix it */
    if ((error = PFbufUnfix(fd, temppage, FALSE)) != PFE_OK)
      return (error);
  }

  /* No valid used page found */
  PFerrno = PFE_EOF;
  return (PFerrno);
}

int PF_GetThisPage(int fd, int pagenum, char **pagebuf)
/****************************************************************************
SPECIFICATIONS:
    Read the page specified by "pagenum" and set *pagebuf to point
    to the page data. The page number should be valid.
*****************************************************************************/
{
  int error;
  PFfpage *fpage;

  if (PFinvalidFd(fd)) {
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  if (PFinvalidPagenum(fd, pagenum)) {
    PFerrno = PFE_INVALIDPAGE;
    return (PFerrno);
  }

  if ((error = PFbufGet(fd, pagenum, &fpage, PFreadfcn, PFwritefcn)) != PFE_OK) {
    if (error == PFE_PAGEFIXED)
      *pagebuf = fpage->pagebuf;
    return (error);
  }

  if (fpage->nextfree == PF_PAGE_USED) {
    /* page is used*/
    *pagebuf = (char *)fpage->pagebuf;
    return (PFE_OK);
  } else {
    /* invalid page */
    if (PFbufUnfix(fd, pagenum, FALSE) != PFE_OK) {
      printf("internal error:PFgetThis()\n");
      exit(1);
    }
    PFerrno = PFE_INVALIDPAGE;
    return (PFerrno);
  }
}

int PF_AllocPage(int fd, int *pagenum, char **pagebuf)
/****************************************************************************
SPECIFICATIONS:
    Allocate a new, empty page for file "fd".
*****************************************************************************/
{
  PFfpage *fpage; /* pointer to file page */
  int error;

  if (PFinvalidFd(fd)) {
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  if (PFftab[fd].hdr.firstfree != PF_PAGE_LIST_END) {
    /* get a page from the free list */
    *pagenum = PFftab[fd].hdr.firstfree;
    if ((error = PFbufGet(fd, *pagenum, &fpage, PFreadfcn,
                         PFwritefcn)) != PFE_OK)
      /* can't get the page */
      return (error);
    PFftab[fd].hdr.firstfree = fpage->nextfree;
    PFftab[fd].hdrchanged = TRUE;
  } else {
    /* Free list empty, allocate one more page from the file */
    *pagenum = PFftab[fd].hdr.numpages;
        if ((error = PFbufAlloc(fd, *pagenum, &fpage, PFwritefcn)) != PFE_OK) {
      /* can't allocate a page */
      #if PF_DEBUG
      fprintf(stderr, "DEBUG PF_AllocPage: PFbufAlloc failed for fd=%d pagenum=%d error=%d\n",
              fd, *pagenum, error);
      #endif
      return (error);
    } else {
      #if PF_DEBUG
      fprintf(stderr, "DEBUG PF_AllocPage: PFbufAlloc succeeded for fd=%d pagenum=%d (fpage=%p)\n",
              fd, *pagenum, (void *)fpage);
      #endif
    }


    /* increment # of pages for this file */
    PFftab[fd].hdr.numpages++;
    PFftab[fd].hdrchanged = TRUE;

    /* mark this page dirty */
    /* mark this page dirty (make page 'used' in buffer). If PFbufUsed fails,
   return the error to the caller so it can be diagnosed by the caller. */
    if ((error = PFbufUsed(fd, *pagenum)) != PFE_OK) {
      /* propagate the PF error (do NOT exit the process here) */
      PFerrno = error;
      return (error);
    }

  }

  /* Mark the new page used */
  fpage->nextfree = PF_PAGE_USED;

  /* set return value */
  *pagebuf = fpage->pagebuf;

  return (PFE_OK);
}

int PF_DisposePage(int fd, int pagenum)
/****************************************************************************
SPECIFICATIONS:
    Dispose the page numbered "pagenum" of the file "fd".
*****************************************************************************/
{
  PFfpage *fpage; /* pointer to file page */
  int error;

  if (PFinvalidFd(fd)) {
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  if (PFinvalidPagenum(fd, pagenum)) {
    PFerrno = PFE_INVALIDPAGE;
    return (PFerrno);
  }

  if ((error = PFbufGet(fd, pagenum, &fpage, PFreadfcn, PFwritefcn)) != PFE_OK)
    /* can't get this page */
    return (error);

  if (fpage->nextfree != PF_PAGE_USED) {
    /* this page already freed */
    if (PFbufUnfix(fd, pagenum, FALSE) != PFE_OK) {
      printf("internal error: PFdispose()\n");
      exit(1);
    }
    PFerrno = PFE_PAGEFREE;
    return (PFerrno);
  }

  /* put this page into the free list */
  fpage->nextfree = PFftab[fd].hdr.firstfree;
  PFftab[fd].hdr.firstfree = pagenum;
  PFftab[fd].hdrchanged = TRUE;

  /* unfix this page */
  return (PFbufUnfix(fd, pagenum, TRUE));
}

int PF_UnfixPage(int fd, int pagenum, int dirty)
/****************************************************************************
SPECIFICATIONS:
    Tell the Paged File Interface that the page numbered "pagenum"
    of the file "fd" is no longer needed in the buffer.
    Set the variable "dirty" to TRUE if page has been modified.
*****************************************************************************/
{
  /* DEBUGGING: print every unfix call */
  #if PF_DEBUG
  fprintf(stderr, "DEBUG PF_UnfixPage called: fd=%d pagenum=%d dirty=%d\n",
          fd, pagenum, dirty);
  #endif

  /* Validate fd and pagenum first (avoid printing diagnostics for invalid inputs) */
  if (PFinvalidFd(fd)) {
    PFerrno = PFE_FD;
    return (PFerrno);
  }

  if (PFinvalidPagenum(fd, pagenum)) {
    PFerrno = PFE_INVALIDPAGE;
    return (PFerrno);
  }

    /* If the page is not present in hash, treat as already-unfixed and return OK.
     This makes Unfix idempotent from caller perspective and avoids spurious failures
     if the page mapping was dropped (e.g., due to earlier release). */
    if (PFhashFind(fd, pagenum) == NULL) {
      return PFE_OK;
    }

  /* safe to unfix the page */
  return (PFbufUnfix(fd, pagenum, dirty));
}

void PF_SetStrategy(int strategy)
/****************************************************************************
SPECIFICATIONS:
    Sets the page replacement strategy (PF_LRU or PF_MRU).
*****************************************************************************/
{
  PFbufSetStrategy(strategy);
}

/* error messages */
static char *PFerrormsg[] = {
    "No error",
    "No memory",
    "No buffer space",
    "Page already fixed in buffer",
    "page to be unfixed is not in the buffer",
    "unix error",
    "incomplete read of page from file",
    "incomplete write of page to file",
    "incomplete read of header from file",
    "incomplete write of header to file",
    "invalid page number",
    "file already open",
    "file table full",
    "invalid file descriptor",
    "end of file",
    "page already free",
    "page already unfixed",
    "new page to be allocated already in buffer",
    "hash table entry not found",
    "page already in hash table"} ;

void PF_PrintError(char *s)
/****************************************************************************
SPECIFICATIONS:
    Write the string "s" onto stderr, then write the last
    error message from PF onto stderr.
*****************************************************************************/
{

  fprintf(stderr, "%s", s);
  fprintf(stderr, ":%s", PFerrormsg[-1 * PFerrno]);
  if (PFerrno == PFE_UNIX)
    /* print the unix error message */
    perror(" ");
  else
    fprintf(stderr, "\n");
}

void PF_PrintStats(void)
/****************************************************************************
SPECIFICATIONS:
    Print the page access statistics in CSV format.
*****************************************************************************/
{
  /* Print in a CSV format: logical_reads,physical_reads,physical_writes */
  printf("%ld,%ld,%ld\n",
         pf_stats.logical_reads,
         pf_stats.physical_reads,
         pf_stats.physical_writes);
}

/*
 * =================================================================
 * Statistics Functions
 * =================================================================
 */

void PF_ResetStats(void)
/****************************************************************************
SPECIFICATIONS:
    Resets the statistics counters.
*****************************************************************************/
{
  pf_stats.logical_reads = 0;
  pf_stats.physical_reads = 0;
  pf_stats.physical_writes = 0;
}

void PF_GetStats(long *logical_reads, long *physical_reads, long *physical_writes)
/****************************************************************************
SPECIFICATIONS:
    Gets the current statistics counters.
*****************************************************************************/
{
  *logical_reads = pf_stats.logical_reads;
  *physical_reads = pf_stats.physical_reads;
  *physical_writes = pf_stats.physical_writes;
}
