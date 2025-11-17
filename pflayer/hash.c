/* hash.c: Functions to facilitate finding the buffer page given
   a file descriptor and a page number */
#include <stdio.h>
#include <stdlib.h> /* For malloc, free */
#include "pf.h"
#include "pftypes.h"

#ifndef PF_DEBUG
#define PF_DEBUG 0
#endif

/* hash table */
static PFhash_entry *PFhashtbl[PF_HASH_TBL_SIZE];

/* Initialize hash table */
void PFhashInit(void)
/****************************************************************************
SPECIFICATIONS:
    Init the hash table entries. Must be called before any of the other
    hash functions are used.
*****************************************************************************/
{
    int i;
    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashInit: initializing hash table\n");
    #endif
    for (i = 0; i < PF_HASH_TBL_SIZE; i++)
        PFhashtbl[i] = NULL;
}

/* Find entry in hash table. Returns PFbpage* or NULL if not found. */
PFbpage *PFhashFind(int fd, int page)
/****************************************************************************
SPECIFICATIONS:
    Given the file descriptor "fd", and page number "page",
    find the buffer address of this particular page.
*****************************************************************************/
{
    int bucket;               /* bucket to look for the page*/
    PFhash_entry *entry;      /* ptr to entries to search */

    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashFind: searching fd=%d page=%d\n", fd, page);
    #endif

    /* See which bucket it is in */
    bucket = PFhash(fd, page);

    /* go through the linked list of this bucket */
    for (entry = PFhashtbl[bucket]; entry != NULL; entry = entry->nextentry) {
        if (entry->fd == fd && entry->page == page) {
            /* found it */
            #if PF_DEBUG
            fprintf(stderr, "DEBUG PFhashFind: FOUND fd=%d page=%d (bucket=%d)\n",
                    fd, page, bucket);
            #endif
            return (entry->bpage);
        }
    }

    /* not found */
    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashFind: NOT FOUND fd=%d page=%d (bucket=%d)\n",
            fd, page, bucket);
    #endif
    return (NULL);
}

/* Insert mapping into hash table */
int PFhashInsert(int fd, int page, PFbpage *bpage)
/*****************************************************************************
SPECIFICATIONS:
    Insert the file descriptor "fd", page number "page", and the
    buffer address "bpage" into the hash table. 
*****************************************************************************/
{
    int bucket;               /* bucket to insert the page */
    PFhash_entry *entry;      /* pointer to new entry */

    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashInsert: attempt insert fd=%d page=%d\n", fd, page);
    #endif

    if (PFhashFind(fd, page) != NULL) {
        /* page already inserted */
        PFerrno = PFE_HASHPAGEEXIST;
        #if PF_DEBUG
        fprintf(stderr, "DEBUG PFhashInsert: already exists fd=%d page=%d -> PFE_HASHPAGEEXIST\n",
                fd, page);
        #endif
        return (PFerrno);
    }

    /* find the bucket for this page */
    bucket = PFhash(fd, page);

    /* allocate mem for new entry */
    entry = (PFhash_entry *)malloc(sizeof(PFhash_entry));
    if (entry == NULL) {
        /* no mem */
        PFerrno = PFE_NOMEM;
        #if PF_DEBUG
        fprintf(stderr, "DEBUG PFhashInsert: malloc failed for fd=%d page=%d -> PFE_NOMEM\n",
                fd, page);
        #endif
        return (PFerrno);
    }

    /* Insert entry as head of list for this bucket */
    entry->fd = fd;
    entry->page = page;
    entry->bpage = bpage;
    entry->nextentry = PFhashtbl[bucket];
    entry->preventry = NULL;
    if (PFhashtbl[bucket] != NULL)
        PFhashtbl[bucket]->preventry = entry;
    PFhashtbl[bucket] = entry;

    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashInsert: inserted fd=%d page=%d into bucket=%d (bpage=%p)\n",
            fd, page, bucket, (void *)bpage);
    #endif

    return (PFE_OK);
}

/* Delete mapping from hash table */
int PFhashDelete(int fd, int page)
/****************************************************************************
SPECIFICATIONS:
    Delete the entry whose file descriptor is "fd", and whose page number
    is "page" from the hash table.
*****************************************************************************/
{
    int bucket;              /* bucket for this page */
    PFhash_entry *entry;     /* entry to look for */

    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashDelete: attempt delete fd=%d page=%d\n", fd, page);
    #endif

    /* find the bucket */
    bucket = PFhash(fd, page);

    /* See if the entry is in this bucket */
    for (entry = PFhashtbl[bucket]; entry != NULL; entry = entry->nextentry)
        if (entry->fd == fd && entry->page == page)
            break;

    if (entry == NULL) {
        /* not found */
        PFerrno = PFE_HASHNOTFOUND;
        #if PF_DEBUG
        fprintf(stderr, "DEBUG PFhashDelete: NOT FOUND fd=%d page=%d -> PFE_HASHNOTFOUND\n",
                fd, page);
        #endif
        return (PFerrno);
    }

    /* get rid of this entry */
    if (entry == PFhashtbl[bucket])
        PFhashtbl[bucket] = entry->nextentry;
    if (entry->preventry != NULL)
        entry->preventry->nextentry = entry->nextentry;
    if (entry->nextentry != NULL)
        entry->nextentry->preventry = entry->preventry;

    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFhashDelete: deleted fd=%d page=%d from bucket=%d (bpage=%p)\n",
            fd, page, bucket, (void *)entry->bpage);
    #endif

    free((char *)entry);

    return (PFE_OK);
}

/* Print the hash table (for debugging) */
void PFhashPrint(void)
/****************************************************************************
SPECIFICATIONS:
    Print the hash table entries.
*****************************************************************************/
{
    int i;
    PFhash_entry *entry;

    for (i = 0; i < PF_HASH_TBL_SIZE; i++) {
        printf("bucket %d\n", i);
        if (PFhashtbl[i] == NULL)
            printf("\tempty\n");
        else {
            for (entry = PFhashtbl[i]; entry != NULL; entry = entry->nextentry)
                printf("\tfd: %d, page: %d, bpage: %p\n",
                       entry->fd, entry->page, (void *)entry->bpage);
        }
    }
}
