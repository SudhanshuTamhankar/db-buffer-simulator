/* test_objective3.c: Integrates PF, RM, and AM layers */
#include "am.h" // Includes rm.h and pf.h
#include "testam.h" // Includes test definitions

#include <time.h>   // For clock()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DATA_FILE "student_records.db"
#define INDEX_FILE "student_records" // AM_CreateIndex will add ".0"
#define INDEX_NO 0
#define ATTR_TYPE 'i'
#define ATTR_LEN sizeof(int)

#define NUM_RECORDS 200 // Reduced for quicker debug runs

/* Struct to hold the stats for one run */
typedef struct {
    double cpu_time;
    long logical_reads;
    long physical_reads;
    long physical_writes;
} MethodStats;

/*
 * =================================================================
 * RID Packing/Unpacking Functions
 * =================================================================
 */

// Packs {pageNum, slotNum} into a single int
int pack_rid(RID rid) {
    return (rid.pageNum << 16) | (rid.slotNum & 0xFFFF);
}

// Unpacks a single int back into an RID struct
RID unpack_rid(int packed_rid) {
    RID rid;
    rid.pageNum = (packed_rid >> 16);
    rid.slotNum = (packed_rid & 0xFFFF);
    return rid;
}

/*
 * =================================================================
 * Method 1: Build index from an existing, (pre-sorted) file
 * This is our "efficient bulk-loading technique"
 * =================================================================
 */
void method1_BuildFromExisting(MethodStats *stats) {
    RM_FileHandle rm_fh;
    RM_ScanHandle rm_sh;
    int am_fd;
    RID rid;
    int i, err, key;
    char record_data[30];
    clock_t start, end;

    printf("\n--- Method 1: Building index from (pre-sorted) file ---\n");
    
    // 1. Reset stats by re-initializing the PF layer
    PF_ResetStats(); // Use the new reset function

    // 2. Create and populate the data file (RM)
    err = RM_CreateFile(DATA_FILE);
    if (err != AME_OK) { PF_PrintError("RM_CreateFile"); exit(1); }
    err = RM_OpenFile(DATA_FILE, &rm_fh);
    if (err != AME_OK) { PF_PrintError("RM_OpenFile"); exit(1); }

    printf("Populating data file with %d sorted records...\n", NUM_RECORDS);
    for (i = 0; i < NUM_RECORDS; i++) {
        key = i; // Insert keys in sorted order
        sprintf(record_data, "Student_Name_%d", key);
        int record_len = strlen(record_data) + 1;
        err = RM_InsertRec(&rm_fh, record_data, record_len, &rid);
        if (err != AME_OK) { PF_PrintError("RM_InsertRec"); exit(1); }
    }
    err = RM_CloseFile(&rm_fh);
    if (err != AME_OK) { PF_PrintError("RM_CloseFile"); exit(1); }


    // 3. --- START TIMING ---
    start = clock();

    // 4. Create and open the new index file (AM)
    err = AM_CreateIndex(INDEX_FILE, INDEX_NO, ATTR_TYPE, ATTR_LEN);
    if (err != AME_OK) { AM_PrintError("AM_CreateIndex"); exit(1); }
    
    char index_fname[AM_MAX_FNAME_LENGTH];
    sprintf(index_fname, "%s.%d", INDEX_FILE, INDEX_NO);
    am_fd = PF_OpenFile(index_fname);
    if (am_fd < 0) { PF_PrintError("PF_OpenFile (index)"); exit(1); }

    // 5. Re-open the data file and scan it
    printf("Scanning data file and building index...\n");
    err = RM_OpenFile(DATA_FILE, &rm_fh);
    if (err != AME_OK) { PF_PrintError("RM_OpenFile"); exit(1); }
    err = RM_ScanOpen(&rm_fh, &rm_sh);
    if (err != AME_OK) { PF_PrintError("RM_ScanOpen"); exit(1); }

    // 6. For each record, add an entry to the index
    int count = 0;
    while((err = RM_GetNextRec(&rm_sh, record_data, &rid)) != RM_EOF) {
        sscanf(record_data, "Student_Name_%d", &key);
        int packed_rid = pack_rid(rid);
        // Debug: show progress for early failures
        if (count < 5) {
            printf("Inserting key=%d (page=%d, slot=%d)\n", key, rid.pageNum, rid.slotNum);
        }
        err = AM_InsertEntry(am_fd, ATTR_TYPE, ATTR_LEN, (char *)&key, packed_rid);
        if (err != AME_OK) { AM_PrintError("AM_InsertEntry"); exit(1); }
        count++;
    }
    
    // 7. --- STOP TIMING ---
    end = clock();
    stats->cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Index build complete. %d entries added.\n", count);

    // 8. Capture stats using the new getter function
    PF_GetStats(&stats->logical_reads, &stats->physical_reads, &stats->physical_writes);

    // 9. Close all files
    RM_ScanClose(&rm_sh);
    RM_CloseFile(&rm_fh);
    PF_CloseFile(am_fd);
    
    // 10. Cleanup
    RM_DestroyFile(DATA_FILE);
    AM_DestroyIndex(INDEX_FILE, INDEX_NO);
}


