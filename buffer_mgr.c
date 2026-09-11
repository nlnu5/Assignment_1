/*******************************************************************************
 * Advanced Database Organization - Assignment 01
 * Buffer Manager Implementation
 *
 * Student Name: Nishit Raj Lnu
 * Student ID:   A20652913
 *
 * INSTRUCTIONS:
 * - Implement all functions declared in buffer_mgr.h
 * - You MUST implement FIFO and LRU replacement strategies
 * - Use the Storage Manager to read/write pages to disk
 * - Do not modify the function signatures
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "dberror.h"

/******************************************************************************
 * DATA STRUCTURES
 *
 * Define your internal data structures here.
 * Suggested: Frame structure to track each buffer frame.
 ******************************************************************************/

typedef struct Frame {
    PageNumber pageNum;     /* Page number stored in this frame */
    char *data;             /* Page data buffer */
    bool dirty;             /* Has the page been modified? */
    int fixCount;           /* Number of clients using this page */

    /* Used for replacement strategies. */
    unsigned long long lru;
    unsigned long long fifo;
} Frame;

typedef struct BM_MgmtData {
    Frame *frames;          /* Array of frames */
    SM_FileHandle fh;       /* File handle for the page file */
    int numReadIO;          /* Count of read operations */
    int numWriteIO;         /* Count of write operations */

    /* Global timestamp used for FIFO/LRU bookkeeping. */
    unsigned long long time;
} BM_MgmtData;

static BM_MgmtData *getMgmt(BM_BufferPool *bm) {
    return (BM_MgmtData *) bm->mgmtData;
}

static int findPage(BM_MgmtData *mgmt,
                    int numPages,
                    PageNumber pageNum) {
    int i;

    for (i = 0; i < numPages; i++) {
        if (mgmt->frames[i].pageNum == pageNum)
            return i;
    }

    return -1;
}

static int findEmpty(BM_MgmtData *mgmt,
                     int numPages) {
    int i;

    for (i = 0; i < numPages; i++) {
        if (mgmt->frames[i].pageNum == NO_PAGE)
            return i;
    }

    return -1;
}

static int findVictim(BM_BufferPool *bm,
                      BM_MgmtData *mgmt) {
    int victim = -1;
    unsigned long long oldest = ULLONG_MAX;
    int i;

    for (i = 0; i < bm->numPages; i++) {
        Frame *frame = &mgmt->frames[i];
        unsigned long long value;

        /*
         * Empty frames are handled separately.
         */
        if (frame->pageNum == NO_PAGE)
            continue;

        /*
         * A pinned page cannot be selected as a victim.
         */
        if (frame->fixCount != 0)
            continue;

        if (bm->strategy == RS_FIFO)
            value = frame->fifo;
        else
            value = frame->lru;

        if (victim == -1 || value < oldest) {
            victim = i;
            oldest = value;
        }
    }

    return victim;
}

static RC writeFrame(BM_MgmtData *mgmt,
                     Frame *frame) {
    RC rc;

    if (frame->pageNum == NO_PAGE)
        return RC_OK;

    if (!frame->dirty)
        return RC_OK;

    rc = writeBlock(frame->pageNum,
                    &mgmt->fh,
                    frame->data);

    if (rc != RC_OK)
        return rc;

    frame->dirty = false;
    mgmt->numWriteIO++;

    return RC_OK;
}

/******************************************************************************
 * BUFFER POOL OPERATIONS
 ******************************************************************************/

