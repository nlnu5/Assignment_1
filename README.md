# CS 525 - Assignment 1
## Disk Storage and Buffer Manager

**Course:** CS 525 - Advanced Database Organization  
**Assignment:** 1 - Disk Storage and Buffer Manager  
**Student:** Nishit Raj Lnu  
**Student ID:** A20652913  

---

## Overview

This assignment implements a Disk Storage Manager and a Buffer Manager in C.

The required Buffer Manager replacement strategies, FIFO and LRU, are implemented. The optional bonus features, CLOCK replacement and buffer hit/miss statistics, are also implemented.

---

## Files

### `storage_mgr.c`

Implementation of the Storage Manager functions required by the assignment.

### `buffer_mgr.c`

Implementation of the Buffer Manager functions required by the assignment, including the optional CLOCK replacement policy and buffer hit/miss statistics.

---

## Testing

The professor-provided basic tests were used for functional verification. The test programs state that hidden tests are more comprehensive.

### Storage Manager Basic Tests

Compilation:

```bash
gcc -Wall -Wextra -std=c11 storage_mgr.c dberror.c test_storage_basic.c -o test_storage
```

Execution:

```bash
./test_storage
```

Result:

```text
Tests Passed: 22
Tests Failed: 0
```

### Buffer Manager Basic Tests

The supplied buffer tests require `buffer_mgr_stat.h`. The supplied test package did not include this header, so a matching local header was created without modifying the professor-provided test source.

Compilation:

```bash
gcc -Wall -Wextra -std=c11 buffer_mgr.c storage_mgr.c dberror.c buffer_mgr_stat.c test_buffer_basic.c -o test_buffer
```

Execution:

```bash
./test_buffer
```

Result:

```text
Tests Passed: 15
Tests Failed: 0
```

### Combined Basic-Test Result

```text
Storage Manager: 22/22 passed
Buffer Manager:  15/15 passed
--------------------------------
Total:            37/37 passed
```

---

## Optional Bonus (+5 points)

The assignment lists the following optional bonus features:

- CLOCK replacement policy implementation
- Buffer hit/miss statistics

Both bonus features have been implemented in `buffer_mgr.c`.

### CLOCK Replacement

The Buffer Manager supports:

```text
RS_CLOCK
```

in addition to the required:

```text
RS_FIFO
RS_LRU
```

The CLOCK implementation uses a reference bit for each frame and a clock hand to select an unpinned replacement frame.

### Buffer Hit/Miss Statistics

The Buffer Manager maintains counters for:

```text
Buffer hits
Buffer misses
```

The implementation provides:

```c
getNumHits()
getNumMisses()
```

for retrieving the counters.


## Ubuntu Test Screenshots

### Screenshot 1 — Buffer Manager Basic Tests

![Buffer manager test result](image.png)

Expected result:

```text
Tests Passed: 15
Tests Failed: 0
```

---

### Screenshot 2 — Storage Manager Basic Tests

![Storage manager test result](image-1.png)
Expected result:

```text
Tests Passed: 22
Tests Failed: 0
```

---

### Screenshot 3 — Buffer Manager Valgrind

 ![Valgrind test result](image-2.png)

Expected result:

```text
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```


## Dependencies

The implementation uses:

- C
- GCC
- Standard C libraries
- The provided assignment header files

No external libraries are required.

The implementation uses the provided:

```text
buffer_mgr.h
storage_mgr.h
dberror.h
dt.h
```

---

## AI Assistance and Academic Integrity

AI assistance was used during the development of this assignment.

### AI Tool

**ChatGPT (OpenAI)** was used as an assistance, debugging, and review tool for:

- Understanding the assignment requirements
- Understanding the Storage Manager and Buffer Manager specifications
- Explaining FIFO, LRU, and CLOCK replacement concepts
- Interpreting compiler errors and warnings
- Debugging compilation and test issues
- Reviewing implementation logic
- Reviewing memory-management behavior
- Interpreting Valgrind output
- Reviewing the implementation against the professor-provided tests
- Designing additional tests for the optional bonus functionality
- Reviewing and organizing the README

AI assistance was used for guidance and development support. The student reviewed and tested the implementation and is responsible for the submitted code and its behavior.

The student should be able to explain the submitted implementation and the use of FIFO, LRU, CLOCK, dirty-page handling, pin/unpin behavior, and hit/miss tracking.

### Testing and Verification

The implementation was compiled using GCC and tested using the professor-provided basic tests.

The required functionality was verified with:

```text
Storage Manager: 22/22 passed
Buffer Manager:  15/15 passed
Total:            37/37 passed
```

Valgrind was also used to check for memory leaks.

### Online Sources

No external online source was used to copy implementation code.

The assignment specification, professor-provided starter files, professor-provided tests, and course materials were used as the primary sources.

---

## Final Verification

### Required Functional Tests

```text
Storage Manager Basic Tests: 22 passed, 0 failed
Buffer Manager Basic Tests:  15 passed, 0 failed
Total:                        37 passed, 0 failed
```

### Optional Bonus

```text
CLOCK replacement:            IMPLEMENTED
Buffer hit/miss statistics:   IMPLEMENTED
```

Bonus test results should be added after running the dedicated bonus tests.

### Valgrind

```text
Buffer Manager:
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts

Storage Manager:
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

The professor-provided tests are basic tests only; passing them does not guarantee the result of the hidden autograder tests.
