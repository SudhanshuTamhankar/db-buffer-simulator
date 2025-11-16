/* testrm.c: Test for the Record Management (RM) Layer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../pflayer/pf.h" // Include PF header
#include "rm.h"             // Include RM header

#define TEST_FILE "testfile.db"
#define NUM_RECORDS 50
#define MAX_RECORD_LEN 100

// Function to print a record's data (first 20 bytes)
void print_record(char *data, int len) {
    printf(" (len %d) '%.*s...'", len, len > 20 ? 20 : len, data);
}

int main() {
    RM_FileHandle fh;
    RM_ScanHandle sh;
    RID rids[NUM_RECORDS]; // Array to store RIDs
    char insert_buf[MAX_RECORD_LEN];
    char get_buf[MAX_RECORD_LEN];
    int i;
    int err;

    printf("Initializing RM Layer...\n");
    RM_Init();

    printf("Creating file '%s'...\n", TEST_FILE);
    err = RM_CreateFile(TEST_FILE);
    if (err != PFE_OK) {
        PF_PrintError("RM_CreateFile");
        exit(1);
    }

    printf("Opening file...\n");
    err = RM_OpenFile(TEST_FILE, &fh);
    if (err != PFE_OK) {
        PF_PrintError("RM_OpenFile");
        exit(1);
    }

    // --- 1. INSERT RECORDS ---
    printf("\n--- Inserting %d variable-length records ---\n", NUM_RECORDS);
    srand(time(NULL));
    for (i = 0; i < NUM_RECORDS; i++) {
        // Create a variable-length string
        int len = (rand() % 50) + 10; // Length from 10 to 59
        sprintf(insert_buf, "Record %d", i);
        memset(insert_buf + strlen(insert_buf), 'x', len - strlen(insert_buf));
        insert_buf[len] = '\0';
        
        err = RM_InsertRec(&fh, insert_buf, len, &rids[i]);
        if (err != PFE_OK) {
            PF_PrintError("RM_InsertRec");
            exit(1);
        }
        printf("Inserted Record %d. RID: (Page %d, Slot %d)\n", 
               i, rids[i].pageNum, rids[i].slotNum);
    }

    // --- 2. DELETE RECORDS ---
    printf("\n--- Deleting every 3rd record ---\n");
    for (i = 0; i < NUM_RECORDS; i += 3) {
        printf("Deleting Record %d. RID: (Page %d, Slot %d)\n", 
               i, rids[i].pageNum, rids[i].slotNum);
        err = RM_DeleteRec(&fh, &rids[i]);
        if (err != PFE_OK) {
            PF_PrintError("RM_DeleteRec");
            exit(1);
        }
    }

    // --- 3. SCAN AND VERIFY ---
    printf("\n--- Scanning all records... ---\n");
    err = RM_ScanOpen(&fh, &sh);
    if (err != PFE_OK) {
        PF_PrintError("RM_ScanOpen");
        exit(1);
    }

    int records_found = 0;
    RID scan_rid;
    while ((err = RM_GetNextRec(&sh, get_buf, &scan_rid)) != RM_EOF) {
        if (err != PFE_OK) {
            PF_PrintError("RM_GetNextRec");
            exit(1);
        }

        // Verify RID
        printf("Found RID: (Page %d, Slot %d)", scan_rid.pageNum, scan_rid.slotNum);
        
        // Check if this record should have been deleted
        if (scan_rid.slotNum % 3 == 0) {
            printf("\n*** ERROR: Found record %d, which should be deleted! ***\n", scan_rid.slotNum);
        } else {
            printf(" - OK\n");
        }
        records_found++;
    }

    printf("Scan complete. Found %d records.\n", records_found);

    // Verify count (50 total - 17 deleted = 33)
    int expected_count = NUM_RECORDS - (NUM_RECORDS / 3 + (NUM_RECORDS % 3 > 0 ? 1 : 0));
    if (records_found == expected_count) {
        printf("Record count is correct! (%d)\n", expected_count);
    } else {
        printf("*** ERROR: Expected %d records, but found %d! ***\n", expected_count, records_found);
    }

    // --- 4. CHECK SPACE UTILIZATION (NEW) ---
    printf("\n--- Checking Space Utilization ---\n");
    int total_pages, total_record_bytes, total_wasted_bytes;
    err = RM_GetSpaceUtilization(&fh, &total_pages, &total_record_bytes, &total_wasted_bytes);
    if (err != PFE_OK) {
        PF_PrintError("RM_GetSpaceUtilization");
        exit(1);
    }
    
    int total_bytes = total_pages * PF_PAGE_SIZE;
    double utilization_percent = ((double)total_record_bytes / (double)total_bytes) * 100.0;
    
    printf("Total Pages: %d\n", total_pages);
    printf("Total Bytes: %d\n", total_bytes);
    printf("Bytes Used by Records: %d\n", total_record_bytes);
    printf("Bytes Wasted (header, slots, free, holes): %d\n", total_wasted_bytes);
    printf("Space Utilization (Record Data / Total Bytes): %.2f%%\n", utilization_percent);
    
    // --- 5. CLEANUP ---
    printf("\nClosing scan...\n");
    err = RM_ScanClose(&sh);
    if (err != PFE_OK) {
        PF_PrintError("RM_ScanClose");
        exit(1);
    }

    // --- 4. CLEANUP ---
    printf("\nClosing file...\n");
    err = RM_CloseFile(&fh);
    if (err != PFE_OK) {
        PF_PrintError("RM_CloseFile");
        exit(1);
    }

    printf("Destroying file...\n");
    err = RM_DestroyFile(TEST_FILE);
    if (err != PFE_OK) {
        PF_PrintError("RM_DestroyFile");
        exit(1);
    }

    printf("\n*** RM Layer Test Passed! ***\n");
    return 0;
}