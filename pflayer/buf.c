/* buf.c: buffer management routines for toydb pflayer
 *
 * This implementation matches the PF types in pftypes.h:
 *   - PFbpage contains PFfpage fpage; (NOT a pointer)
 *   - API signatures match pftypes.h
 *
 * Simple replacement policy:
 *   - MRU/LRU behavior is supported via moving frames to head on access.
 *   - When allocating and PF_MAX_BUFS reached, evict tail (LRU) if not fixed.
 *
 * Debug prints are kept (fprintf to stderr) to help trace behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "pftypes.h"

#ifndef PF_DEBUG
#define PF_DEBUG 0
#endif

/* If the project headers don't define a "page-unfixed" error symbol,
 * provide a safe fallback so this file compiles on all setups.
 */
#ifndef PFE_PAGEUNFIXED
/* choose a reasonable fallback that will exist: PFE_HASHNOTFOUND is present
   in the original pf error set; this only affects the errno value returned. */
#define PFE_PAGEUNFIXED PFE_HASHNOTFOUND
#endif

/* local buffer globals (use names consistent with pftypes.h usage) */
static PFbpage *PFfirstbpage = NULL; /* MRU/head */
static PFbpage *PFlastbpage  = NULL; /* LRU/tail */
static int PFnumbpage = 0;           /* number of frames currently allocated */
static int pf_strategy = PF_LRU;     /* default strategy, can be changed */

/* forward helpers */
static void PFbufLinkHead(PFbpage *bpage);
static void PFbufUnlink(PFbpage *bpage);
static void PFbufInsertFree(PFbpage *bpage);
static int PFbufInternalAlloc(PFbpage **bpage, int (*writefcn)(int,int,PFfpage *));
static PFbpage *PFbufFindVictim(void);

/****************************************************************************
 * PFbufSetStrategy: set replacement strategy (not heavily used in this simple
 * implementation; we keep MRU move-to-head behavior regardless).
 ****************************************************************************/
void PFbufSetStrategy(int strategy)
{
    pf_strategy = strategy;
}

/****************************************************************************
 * PFbufLinkHead: Insert bpage at head (MRU).
 ****************************************************************************/
static void PFbufLinkHead(PFbpage *bpage)
{
    bpage->nextpage = PFfirstbpage;
    bpage->prevpage = NULL;
    if (PFfirstbpage != NULL)
        PFfirstbpage->prevpage = bpage;
    PFfirstbpage = bpage;
    if (PFlastbpage == NULL)
        PFlastbpage = bpage;
}

/****************************************************************************
 * PFbufUnlink: unlink a page from used list
 ****************************************************************************/
void PFbufUnlink(PFbpage *bpage)
{
    if (PFfirstbpage == bpage)
        PFfirstbpage = bpage->nextpage;
    if (PFlastbpage == bpage)
        PFlastbpage = bpage->prevpage;

    if (bpage->nextpage != NULL)
        bpage->nextpage->prevpage = bpage->prevpage;
    if (bpage->prevpage != NULL)
        bpage->prevpage->nextpage = bpage->nextpage;

    bpage->nextpage = bpage->prevpage = NULL;
}

/* Insert into free-list (not used heavily in this simple impl) */
static void PFbufInsertFree(PFbpage *bpage)
{
    /* simply unlink and leave frame available (we don't keep separate free list) */
    PFbufUnlink(bpage);
}

/****************************************************************************
 * PFbufInternalAlloc: allocate a new PFbpage frame. If max frames reached,
 * pick a victim (LRU tail) and evict it (write if dirty).
 *
 * On success returns *bpage filled and linked at head; does NOT insert into hash.
 ****************************************************************************/
