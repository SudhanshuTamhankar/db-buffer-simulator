/* pftypes.h: Internal Type Declarations for PF Layer */
#ifndef PFTYPES_H
#define PFTYPES_H

#include "pf.h"

/************************ Statistics **************************/
typedef struct PFstats {
  long logical_reads;  /* Page requests found in buffer */
  long physical_reads; /* Page requests read from disk */
  long physical_writes; /* Pages written to disk */
} PFstats;

extern PFstats pf_stats;

/**************************** File Page Decls *********************/
/* Each file contains a header, which is a integer pointing
to the first free page, or -1 if no more free pages in the file.
Followed by this header are the file pages as declared in struct PFfpage */
typedef struct PFhdr_str {
  int firstfree; /* first free page in the linked list of
                                free pages */
  int numpages;  /* # of pages in the file */
} PFhdr_str;

#define PF_HDR_SIZE sizeof(PFhdr_str) /* size of file header */

/* actual page struct to be written onto the file */
#define PF_PAGE_LIST_END -1 /* end of list of free pages */
#define PF_PAGE_USED -2     /* page is being used */
typedef struct PFfpage {
  int nextfree; /* page number of next free page in the linked
                   list of free pages, or PF_PAGE_LIST_END if
                   end of list, or PF_PAGE_USED if this page is not free */
  char pagebuf[PF_PAGE_SIZE]; /* actual page data */
} PFfpage;

/*************************** Opened File Table **********************/
#define PF_FTAB_SIZE 20 /* size of open file table */

/* open file table entry */
typedef struct PFftab_ele {
  char *fname;         /* file name, or NULL if entry not used */
  int unixfd;          /* unix file descriptor*/
  PFhdr_str hdr;       /* file header */
  short hdrchanged;    /* TRUE if file header has changed */
} PFftab_ele;

/************************** Buffer Page Decls *********************/
#define PF_MAX_BUFS 20 /* max # of buffers */

/* buffer page decl */
typedef struct PFbpage {
  struct PFbpage *nextpage; /* next in the linked list of
                                        buffer page */
  struct PFbpage *prevpage; /* previous in the linked list
                                        of buffer pages */
  unsigned short dirty : 1, /* TRUE if page is dirty */
      fixed : 1;            /* TRUE if page is fixed in buffer*/
  int page;                 /* page number of this page */
  int fd;                   /* file desciptor of this page */
  PFfpage fpage;            /* page data from the file */
} PFbpage;

/******************** Hash Table Decls ****************************/
#define PF_HASH_TBL_SIZE 20 /* size of PF hash table */

/* Hash table bucket entries*/
typedef struct PFhash_entry {
  struct PFhash_entry *nextentry; /* next hash table element, or NULL */
  struct PFhash_entry *preventry; /*previous hash table element,or NULL*/
  int fd;                         /* file descriptor */
  int page;                       /* page number */
  struct PFbpage *bpage;          /* pointer to buffer holding this page */
} PFhash_entry;

/* Hash function for hash table */
#define PFhash(fd, page) (((fd) + (page)) % PF_HASH_TBL_SIZE)

/******************* Interface functions from Hash Table ****************/
void PFhashInit(void);
PFbpage *PFhashFind(int fd, int page);
int PFhashInsert(int fd, int page, PFbpage *bpage);
int PFhashDelete(int fd, int page);
void PFhashPrint(void);

/****************** Interface functions from Buffer Manager *************/
void PFbufSetStrategy(int strategy); /* New Function */
int PFbufGet(int fd, int pagenum, PFfpage **fpage,
             int (*readfcn)(int, int, PFfpage *),
             int (*writefcn)(int, int, PFfpage *));
int PFbufUnfix(int fd, int pagenum, int dirty);
int PFbufAlloc(int fd, int pagenum, PFfpage **fpage,
             int (*writefcn)(int, int, PFfpage *));
int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage *));
int PFbufUsed(int fd, int pagenum);
void PFbufPrint(void);

/****************** Interface functions from PF (for buf.c) *************/
int PFreadfcn(int fd, int pagenum, PFfpage *buf);
int PFwritefcn(int fd, int pagenum, PFfpage *buf);

#endif /* PFTYPES_H */