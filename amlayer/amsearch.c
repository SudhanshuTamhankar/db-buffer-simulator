#include "am.h"

/* searches for a key in a binary tree - returns FOUND or NOTFOUND and
returns the pagenumber and the offset where key is present or could
be inserted */
int AM_Search(int fileDesc, char attrType, int attrLength, char *value,
              int *pageNum, char **pageBuf, int *indexPtr) {
  int errVal;
  int nextPage; /* next page to be followed on the path from root to leaf*/
  AM_LEAFHEADER lhead, *lheader; /* local pointer to leaf header */
  AM_INTHEADER ihead, *iheader;  /* local pointer to internal node header */

  /* initialise the headeers */
  lheader = &lhead;
  iheader = &ihead;

  /* get the root of the B+ tree */
  errVal = PF_GetFirstPage(fileDesc, pageNum, pageBuf);
  AM_Check(errVal);
  
  AM_RootPageNum = *pageNum; /* Save root page num */

  if (**pageBuf == 'l')
  /* if root is a leaf page */
  {
    memcpy(lheader, *pageBuf, AM_sl);
    if (lheader->attrLength != attrLength) {
      PF_UnfixPage(fileDesc, *pageNum, FALSE); /* Unfix before returning */
      return (AME_INVALIDATTRLENGTH);
    }
  } else /* root is not a leaf */
  {
    memcpy(iheader, *pageBuf, AM_sint);
    if (iheader->attrLength != attrLength) {
      PF_UnfixPage(fileDesc, *pageNum, FALSE); /* Unfix before returning */
      return (AME_INVALIDATTRLENGTH);
    }
  }
  
  /* * find the leaf at which key is present or can be inserted.
   * The stack should only contain the path of *internal* nodes.
   */
  while ((**pageBuf) != 'l') {
    
    /* find the next page to be followed */
    /* We use iheader here. It's correct for the *current* page. */
    nextPage =
        AM_BinSearch(*pageBuf, attrType, attrLength, value, indexPtr, iheader);

    /*
     * ========================================================
     * BUG FIX: Push the *PARENT* (which is an internal node)
     * ========================================================
     */
    AM_PushStack(*pageNum, *indexPtr);

    errVal = PF_UnfixPage(fileDesc, *pageNum, FALSE);
    AM_Check(errVal);

    /* set pageNum to the next page to be followed */
    *pageNum = nextPage;

    /* Get the next page to be followed */
    errVal = PF_GetThisPage(fileDesc, *pageNum, pageBuf);
    
    /*
     * ========================================================
     * BUG FIX: Check for error *before* accessing pageBuf
     * ========================================================
     */
    if (errVal != PFE_OK) {
        /* If GetThisPage fails, we must return an error */
        /* AM_Check will handle the return */
        AM_Check(errVal);
    }

    if (**pageBuf == 'l') {
      /* if next page is a leaf */
      memcpy(lheader, *pageBuf, AM_sl);
      if (lheader->attrLength != attrLength) {
        PF_UnfixPage(fileDesc, *pageNum, FALSE);
        return (AME_INVALIDATTRLENGTH);
      }
    } else {
      /* if next page is an internal node */
      memcpy(iheader, *pageBuf, AM_sint); /* This updates iheader for the next loop iter */
      if (iheader->attrLength != attrLength) {
        PF_UnfixPage(fileDesc, *pageNum, FALSE);
        return (AME_INVALIDATTRLENGTH);
      }
    }
  }
  /* find whether key is in leaf or not */
  return (AM_SearchLeaf(*pageBuf, attrType, attrLength, value, indexPtr, lheader));
}

/* Finds the place (index) from where the next page to be followed is got*/
int AM_BinSearch(char *pageBuf, char attrType, int attrLength, char *value,
                 int *indexPtr, AM_INTHEADER *header) {
  int low, high, mid; /* for binary search */
  int compareVal;     /* result of comparison of key with value */
  int recSize;        /* size in bytes of a key,ptr pair */
  int pageNum;        /* page number of node to be followed along the B+ tree */

  recSize = AM_si + attrLength;
  low = 0;
  high = header->numKeys - 1;
  
  /* Binary search over the keys */
  while (low <= high) {
    mid = (low + high) / 2;
    compareVal = AM_Compare(pageBuf + AM_sint + AM_si + (mid * recSize),
                            attrType, attrLength, value);

    if (compareVal == 0) {
      /* Found exact match */
      *indexPtr = mid + 1;
      memcpy(&pageNum, pageBuf + AM_sint + (mid + 1) * recSize, AM_si);
      return pageNum;
    } else if (compareVal < 0) {
      /* value < key[mid] */
      high = mid - 1;
    } else {
      /* value > key[mid] */
      low = mid + 1;
    }
  }
  
  /*
   * No exact match found. 'low' is now the insertion point,
   * which means we follow the pointer *at* index 'low'.
   */
  *indexPtr = low;
  memcpy(&pageNum, pageBuf + AM_sint + low * recSize, AM_si);
  return pageNum;
}

/* search a leaf node for the key- returns the place where it is found or can
be inserted */
int AM_SearchLeaf(char *pageBuf, char attrType, int attrLength, char *value,
                  int *indexPtr, AM_LEAFHEADER *header) {
  int low, high, mid; /* for binary search */
  int compareVal;     /* result of comparison of key with value */
  int recSize;        /* size in bytes of a key,ptr pair */

  recSize = AM_ss + attrLength;
  low = 0;
  high = header->numKeys - 1;

  /* The leaf is Empty */
  if (high < 0) {
    *indexPtr = 1; /* C-style index is 0, 1-based index is 1 */
    return (AM_NOT_FOUND);
  }

  /* Binary search over the keys */
  while (low <= high) {
    mid = (low + high) / 2;
    compareVal = AM_Compare(pageBuf + AM_sl + (mid * recSize),
                            attrType, attrLength, value);
    
    if (compareVal == 0) {
      /* Found exact match */
      *indexPtr = mid + 1; /* Convert 0-based to 1-based index */
      return (AM_FOUND);
    } else if (compareVal < 0) {
      /* value < key[mid] */
      high = mid - 1;
    } else {
      /* value > key[mid] */
      low = mid + 1;
    }
  }

  /*
   * No exact match found. 'low' is the insertion point.
   * Return 1-based index.
   */
  *indexPtr = low + 1;
  return (AM_NOT_FOUND);
}

/* Compare value in bufPtr with value in valPtr - returns -1 ,0 or 1 according
to whether value in valPtr is less than , equal to or greater than value
in BufPtr*/
int AM_Compare(char *bufPtr, char attrType, int attrLength, char *valPtr) {
  int bufint, valint;     /* temporary aligned storage for comparison */
  float buffloat, valfloat; /* temporary aligned storage for comparison */

  switch (attrType) {
  case 'i': {
    memcpy(&bufint, bufPtr, AM_si);
    memcpy(&valint, valPtr, AM_si);
    if (valint < bufint) return (-1);
    else if (valint > bufint) return (1);
    else return (0);
  }
  case 'f': {
    memcpy(&buffloat, bufPtr, AM_sf);
    memcpy(&valfloat, valPtr, AM_sf);
    if (valfloat < buffloat) return (-1);
    else if (valfloat > buffloat) return (1);
    else return (0);
  }
  case 'c': {
    /* strncmp returns < 0, 0, or > 0 */
    return (strncmp(valPtr, bufPtr, attrLength));
  }
  }
  return 0; /* Should never reach here */
}