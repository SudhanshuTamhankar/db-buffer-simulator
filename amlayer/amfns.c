#include "am.h"

/* Creates a secondary idex file called fileName.indexNo */
int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength) {
  char *pageBuf; /* buffer for holding a page */
  char indexfName[AM_MAX_FNAME_LENGTH]; /* String to store the indexed
                       files name with extension           */
  int pageNum;  /* page number of the root page(also the first page) */
  int fileDesc; /* file Descriptor */
  int errVal;
  int maxKeys; /* Maximum keys that can be held on one internal page */
  AM_LEAFHEADER head, *header;

  /* Check the parameters */
  if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
    AM_Errno = AME_INVALIDATTRTYPE;
    return (AME_INVALIDATTRTYPE);
  }

  if ((attrLength < 1) || (attrLength > 255)) {
    AM_Errno = AME_INVALIDATTRLENGTH;
    return (AME_INVALIDATTRLENGTH);
  }

  if (attrLength != 4)
    if (attrType != 'c') {
      AM_Errno = AME_INVALIDATTRLENGTH;
      return (AME_INVALIDATTRLENGTH);
    }

  header = &head;

  /* Get the filename with extension and create a paged file by that name*/
  sprintf(indexfName, "%s.%d", fileName, indexNo);
  errVal = PF_CreateFile(indexfName);
  AM_Check(errVal);

  /* open the new file */
  fileDesc = PF_OpenFile(indexfName);
  if (fileDesc < 0) {
    AM_Errno = AME_PF;
    return (AME_PF);
  }

  /* allocate a new page for the root */
  errVal = PF_AllocPage(fileDesc, &pageNum, &pageBuf);
  AM_Check(errVal);

  /* initialise the header */
  header->pageType = 'l';
  header->nextLeafPage = AM_NULL_PAGE;
  header->recIdPtr = PF_PAGE_SIZE;
  header->keyPtr = AM_sl;
  header->freeListPtr = AM_NULL;
  header->numinfreeList = 0;
  header->attrLength = attrLength;
  header->numKeys = 0;
  /* the maximum keys in an internal node- has to be even always*/
  maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
  if ((maxKeys % 2) != 0)
    header->maxKeys = maxKeys - 1;
  else
    header->maxKeys = maxKeys;
  /* copy the header onto the page */
  memcpy(pageBuf, header, AM_sl);

  errVal = PF_UnfixPage(fileDesc, pageNum, TRUE);
  AM_Check(errVal);

  /* Close the file */
  errVal = PF_CloseFile(fileDesc);
  AM_Check(errVal);

  /* initialise the root page and the leftmost page numbers */
  AM_RootPageNum = pageNum;
  return (AME_OK);
}

/* Destroys the index fileName.indexNo */
int AM_DestroyIndex(char *fileName, int indexNo) {
  char indexfName[AM_MAX_FNAME_LENGTH];
  int errVal;

  sprintf(indexfName, "%s.%d", fileName, indexNo);
  errVal = PF_DestroyFile(indexfName);
  AM_Check(errVal);
  return (AME_OK);
}

/* Deletes the recId from the list for value and deletes value if list
becomes empty */
int AM_DeleteEntry(int fileDesc, char attrType, int attrLength, char *value,
                   int recId) {
  char *pageBuf;   /* buffer to hold the page */
  int pageNum;     /* page Number of the page in buffer */
  int index;       /* index where key is present */
  int status;      /* whether key is in tree or not */
  short nextRec;   /* contains the next record on the list */
  short oldhead;   /* contains the old head of the list */
  short temp;
  char *currRecPtr;      /* pointer to the current record in the list */
  AM_LEAFHEADER head, *header; /* header of the page */
  int recSize;           /* length of key,ptr pair for a leaf */
  int tempRec;           /* holds the recId of the current record */
  int i;                 /* loop index */

  /* check the parameters */
  if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
    AM_Errno = AME_INVALIDATTRTYPE;
    return (AME_INVALIDATTRTYPE);
  }

  if (value == NULL) {
    AM_Errno = AME_INVALIDVALUE;
    return (AME_INVALIDVALUE);
  }

  if (fileDesc < 0) {
    AM_Errno = AME_FD;
    return (AME_FD);
  }

  /* initialise the header */
  header = &head;

  /* find the pagenumber and the index of the key to be deleted if it is
  there */
  status = AM_Search(fileDesc, attrType, attrLength, value, &pageNum, &pageBuf,
                     &index);

  /* check if return value is an error */
  if (status < 0) {
    AM_Errno = status;
    return (status);
  }

  /* The key is not in the tree */
  if (status == AM_NOT_FOUND) {
    AM_Errno = AME_NOTFOUND;
    return (AME_NOTFOUND);
  }

  memcpy(header, pageBuf, AM_sl);
  recSize = attrLength + AM_ss;
  currRecPtr = pageBuf + AM_sl + (index - 1) * recSize + attrLength;
  memcpy(&nextRec, currRecPtr, AM_ss);

  /* search the list for recId */
  while (nextRec != 0) {
    memcpy(&tempRec, pageBuf + nextRec, AM_si);

    /* found the recId to be deleted */
    if (recId == tempRec) {
      /* Delete recId */
      memcpy(currRecPtr, pageBuf + nextRec + AM_si, AM_ss);
      header->numinfreeList++;
      oldhead = header->freeListPtr;
      header->freeListPtr = nextRec;
      memcpy(pageBuf + nextRec + AM_si, &oldhead, AM_ss);
      break;
    } else {
      /* go over to the next item on the list */
      currRecPtr = pageBuf + nextRec + AM_si;
      memcpy(&nextRec, currRecPtr, AM_ss);
    }
  }

  /* if end of list reached then key not in tree */
  if (nextRec == AM_NULL) {
    AM_Errno = AME_NOTFOUND;
    return (AME_NOTFOUND);
  }

  /* check if list is empty */
  memcpy(&temp, pageBuf + AM_sl + (index - 1) * recSize + attrLength, AM_ss);
  if (temp == 0) {
    /* list is empty , so delete key from the list */
    for (i = index; i < (header->numKeys); i++)
      memcpy(pageBuf + AM_sl + (i - 1) * recSize, pageBuf + AM_sl + i * recSize,
             recSize);
    header->numKeys--;
    header->keyPtr = header->keyPtr - recSize;
  }

  /* copy the header onto the buffer */
  memcpy(pageBuf, header, AM_sl);

  /* Unfix the page, it was modified */
  PF_UnfixPage(fileDesc, pageNum, TRUE);

  /* empty the stack so that it is set for next amlayer call */
  AM_EmptyStack();
  AM_Errno = AME_OK;
  return (AME_OK);
}