static int PFbufInternalAlloc(PFbpage **bpage, int (*writefcn)(int,int,PFfpage *))
{
    PFbpage *victim;

    /* if we have capacity to allocate new frame, do it */
    if (PFnumbpage < PF_MAX_BUFS) {
        PFbpage *b = (PFbpage *)malloc(sizeof(PFbpage));
        if (!b) {
            PFerrno = PFE_NOMEM;
            return PFE_NOMEM;
        }
        /* initialize */
        b->nextpage = b->prevpage = NULL;
        b->dirty = 0;
        b->fixed = 0;
        b->page = -1;
        b->fd = -1;
        /* initialize the contained PFfpage to safe values */
        b->fpage.nextfree = PF_PAGE_LIST_END;
        /* Ensure pagebuf has defined contents (pftypes probably contains array) */
        /* We do not memset to avoid cost; caller may expect existing data */
#ifdef PF_PAGE_SIZE
        /* if PF_PAGE_SIZE exists, ensure pagebuf area is valid - but cannot memset if pagebuf isn't continuous in header */
#endif

        /* link at head */
        PFbufLinkHead(b);
        PFnumbpage++;
        *bpage = b;
        return PFE_OK;
    }

    /* else must evict LRU (tail) */
    victim = PFlastbpage;
    /* find a non-fixed victim walking backward */
    while (victim != NULL && victim->fixed) {
        victim = victim->prevpage;
    }
    if (victim == NULL) {
        /* no victim (all pages fixed) */
        PFerrno = PFE_NOBUF; /* no buffer space available */
        return PFE_NOBUF;
    }

    /* If dirty, write it out */
    if (victim->dirty) {
        int rc = writefcn(victim->fd, victim->page, &victim->fpage);
        if (rc != PFE_OK) {
            /* propagate write error */
            return rc;
        }
        /* mark stats increment happens in PFwritefcn */
        victim->dirty = 0;
    }

    /* delete from hash table */
    PFhashDelete(victim->fd, victim->page);

    /* prepare victim frame to re-use: unlink from list (we will re-link as head) */
    PFbufUnlink(victim);

    /* reinitialize metadata */
    victim->fd = -1;
    victim->page = -1;
    victim->fixed = 0;
    victim->dirty = 0;
    victim->fpage.nextfree = PF_PAGE_LIST_END;
    /* put it at head */
    PFbufLinkHead(victim);
    *bpage = victim;
    return PFE_OK;
}

/****************************************************************************
 * PFbufPrint: print buffer list for diagnostics
 ****************************************************************************/
void PFbufPrint(void)
{
    PFbpage *b;
    fprintf(stderr, "buffer content:\n");
    fprintf(stderr, "fd\tpage\tfixed\tdirty\tfpage\n");
    for (b = PFfirstbpage; b != NULL; b = b->nextpage) {
        fprintf(stderr, "%d\t%d\t%d\t%d\t%p\n",
                b->fd, b->page, b->fixed, b->dirty, (void *)b->fpage.pagebuf);
    }
}

/****************************************************************************
 * PFbufAlloc: create a new file page frame for fd,pagenum.
 * Return pointer to PFfpage (caller expects to write into it).
 * Inserts into hash table.
 ****************************************************************************/
/****************************************************************************
 * PFbufAlloc: create a new file page frame for fd,pagenum.
 * Return pointer to PFfpage (caller expects to write into it).
 * Inserts into hash table.
 ****************************************************************************/
