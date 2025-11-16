#include "am.h"

/* splits a leaf node */
int AM_SplitLeaf(int fileDesc, char *pageBuf, int *pageNum, int attrLength,
                 int recId, char *value, int status, int index, char *key) {
  AM_LEAFHEADER head, temphead; /* local header */
  AM_LEAFHEADER *header, *tempheader;
  char tempPage[PF_PAGE_SIZE]; /* temporary page for manipulation on the page */
  char *tempPageBuf, *tempPageBuf1; /* buffers for new pages to be allocated */
  int errVal;
  int tempPageNum, tempPageNum1; /* pagenumbers for pages to be allocated */

  /* initialise pointers to headers */
  header = &head;
  tempheader = &temphead;

  /* copy header from buffer */
  memcpy(header, pageBuf, AM_sl);

  /* compact half the keys into temporary page */
  AM_Compact(1, (header->numKeys) / 2, pageBuf, tempPage, header);

  /* Allocate a new page for the other half of the leaf*/
  errVal = PF_AllocPage(fileDesc, &tempPageNum, &tempPageBuf);
  AM_Check(errVal);

  /* compact the other half keys */
  AM_Compact((header->numKeys) / 2 + 1, header->numKeys, pageBuf, tempPageBuf,
             header);

  /*check where key has to be inserted */
  if (index <= ((header->numKeys) / 2)) {
    /*value to be inserted is in first half */
    errVal =
        AM_InsertintoLeaf(tempPage, attrLength, value, recId, index, status);
  } else {
    /* value to be inserted in second half */
    index = index - ((header->numKeys) / 2);
    errVal = AM_InsertintoLeaf(tempPageBuf, attrLength, value, recId, index,
                             status);
  }

  /* change the next leafpage of first half of leaf to second half */
  memcpy(tempheader, tempPage, AM_sl);
  tempheader->nextLeafPage = tempPageNum;
  memcpy(tempPage, tempheader, AM_sl);
  memcpy(pageBuf, tempPage, PF_PAGE_SIZE);

  /* copy the value of key to be written onto the parent */
  memcpy(key, tempPageBuf + AM_sl, attrLength);

  /*check if the split page is root */
  if ((*pageNum) == AM_RootPageNum) {
    /* the page being split is the root*/
    /* Allocate a new page for another leaf as a new root has
    to be created*/
    errVal = PF_AllocPage(fileDesc, &tempPageNum1, &tempPageBuf1);
    AM_Check(errVal);

    AM_LeftPageNum = tempPageNum1; /* this will remain the
                             leftmost page hence*/

    /* copy the old first half(actually the root) into a new page */
    memcpy(tempPageBuf1, pageBuf, PF_PAGE_SIZE);
    /* Initialise the new root page */

    AM_FillRootPage(pageBuf, tempPageNum1, tempPageNum, key, header->attrLength,
                    header->maxKeys);
    errVal = PF_UnfixPage(fileDesc, tempPageNum1, TRUE);
    AM_Check(errVal);
  }
  
  /* Unfix the new page (the second half of the split) */
  errVal = PF_UnfixPage(fileDesc, tempPageNum, TRUE);
  AM_Check(errVal);

  if ((*pageNum) == AM_RootPageNum) {
    /* Unfix the original page (which is now the new internal root) */
    errVal = PF_UnfixPage(fileDesc, *pageNum, TRUE);
    AM_Check(errVal);
    return (FALSE);
  }
  else {
    /* Unfix the original page (which is now just the first half) */
    errVal = PF_UnfixPage(fileDesc, *pageNum, TRUE);
    AM_Check(errVal);

    *pageNum = tempPageNum;
    return (TRUE);
  }
}

/* Adds to the parent(on top of the path stack) attribute value and page Number*/
int AM_AddtoParent(int fileDesc, int pageNum, char *value, int attrLength) {
  char tempPage[PF_PAGE_SIZE]; /* temporary page for manipulating page */
  int pageNumber; /* pageNumber of parent to which key is to be added-
                                     got from stack*/
  int offset;     /* Place in parent where key is to be added -
                       got from stack*/
  int errVal;
  int pageNum1, pageNum2; /* pagenumbers for new pages to be allocated */

  char *pageBuf, *pageBuf1, *pageBuf2;
  AM_INTHEADER head, *header;

  /* initialise header */
  header = &head;
  /* Get the top of stack values for the page number of the parent
   and offset of the key */
  AM_topofStack(&pageNumber, &offset);
  AM_PopStack();

  /* Get the parent node */
  errVal = PF_GetThisPage(fileDesc, pageNumber, &pageBuf);
  AM_Check(errVal);

  /* copy the header from buffer */
  memcpy(header, pageBuf, AM_sint);

  /* check if there is room in this node for another key */
  if ((header->numKeys) < (header->maxKeys)) {
    /* add the attribute value to the node */
    AM_AddtoIntPage(pageBuf, value, pageNum, header, offset);

    /* copy the updated header into buffer*/
    memcpy(pageBuf, header, AM_sint);

    errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
    AM_Check(errVal);
    return (AME_OK);
  } else {
    /* not enough room for another key */
    errVal = PF_AllocPage(fileDesc, &pageNum1, &pageBuf1);
    AM_Check(errVal);

    /* split the internal node */
    AM_SplitIntNode(pageBuf, tempPage, pageBuf1, header, value, pageNum, offset);

    /* check if page being split is root */
    if (pageNumber == AM_RootPageNum) {
      /* allocate a new page for a new root */
      errVal = PF_AllocPage(fileDesc, &pageNum2, &pageBuf2);
      AM_Check(errVal);

      /* copy the first half into another buffer */
      memcpy(pageBuf2, tempPage, PF_PAGE_SIZE);

      /* fill the header of new root page and the
      attribute value */
      AM_FillRootPage(pageBuf, pageNum2, pageNum1, value, header->attrLength,
                      header->maxKeys);

      errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
      AM_Check(errVal);
      
      /* Unfix the new root page (pageNum2) */
      errVal = PF_UnfixPage(fileDesc, pageNum2, TRUE);
      AM_Check(errVal);

    } else {
      memcpy(pageBuf, tempPage, PF_PAGE_SIZE);

      errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
      AM_Check(errVal);
    }
    
    errVal = PF_UnfixPage(fileDesc, pageNum1, TRUE);
    AM_Check(errVal);
    
    if (pageNumber != AM_RootPageNum) {
      /* recursive call to add to the parent of this
      internal node*/
      errVal = AM_AddtoParent(fileDesc, pageNum1, value, attrLength);
      AM_Check(errVal);
    }
  }
  return (AME_OK);
}