/*
 * =================================================================
 * Method 2: Build index by inserting records one by one (randomly)
 * =================================================================
 */
void method2_InsertOneByOne(MethodStats *stats) {
    RM_FileHandle rm_fh;
    int am_fd;
    RID rid;
    int i;
    int err;
    clock_t start, end;

    printf("\n--- Method 2: Inserting %d records one by one (randomly) ---\n", NUM_RECORDS);

    // 1. Reset stats by re-initializing the PF layer
    PF_ResetStats(); // Use the new reset function

    // 2. Create and open the data file (RM)
    err = RM_CreateFile(DATA_FILE);
    if (err != AME_OK) { PF_PrintError("RM_CreateFile"); exit(1); }
    err = RM_OpenFile(DATA_FILE, &rm_fh);
    if (err != AME_OK) { PF_PrintError("RM_OpenFile"); exit(1); }

    // 3. Create and open the index file (AM)
    err = AM_CreateIndex(INDEX_FILE, INDEX_NO, ATTR_TYPE, ATTR_LEN);
    if (err != AME_OK) { AM_PrintError("AM_CreateIndex"); exit(1); }
    
    char index_fname[AM_MAX_FNAME_LENGTH];
    sprintf(index_fname, "%s.%d", INDEX_FILE, INDEX_NO);
    am_fd = PF_OpenFile(index_fname);
    if (am_fd < 0) { PF_PrintError("PF_OpenFile (index)"); exit(1); }

    // 4. --- START TIMING ---
    start = clock();
    
    printf("Inserting %d records into data file and index...\n", NUM_RECORDS);
    srand(time(NULL));
    for (i = 0; i < NUM_RECORDS; i++) {
        int key = rand() % (NUM_RECORDS * 5); // Random Student ID
        char record_data[30];
        sprintf(record_data, "Student_Name_%d", key);
        int record_len = strlen(record_data) + 1;

        // 4a. Insert into data file
        err = RM_InsertRec(&rm_fh, record_data, record_len, &rid);
        if (err != AME_OK) { PF_PrintError("RM_InsertRec"); exit(1); }

        // 4b. Pack the RID
        int packed_rid = pack_rid(rid);

        // 4c. Insert (key, packed_RID) into index file
        err = AM_InsertEntry(am_fd, ATTR_TYPE, ATTR_LEN, (char *)&key, packed_rid);
        if (err != AME_OK) { AM_PrintError("AM_InsertEntry"); exit(1); }
    }
    
    // 5. --- STOP TIMING ---
    end = clock();
    stats->cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Successfully inserted %d records.\n", NUM_RECORDS);

    // 6. Capture stats using the new getter function
    PF_GetStats(&stats->logical_reads, &stats->physical_reads, &stats->physical_writes);

    // 7. Close files
    err = RM_CloseFile(&rm_fh);
    if (err != AME_OK) { PF_PrintError("RM_CloseFile"); exit(1); }
    err = PF_CloseFile(am_fd);
    if (err != AME_OK) { PF_PrintError("PF_CloseFile"); exit(1); }
    
    // 8. Cleanup
    RM_DestroyFile(DATA_FILE);
    AM_DestroyIndex(INDEX_FILE, INDEX_NO);
}


/*
 * =================================================================
 * Main Function
 * =================================================================
 */
int main(void) {
    
    MethodStats stats1, stats2;
    
    // Initialize all layers
    RM_Init(); // This calls PF_Init() internally, which resets stats

    // Best-effort cleanup from previous runs
    // Ignore errors if files do not exist yet
    RM_DestroyFile(DATA_FILE);
    AM_DestroyIndex(INDEX_FILE, INDEX_NO);
    
    // --- Run Method 1 (Build from Existing) ---
    method1_BuildFromExisting(&stats1);
    
    // --- Run Method 2 (One by One) ---
    // RM_Init() or PF_ResetStats() must be called to reset stats
    method2_InsertOneByOne(&stats2);

    // --- Print Final Comparison Table ---
    printf("\n\n--- FINAL COMPARISON (Building Index with %d Records) ---\n", NUM_RECORDS);
    printf("--------------------------------------------------------------------------\n");
    printf("| Method                                | Time (sec) | Physical Reads | Physical Writes | Logical Reads |\n");
    printf("|---------------------------------------|------------|----------------|-----------------|---------------|\n");
    
    printf("| 1: Scan Sorted File (Simple Bulk Load) | %-10.4f | %-14ld | %-15ld | %-13ld |\n", 
           stats1.cpu_time, stats1.physical_reads, stats1.physical_writes, stats1.logical_reads);
    
    printf("| 2: Insert One-by-One (Random)         | %-10.4f | %-14ld | %-15ld | %-13ld |\n", 
           stats2.cpu_time, stats2.physical_reads, stats2.physical_writes, stats2.logical_reads);
    
    printf("--------------------------------------------------------------------------\n");
    printf("\n*** Objective 3 Comparison Complete! ***\n");
    
    return 0;
}