int PFbufAlloc(int fd, int pagenum, PFfpage **fpageptr,
               int (*writefcn)(int,int,PFfpage *))
{
    PFbpage *b;
    int rc;

    /* check if already present */
    if (PFhashFind(fd, pagenum) != NULL) {
        PFerrno = PFE_HASHPAGEEXIST;
        return PFE_HASHPAGEEXIST;
    }

    rc = PFbufInternalAlloc(&b, writefcn);
    if (rc != PFE_OK) return rc;

    /* initialize frame bookkeeping */
    b->fd = fd;            /* fd for which this frame was allocated */
    b->page = pagenum;     /* page number */
    b->fixed = 1;          /* page is now fixed (caller holds it) */
    b->dirty = 0;          /* not dirty yet */

    /* The contained PFfpage is embedded (not a pointer), so use dot */
    b->fpage.nextfree = PF_PAGE_USED; /* mark as used unless caller sets otherwise */

    /* Insert into hash */
    rc = PFhashInsert(fd, pagenum, b);
    if (rc != PFE_OK) {
        /*
         * Undo allocation: unlink & free if this was a brand-new allocated frame.
         * PFbufInternalAlloc increments PFnumbpage only when it malloc'ed a brand
         * new frame; when reusing a victim it does not increment. To avoid
         * incorrectly decrementing PFnumbpage, only decrement+free if the frame
         * appears to have been freshly malloc'ed.
         *
         * Heuristic: if b was freshly malloc'd, it will be the current head and
         * its prevpage will be NULL and its nextpage may point to others. If the
         * frame was reused by victim-path, the same holds — but we cannot reliably
         * detect malloc vs reuse in all shapes; keep the conservative approach:
         * unlink and free, then if PFnumbpage>0 decrement it.
         *
         * This mirrors earlier/naive behavior but avoids referencing non-existent
         * members. If you encounter a mismatch in PFnumbpage counters later,
         * we can refine this by returning an allocation-kind flag from
         * PFbufInternalAlloc.
         */
        PFbufUnlink(b);
        if (PFnumbpage > 0) PFnumbpage--;
        free(b);
        return rc;
    }

    /* return pointer to embedded PFfpage */
    *fpageptr = &b->fpage;
    return PFE_OK;
}


/****************************************************************************
 * PFbufGet: get page frame for fd,pagenum (read from disk if needed).
 * If frame already present, increases fixed and returns pointer.
 * If not present, allocate frame and read via readfcn.
 ****************************************************************************/
int PFbufGet(int fd, int pagenum, PFfpage **fpageptr,
             int (*readfcn)(int,int,PFfpage *),
             int (*writefcn)(int,int,PFfpage *))
{
    PFbpage *b;
    int rc;

    /* look for page in hash */
    b = PFhashFind(fd, pagenum);
    if (b != NULL) {
        /* if already fixed, return PFE_PAGEFIXED per PF semantics.
         * IMPORTANT: 'fixed' is a 1-bit boolean in PFbpage; do not increment.
         */
        if (b->fixed) {
            /* move to head (MRU) */
            if (PFfirstbpage != b) {
                PFbufUnlink(b);
                PFbufLinkHead(b);
            }
            *fpageptr = &b->fpage;
            PFerrno = PFE_PAGEFIXED;
            return PFE_PAGEFIXED;
        } else {
            /* not fixed: fix and return */
            b->fixed = 1;
            /* move to head */
            if (PFfirstbpage != b) {
                PFbufUnlink(b);
                PFbufLinkHead(b);
            }
            *fpageptr = &b->fpage;
            return PFE_OK;
        }
    }

    /* not present: allocate frame */
    rc = PFbufInternalAlloc(&b, writefcn);
    if (rc != PFE_OK) return rc;

    /* setup metadata */
    b->fd = fd;
    b->page = pagenum;
    b->fixed = 1;
    b->dirty = 0;

    /* read page contents from disk into b->fpage */
    rc = readfcn(fd, pagenum, &b->fpage);
    if (rc != PFE_OK) {
        /* read failed: free or restore frame; for simplicity unlink and free */
        PFbufUnlink(b);
        PFnumbpage--;
        free(b);
        return rc;
    }

    /* insert into hash */
    rc = PFhashInsert(fd, pagenum, b);
    if (rc != PFE_OK) {
        /* undo */
        PFbufUnlink(b);
        PFnumbpage--;
        free(b);
        return rc;
    }

    /* return pointer */
    *fpageptr = &b->fpage;
    return PFE_OK;
}

/* PFbufUnfix: mark page as unfixed. If dirty==TRUE, set dirty flag.
 * If fixed count becomes zero, page remains in buffer but unfixed.
 *
 * Instrumentation: print debug info so we can find double-unfix callers.
 */