/**
 * Initialize a buffer pool.
 *
 * @param bm Pointer to buffer pool structure to initialize
 * @param pageFileName Name of the page file to manage
 * @param numPages Number of frames in the buffer pool
 * @param strategy Replacement strategy (RS_FIFO or RS_LRU)
 * @param stratData Strategy-specific data (can be NULL)
 * @return RC_OK on success, error code on failure
 */
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData) {
    BM_MgmtData *mgmt;
    RC rc;
    int i;

    /* stratData is not required for FIFO or LRU. */
    (void) stratData;

    if (bm == NULL || pageFileName == NULL || numPages <= 0)
        THROW(RC_BM_POOL_NOT_INIT, "Invalid buffer pool");

    if (strategy != RS_FIFO && strategy != RS_LRU)
        THROW(RC_BM_INVALID_STRATEGY, "Invalid replacement strategy");

    mgmt = calloc(1, sizeof(BM_MgmtData));

    if (mgmt == NULL)
        THROW(RC_WRITE_FAILED, "Memory allocation failed");

    mgmt->frames = calloc(numPages, sizeof(Frame));

    if (mgmt->frames == NULL) {
        free(mgmt);
        THROW(RC_WRITE_FAILED, "Memory allocation failed");
    }

    /*
     * Initialize every frame.
     */
    for (i = 0; i < numPages; i++) {
        mgmt->frames[i].pageNum = NO_PAGE;
        mgmt->frames[i].dirty = false;
        mgmt->frames[i].fixCount = 0;
        mgmt->frames[i].lru = 0;
        mgmt->frames[i].fifo = 0;

        mgmt->frames[i].data =
            calloc(PAGE_SIZE, sizeof(char));

        if (mgmt->frames[i].data == NULL) {
            int j;

            for (j = 0; j < i; j++)
                free(mgmt->frames[j].data);

            free(mgmt->frames);
            free(mgmt);

            THROW(RC_WRITE_FAILED, "Memory allocation failed");
        }
    }

    /*
     * Open the page file using the Storage Manager.
     */
    rc = openPageFile((char *) pageFileName, &mgmt->fh);

    if (rc != RC_OK) {
        for (i = 0; i < numPages; i++)
            free(mgmt->frames[i].data);

        free(mgmt->frames);
        free(mgmt);

        return rc;
    }

    bm->pageFile = malloc(strlen(pageFileName) + 1);

    if (bm->pageFile == NULL) {
        closePageFile(&mgmt->fh);

        for (i = 0; i < numPages; i++)
            free(mgmt->frames[i].data);

        free(mgmt->frames);
        free(mgmt);

        THROW(RC_WRITE_FAILED, "Memory allocation failed");
    }

    strcpy(bm->pageFile, pageFileName);

    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = mgmt;

    return RC_OK;
}

/**
 * Shutdown a buffer pool.
 * Write all dirty pages to disk and free resources.
 *
 * @param bm Pointer to buffer pool
 * @return RC_OK on success, RC_BM_PAGE_PINNED if pages still pinned
 */
RC shutdownBufferPool(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;
    RC rc;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    mgmt = getMgmt(bm);

    /*
     * The buffer pool cannot be shut down while a page is pinned.
     */
    for (i = 0; i < bm->numPages; i++) {
        if (mgmt->frames[i].fixCount > 0)
            THROW(RC_BM_PAGE_PINNED,
                  "Page is pinned");
    }

    /*
     * Write all dirty, unpinned pages.
     */
    rc = forceFlushPool(bm);

    if (rc != RC_OK)
        return rc;

    rc = closePageFile(&mgmt->fh);

    if (rc != RC_OK)
        return rc;

    /*
     * Free all frame data.
     */
    for (i = 0; i < bm->numPages; i++)
        free(mgmt->frames[i].data);

    free(mgmt->frames);
    free(mgmt);

    free(bm->pageFile);

    bm->pageFile = NULL;
    bm->mgmtData = NULL;
    bm->numPages = 0;

    return RC_OK;
}

/**
 * Write all dirty pages with fixCount 0 to disk.
 *
 * @param bm Pointer to buffer pool
 * @return RC_OK on success
 */