/* Inserts a value,recId pair into the tree */
int AM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value,
                   int recId) {
  char *pageBuf; /* buffer to hold page */
  int pageNum;   /* page number of the page in buffer */
  int index;     /* index where key can be found or can be inserted */
  int status;    /* whether key is old or new */
  int inserted;  /* Whether key has been inserted into the leaf or
                        splitting is needed */
  int addtoparent; /* Whether key has to be added to the parent */
  int errVal;      /* return value of functions within this function */
  char key[AM_MAXATTRLENGTH]; /* holds the attribute to be passed
                                   back to the parent */

  /* check the parameters */
  if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
    AM_Errno = AME_INVALIDATTRTYPE;
    return (AME_INVALIDATTRTYPE);
  }

  if (value == NULL) {
    AM_Errno = AME_INVALIDVALUE;
    return (AME_INVALIDVALUE);
  }

  if (fileDesc < 0) {
    AM_Errno = AME_FD;
    return (AME_FD);
  }

  /* Search the leaf for the key */
  status = AM_Search(fileDesc, attrType, attrLength, value, &pageNum, &pageBuf,
                     &index);

  /* check if there is an error */
  if (status < 0) {
    AM_EmptyStack();
    AM_Errno = status;
    return (status);
  }

  /* Insert into leaf the key,recId pair */
  inserted =
      AM_InsertintoLeaf(pageBuf, attrLength, value, recId, index, status);

  /*
   * ========================================================
   * BUG FIX: Empty stack after successful insert.
   * ========================================================
   */
  if (inserted == TRUE) {
    // fprintf(stderr, "DEBUG AM_InsertEntry -> PF_UnfixPage: fileDesc=%d pageNum=%d (after insert)\n",
    //         fileDesc, pageNum);
    errVal = PF_UnfixPage(fileDesc, pageNum, TRUE);
    AM_Check(errVal);
    AM_EmptyStack(); /* Clear the stack after successful insert */
    return (AME_OK);
  }


  /* check if there is any error */
  if (inserted < 0) {
    AM_EmptyStack();
    AM_Errno = inserted;
    return (inserted);
  }

  /* if not inserted then have to split */
  if (inserted == FALSE) {
    /* Split the leaf page */
    addtoparent = AM_SplitLeaf(fileDesc, pageBuf, &pageNum, attrLength, recId,
                             value, status, index, key);

    /* check for errors */
    if (addtoparent < 0) {
      AM_EmptyStack();
      AM_Errno = addtoparent;
      return (addtoparent);
    }

    /* if key has to be added to the parent */
    if (addtoparent == TRUE) {
      errVal = AM_AddtoParent(fileDesc, pageNum, key, attrLength);
      if (errVal < 0) {
        AM_EmptyStack();
        AM_Errno = errVal;
        return (errVal);
      }
    }
  }
  AM_EmptyStack();
  return (AME_OK);
}

/* error messages */
static char *AMerrormsg[] = {
    "No error",
    "Invalid Attribute Length",
    "Key Not Found in Tree",
    "PF error",
    "Internal error - contact database manager immediately",
    "Invalid scan Descriptor",
    "Invalid operator to OpenIndexScan",
    "Scan Over",
    "Scan Table is full",
    "Invalid Attribute Type",
    "Invalid file Descriptor",
    "Invalid value to Delete or Insert Entry"};

void AM_PrintError(char *s) {
  fprintf(stderr, "%s", s);
  fprintf(stderr, ": %s", AMerrormsg[-1 * AM_Errno]);
  if (AM_Errno == AME_PF)
    /* print the PF error message */
    PF_PrintError(" ");
  else
    fprintf(stderr, "\n");
}