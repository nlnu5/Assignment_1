/*******************************************************************************
 * Advanced Database Organization - Assignment 01
 * Storage Manager Implementation
 *
 * Student Name: Nishit Raj Lnu
 * Student ID:   A20652913
 *
 * INSTRUCTIONS:
 * - Implement all functions declared in storage_mgr.h
 * - Do not modify the function signatures
 * - Use the provided dberror.h return codes
 * - Test your implementation using the provided test files
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "storage_mgr.h"
#include "dberror.h"

/******************************************************************************
 * INITIALIZATION
 ******************************************************************************/

/**
 * Initialize the storage manager.
 * Called once when the system starts.
 */
void initStorageManager(void) {
    /* No global initialization is required. */
}

/******************************************************************************
 * FILE OPERATIONS
 ******************************************************************************/

/**
 * Create a new page file with a single empty page.
 * The file should be PAGE_SIZE bytes (4096 bytes).
 * All bytes in the initial page should be zero.
 *
 * @param fileName Name of the file to create
 * @return RC_OK on success, error code on failure
 */
RC createPageFile(char *fileName) {
    FILE *fp;
    char *page;

    if (fileName == NULL)
        THROW(RC_WRITE_FAILED, "Invalid file name");

    /* Do not overwrite an existing page file. */
    fp = fopen(fileName, "rb");

    if (fp != NULL) {
        fclose(fp);
        THROW(RC_FILE_ALREADY_EXISTS, "File already exists");
    }

    fp = fopen(fileName, "wb");

    if (fp == NULL)
        THROW(RC_WRITE_FAILED, "Unable to create file");

    /* calloc initializes the page to all zeros. */
    page = calloc(PAGE_SIZE, sizeof(char));

    if (page == NULL) {
        fclose(fp);
        THROW(RC_WRITE_FAILED, "Memory allocation failed");
    }

    if (fwrite(page, 1, PAGE_SIZE, fp) != PAGE_SIZE) {
        free(page);
        fclose(fp);
        THROW(RC_WRITE_FAILED, "Unable to write initial page");
    }

    free(page);

    if (fclose(fp) != 0)
        THROW(RC_WRITE_FAILED, "Unable to close file");

    return RC_OK;
}

/**
 * Open an existing page file.
 * Populate the file handle with file information.
 *
 * @param fileName Name of the file to open
 * @param fHandle Pointer to file handle to populate
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist
 */
RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    FILE *fp;
    long fileSize;

    if (fileName == NULL || fHandle == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "Invalid file handle");

    fp = fopen(fileName, "r+b");

    if (fp == NULL)
        THROW(RC_FILE_NOT_FOUND, "File not found");

    /* Determine the number of pages from the file size. */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        THROW(RC_FILE_NOT_FOUND, "Unable to determine file size");
    }

    fileSize = ftell(fp);

    if (fileSize < 0 || fileSize % PAGE_SIZE != 0) {
        fclose(fp);
        THROW(RC_FILE_NOT_FOUND, "Invalid page file");
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        THROW(RC_FILE_NOT_FOUND, "Unable to reset file position");
    }

    fHandle->fileName = malloc(strlen(fileName) + 1);

    if (fHandle->fileName == NULL) {
        fclose(fp);
        THROW(RC_WRITE_FAILED, "Memory allocation failed");
    }

    strcpy(fHandle->fileName, fileName);

    fHandle->totalNumPages = (int)(fileSize / PAGE_SIZE);
    fHandle->curPagePos = 0;
    fHandle->mgmtInfo = fp;

    return RC_OK;
}

/**
 * Close an open page file.
 *
 * @param fHandle Pointer to file handle
 * @return RC_OK on success, error code on failure
 */
RC closePageFile(SM_FileHandle *fHandle) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (fclose((FILE *)fHandle->mgmtInfo) != 0)
        THROW(RC_WRITE_FAILED, "Unable to close file");

    free(fHandle->fileName);

    fHandle->fileName = NULL;
    fHandle->mgmtInfo = NULL;
    fHandle->totalNumPages = 0;
    fHandle->curPagePos = 0;

    return RC_OK;
}

/**
 * Delete a page file from disk.
 *
 * @param fileName Name of the file to delete
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist
 */
RC destroyPageFile(char *fileName) {
    if (fileName == NULL)
        THROW(RC_FILE_NOT_FOUND, "Invalid file name");

    if (remove(fileName) != 0)
        THROW(RC_FILE_NOT_FOUND, "Unable to delete file");

    return RC_OK;
}

/******************************************************************************
 * READ OPERATIONS
 ******************************************************************************/

/**
 * Read a specific page from the file into memory.
 *
 * @param pageNum The page number to read (0-indexed)
 * @param fHandle Pointer to file handle
 * @param memPage Buffer to store the page data (must be PAGE_SIZE bytes)
 * @return RC_OK on success, RC_READ_NON_EXISTING_PAGE if page doesn't exist
 */
RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    FILE *fp;

    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (memPage == NULL)
        THROW(RC_READ_NON_EXISTING_PAGE, "Invalid memory page");

    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        THROW(RC_READ_NON_EXISTING_PAGE, "Page does not exist");

    fp = (FILE *)fHandle->mgmtInfo;

    if (fseek(fp, (long)pageNum * PAGE_SIZE, SEEK_SET) != 0)
        THROW(RC_READ_NON_EXISTING_PAGE, "Unable to seek to page");

    if (fread(memPage, 1, PAGE_SIZE, fp) != PAGE_SIZE)
        THROW(RC_READ_NON_EXISTING_PAGE, "Unable to read page");

    fHandle->curPagePos = pageNum;

    return RC_OK;
}

/**
 * Get the current page position.
 *
 * @param fHandle Pointer to file handle
 * @return Current page position
 */
int getBlockPos(SM_FileHandle *fHandle) {
    if (fHandle == NULL)
        return -1;

    return fHandle->curPagePos;
}

/**
 * Read the first page (page 0).
 */
RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(0, fHandle, memPage);
}

/**
 * Read the previous page relative to current position.
 */
RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (fHandle->curPagePos <= 0)
        THROW(RC_READ_NON_EXISTING_PAGE, "No previous page");

    return readBlock(fHandle->curPagePos - 1, fHandle, memPage);
}

/**
 * Read the current page.
 */
RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

/**
 * Read the next page relative to current position.
 */
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (fHandle->curPagePos >= fHandle->totalNumPages - 1)
        THROW(RC_READ_NON_EXISTING_PAGE, "No next page");

    return readBlock(fHandle->curPagePos + 1, fHandle, memPage);
}

/**
 * Read the last page.
 */
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (fHandle->totalNumPages <= 0)
        THROW(RC_READ_NON_EXISTING_PAGE, "No pages");

    return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

/******************************************************************************
 * WRITE OPERATIONS
 ******************************************************************************/

/**
 * Write a page to the file.
 *
 * @param pageNum The page number to write (0-indexed)
 * @param fHandle Pointer to file handle
 * @param memPage Buffer containing the page data
 * @return RC_OK on success, RC_WRITE_FAILED on failure
 */
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    FILE *fp;

    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (memPage == NULL)
        THROW(RC_WRITE_FAILED, "Invalid memory page");

    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        THROW(RC_INVALID_PAGE_NUM, "Invalid page number");

    fp = (FILE *)fHandle->mgmtInfo;

    if (fseek(fp, (long)pageNum * PAGE_SIZE, SEEK_SET) != 0)
        THROW(RC_WRITE_FAILED, "Unable to seek to page");

    if (fwrite(memPage, 1, PAGE_SIZE, fp) != PAGE_SIZE)
        THROW(RC_WRITE_FAILED, "Unable to write page");

    if (fflush(fp) != 0)
        THROW(RC_WRITE_FAILED, "Unable to flush page");

    fHandle->curPagePos = pageNum;

    return RC_OK;
}

/**
 * Write to the current page position.
 */
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

/**
 * Append an empty page to the end of the file.
 *
 * @param fHandle Pointer to file handle
 * @return RC_OK on success, error code on failure
 */
RC appendEmptyBlock(SM_FileHandle *fHandle) {
    FILE *fp;
    char *page;

    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    page = calloc(PAGE_SIZE, sizeof(char));

    if (page == NULL)
        THROW(RC_WRITE_FAILED, "Memory allocation failed");

    fp = (FILE *)fHandle->mgmtInfo;

    if (fseek(fp, 0, SEEK_END) != 0) {
        free(page);
        THROW(RC_WRITE_FAILED, "Unable to seek to end of file");
    }

    if (fwrite(page, 1, PAGE_SIZE, fp) != PAGE_SIZE) {
        free(page);
        THROW(RC_WRITE_FAILED, "Unable to append page");
    }

    if (fflush(fp) != 0) {
        free(page);
        THROW(RC_WRITE_FAILED, "Unable to flush page");
    }

    free(page);

    fHandle->totalNumPages++;

    return RC_OK;
}

/**
 * Ensure the file has at least numberOfPages pages.
 * If the file has fewer pages, append empty pages.
 *
 * @param numberOfPages Minimum number of pages required
 * @param fHandle Pointer to file handle
 * @return RC_OK on success, error code on failure
 */
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
    RC rc;

    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    if (numberOfPages < 0)
        THROW(RC_INVALID_PAGE_NUM, "Invalid number of pages");

    while (fHandle->totalNumPages < numberOfPages) {
        rc = appendEmptyBlock(fHandle);

        if (rc != RC_OK)
            return rc;
    }

    return RC_OK;
}