RC forceFlushPool(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;
    RC rc;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    mgmt = getMgmt(bm);

    for (i = 0; i < bm->numPages; i++) {
        Frame *frame = &mgmt->frames[i];

        /*
         * Pinned pages are not flushed by forceFlushPool.
         */
        if (frame->dirty && frame->fixCount == 0) {
            rc = writeFrame(mgmt, frame);

            if (rc != RC_OK)
                return rc;
        }
    }

    return RC_OK;
}

/******************************************************************************
 * PAGE ACCESS OPERATIONS
 ******************************************************************************/

/**
 * Mark a page as dirty.
 *
 * @param bm Pointer to buffer pool
 * @param page Page handle identifying the page
 * @return RC_OK on success
 */
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_MgmtData *mgmt;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    if (page == NULL)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Invalid page");

    mgmt = getMgmt(bm);

    i = findPage(mgmt,
                 bm->numPages,
                 page->pageNum);

    if (i == -1)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Page not found");

    mgmt->frames[i].dirty = true;

    return RC_OK;
}

/**
 * Unpin a page (decrease fix count).
 *
 * @param bm Pointer to buffer pool
 * @param page Page handle identifying the page
 * @return RC_OK on success
 */
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_MgmtData *mgmt;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    if (page == NULL)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Invalid page");

    mgmt = getMgmt(bm);

    i = findPage(mgmt,
                 bm->numPages,
                 page->pageNum);

    if (i == -1)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Page not found");

    /*
     * Do not allow the fix count to become negative.
     */
    if (mgmt->frames[i].fixCount > 0)
        mgmt->frames[i].fixCount--;

    return RC_OK;
}

/**
 * Force a specific page to disk.
 *
 * @param bm Pointer to buffer pool
 * @param page Page handle identifying the page
 * @return RC_OK on success
 */
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    BM_MgmtData *mgmt;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    if (page == NULL)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Invalid page");

    mgmt = getMgmt(bm);

    i = findPage(mgmt,
                 bm->numPages,
                 page->pageNum);

    if (i == -1)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Page not found");

    return writeFrame(mgmt,
                      &mgmt->frames[i]);
}

/**
 * Pin a page into the buffer pool.
 * If the page is already in the pool, increase its fix count.
 * Otherwise, load it from disk (potentially evicting another page).
 *
 * @param bm Pointer to buffer pool
 * @param page Page handle to populate
 * @param pageNum Page number to pin
 * @return RC_OK on success, RC_BM_NO_FREE_FRAME if all pages are pinned
 */
RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {
    BM_MgmtData *mgmt;
    int i;
    RC rc;

    if (bm == NULL || bm->mgmtData == NULL)
        THROW(RC_BM_POOL_NOT_INIT,
              "Buffer pool not initialized");

    if (page == NULL)
        THROW(RC_BM_PAGE_NOT_FOUND,
              "Invalid page handle");

    mgmt = getMgmt(bm);

    /*
     * The requested page must already exist in the page file.
     */
    if (pageNum < 0 ||
        pageNum >= mgmt->fh.totalNumPages)
        THROW(RC_READ_NON_EXISTING_PAGE,
              "Page does not exist");

    /*
     * First check whether the page is already in the buffer pool.
     */
    i = findPage(mgmt,
                 bm->numPages,
                 pageNum);

    if (i != -1) {
        /*
         * Page is already resident.
         */
        mgmt->frames[i].fixCount++;

        /*
         * IMPORTANT:
         * LRU timestamp must be updated on EVERY pin,
         * including a re-pin of a page already in memory.
         */
        mgmt->time++;
        mgmt->frames[i].lru = mgmt->time;

        page->pageNum = pageNum;
        page->data = mgmt->frames[i].data;

        return RC_OK;
    }

    /*
     * Look for an unused frame first.
     */
    i = findEmpty(mgmt, bm->numPages);

    /*
     * If there is no empty frame, select a replacement victim.
     */
    if (i == -1)
        i = findVictim(bm, mgmt);

    /*
     * Every frame is occupied by a pinned page.
     */
    if (i == -1)
        THROW(RC_BM_NO_FREE_FRAME,
              "No free frame");

    /*
     * If the victim is dirty, write it back before replacing it.
     */
    rc = writeFrame(mgmt,
                    &mgmt->frames[i]);

    if (rc != RC_OK)
        return rc;

    /*
     * Read the requested page from disk into the frame.
     */
    rc = readBlock(pageNum,
                   &mgmt->fh,
                   mgmt->frames[i].data);

    if (rc != RC_OK)
        return rc;

    mgmt->numReadIO++;

    /*
     * Advance the global timestamp for this access.
     */
    mgmt->time++;

    /*
     * Update frame metadata.
     */
    mgmt->frames[i].pageNum = pageNum;
    mgmt->frames[i].dirty = false;
    mgmt->frames[i].fixCount = 1;

    /*
     * LRU records the most recent access.
     * FIFO records when the page entered the buffer.
     */
    mgmt->frames[i].lru = mgmt->time;
    mgmt->frames[i].fifo = mgmt->time;

    /*
     * Populate the caller's page handle.
     */
    page->pageNum = pageNum;
    page->data = mgmt->frames[i].data;

    return RC_OK;
}