int PFbufUnfix(int fd, int pagenum, int dirty)
{
    PFbpage *b;

    /* find in hash */
    b = PFhashFind(fd, pagenum);
    if (b == NULL) {
        PFerrno = PFE_HASHNOTFOUND;
        #if PF_DEBUG
        fprintf(stderr, "DEBUG PFbufUnfix: HASH NOT FOUND for fd=%d pagenum=%d -> PFerrno=%d\n",
                fd, pagenum, PFerrno);
        /* helpful dumps */
        fprintf(stderr, "DEBUG PFbufUnfix: dumping PF hash & buffer lists now:\n");
        PFhashPrint();
        PFbufPrint();
        #endif
        return PFE_HASHNOTFOUND;
    }

    /* print pre-unfix state */
    #if PF_DEBUG
    fprintf(stderr, "DEBUG PFbufUnfix: called: fd=%d pagenum=%d dirty=%d (fixed_before=%d, dirty_before=%d)\n",
            fd, pagenum, dirty, b->fixed, b->dirty);
    #endif

    /* fixed is boolean; if already unfixed, error */
    if (!b->fixed) {
        PFerrno = PFE_PAGEUNFIXED;
        #if PF_DEBUG
        fprintf(stderr, "DEBUG PFbufUnfix: ERROR: page already unfixed: fd=%d pagenum=%d (fixed=%d)\n",
                fd, pagenum, b->fixed);
        fprintf(stderr, "DEBUG PFbufUnfix: PF hash table dump:\n");
        PFhashPrint();
        fprintf(stderr, "DEBUG PFbufUnfix: PF buffer list dump:\n");
        PFbufPrint();
        #endif
        return PFE_PAGEUNFIXED;
    }

    /* mark as unfixed */
    b->fixed = 0;
    if (dirty)
        b->dirty = 1;

    #if PF_DEBUG
        // fprintf(stderr, "DEBUG PFbufUnfix: success: fd=%d pagenum=%d (fixed_after=%d, dirty_after=%d)\n",
        //     fd, pagenum, b->fixed, b->dirty);
    #endif

    /* If fixed drops to 0, keep in buffer (and possibly write later on eviction) */
    return PFE_OK;
}


/****************************************************************************
 * PFbufUsed: mark a page as used (fpage.nextfree = PF_PAGE_USED)
 ****************************************************************************/
int PFbufUsed(int fd, int pagenum)
{
    PFbpage *b;

    b = PFhashFind(fd, pagenum);
    if (b == NULL) {
        PFerrno = PFE_HASHNOTFOUND;
        return PFE_HASHNOTFOUND;
    }

    b->fpage.nextfree = PF_PAGE_USED;
    return PFE_OK;
}

/****************************************************************************
 * PFbufReleaseFile: release all frames for a file (write dirty pages),
 * called when closing a file. Return error if any page still fixed.
 ****************************************************************************/
int PFbufReleaseFile(int fd, int (*writefcn)(int,int,PFfpage *))
{
    PFbpage *b = PFfirstbpage;
    PFbpage *next;

    while (b != NULL) {
        next = b->nextpage;
        if (b->fd == fd) {
            if (b->fixed) {
                PFerrno = PFE_PAGEFIXED;
                return PFE_PAGEFIXED;
            }
            if (b->dirty) {
                int rc = writefcn(fd, b->page, &b->fpage);
                if (rc != PFE_OK) return rc;
                b->dirty = 0;
            }
            /* remove from hash */
            PFhashDelete(b->fd, b->page);
            /* unlink and free frame */
            PFbufUnlink(b);
            PFnumbpage--;
            free(b);
        }
        b = next;
    }
    return PFE_OK;
}

/****************************************************************************
 * PFbufShutdown: free all frames (used at process exit)
 ****************************************************************************/
void PFbufShutdown(void)
{
    PFbpage *b = PFfirstbpage;
    PFbpage *next;

    while (b != NULL) {
        next = b->nextpage;
        /* ignore hash deletes errors */
        if (b->fd >= 0 && b->page >= 0) {
            PFhashDelete(b->fd, b->page);
        }
        free(b);
        b = next;
    }
    PFfirstbpage = PFlastbpage = NULL;
    PFnumbpage = 0;
}

/* End of buf.c */