/* adds a key to an internal node */
void AM_AddtoIntPage(char *pageBuf, char *value, int pageNum,
                     AM_INTHEADER *header, int offset) {
  int recSize;
  int i;

  recSize = header->attrLength + AM_si;

  /* shift all the keys greater than the one to be added to the right to
  make way for new key */
  for (i = header->numKeys; i > offset; i--)
    memcpy(pageBuf + AM_sint + i * recSize + AM_si,
           pageBuf + AM_sint + (i - 1) * recSize + AM_si, recSize);

  /* copy the attribute value into the appropriate place */
  memcpy(pageBuf + AM_sint + offset * recSize + AM_si, value,
         header->attrLength);

  /* copy the pagenumber of the child */
  memcpy(pageBuf + AM_sint + (offset + 1) * recSize, (char *)&pageNum, AM_si);

  /* one more key added*/
  header->numKeys++;
}

/* Fills the header and inserts a key into a new root */
void AM_FillRootPage(char *pageBuf, int pageNum1, int pageNum2, char *value,
                     short attrLength, short maxKeys) {
  AM_INTHEADER temphead, *tempheader;

  tempheader = &temphead;

  /* fill the header */
  tempheader->pageType = 'i';
  tempheader->attrLength = attrLength;
  tempheader->maxKeys = maxKeys;
  tempheader->numKeys = 1;
  memcpy(pageBuf + AM_sint, (char *)&pageNum1, AM_si);
  memcpy(pageBuf + AM_sint + AM_si, value, attrLength);
  memcpy(pageBuf + AM_sint + AM_si + attrLength, (char *)&pageNum2, AM_si);
  memcpy(pageBuf, tempheader, AM_sint);
}

/* Split an internal node */
void AM_SplitIntNode(char *pageBuf, char *pbuf1, char *pbuf2,
                     AM_INTHEADER *header, char *value, int pageNum, int offset) {
  AM_INTHEADER temphead, *tempheader;
  int recSize;
  char tempPage
      [PF_PAGE_SIZE +
       AM_MAXATTRLENGTH]; /* temp page for manipulating pageBuf */
  int length1, length2, length3; 

  tempheader = &temphead;
  recSize = header->attrLength + AM_si;

  tempheader->pageType = header->pageType;
  tempheader->attrLength = header->attrLength;
  tempheader->maxKeys = header->maxKeys;

  length1 = AM_si + (offset * recSize);
  /* copy the keys to the left of the key to be added */
  memcpy(tempPage, pageBuf + AM_sint, length1);

  /* copy the key itself */
  memcpy(tempPage + length1, value, header->attrLength);

  /* copy the pagenumber of the child node */
  memcpy(tempPage + length1 + header->attrLength, (char *)&pageNum, AM_si);

  length2 = (header->maxKeys - offset) * recSize;

  /* copy the rest of the keys */
  memcpy(tempPage + length1 + header->attrLength + AM_si,
         pageBuf + AM_sint + length1, length2);

  /* number of keys in each half */
  length1 = (header->maxKeys) / 2;

  length2 = AM_si + length1 * recSize;
  /* copy the first half into pbuf1 */
  memcpy(pbuf1 + AM_sint, tempPage, length2);
  tempheader->numKeys = length1; // Set numKeys for first half

  /* copy header into pbuf1 */
  memcpy(pbuf1, tempheader, AM_sint);

  /* copy the middle key into value to be passed back to parent */
  memcpy(value, tempPage + AM_si + length1 * recSize, header->attrLength);

  /* Calculate the correct byte size for the second half */
  length3 = (header->maxKeys - length1) * recSize + AM_si;

  /* copy the second half into pbuf2*/
  memcpy(pbuf2 + AM_sint,
         tempPage + AM_si + length1 * recSize + header->attrLength, length3);
         
  /* Set the correct number of keys for the second half */
  tempheader->numKeys = header->maxKeys - length1; 
  memcpy(pbuf2, tempheader, AM_sint);
}