/******************************************************************************
 * STATISTICS FUNCTIONS
 ******************************************************************************/

/**
 * Get array of page numbers stored in each frame.
 * Returns NO_PAGE (-1) for empty frames.
 *
 * @param bm Pointer to buffer pool
 * @return Array of PageNumbers (caller should NOT free this)
 */
PageNumber *getFrameContents(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;
    PageNumber *result;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        return NULL;

    mgmt = getMgmt(bm);

    /*
     * Allocate a snapshot of the current frame contents.
     * The official test program frees this returned array.
     */
    result = malloc(bm->numPages * sizeof(PageNumber));

    if (result == NULL)
        return NULL;

    for (i = 0; i < bm->numPages; i++)
        result[i] = mgmt->frames[i].pageNum;

    return result;
}

/**
 * Get array of dirty flags for each frame.
 *
 * @param bm Pointer to buffer pool
 * @return Array of bools (caller should NOT free this)
 */
bool *getDirtyFlags(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;
    bool *result;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        return NULL;

    mgmt = getMgmt(bm);

    /*
     * Allocate a snapshot of the current dirty flags.
     * The official test program frees this returned array.
     */
    result = malloc(bm->numPages * sizeof(bool));

    if (result == NULL)
        return NULL;

    for (i = 0; i < bm->numPages; i++)
        result[i] = mgmt->frames[i].dirty;

    return result;
}

/**
 * Get array of fix counts for each frame.
 *
 * @param bm Pointer to buffer pool
 * @return Array of ints for each frame
 */
int *getFixCounts(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;
    int *result;
    int i;

    if (bm == NULL || bm->mgmtData == NULL)
        return NULL;

    mgmt = getMgmt(bm);

    /*
     * Allocate a snapshot of the current fix counts.
     * The official test program frees this returned array.
     */
    result = malloc(bm->numPages * sizeof(int));

    if (result == NULL)
        return NULL;

    for (i = 0; i < bm->numPages; i++)
        result[i] = mgmt->frames[i].fixCount;

    return result;
}

/**
 * Get total number of read I/O operations since pool initialization.
 *
 * @param bm Pointer to buffer pool
 * @return Number of disk reads
 */
int getNumReadIO(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;

    if (bm == NULL || bm->mgmtData == NULL)
        return 0;

    mgmt = getMgmt(bm);

    return mgmt->numReadIO;
}

/**
 * Get total number of write I/O operations since pool initialization.
 *
 * @param bm Pointer to buffer pool
 * @return Number of disk writes
 */
int getNumWriteIO(BM_BufferPool *const bm) {
    BM_MgmtData *mgmt;

    if (bm == NULL || bm->mgmtData == NULL)
        return 0;

    mgmt = getMgmt(bm);

    return mgmt->numWriteIO;
}