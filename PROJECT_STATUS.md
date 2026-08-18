# Project status

| Requirement | Implementation | Deterministic test | MINIX VM evidence |
|---|---|---|---|
| 1. Scheduling | Complete | Passed | Complete: 80 data rows |
| 2. Paging | Complete | Passed | Complete: 72 data rows |
| 3. MFS extents | Complete | Passed | Complete: 18 data rows + MFS logs |

## Submission checklist

- [x] Clean MINIX 3.3.0 baseline preserved and tagged
- [x] RR, SJF, priority and MLFQ implementation
- [x] Real worker processes and scheduling CSV metrics
- [x] Configurable hierarchical paging with FIFO and LRU
- [x] Page-fault, empty-frame and hierarchy metrics
- [x] MFS free-space investigation and extent-biased allocator
- [x] Real file/directory create, write, read, verify and remove benchmark
- [x] Editable configuration files for all three requirements
- [x] Deterministic smoke tests and matrix scripts
- [x] Build and execute in the student's MINIX 3.3 VM
- [x] Import final measured CSV files into the report
- [x] Final visual/word-count QA of DOCX report and PPTX
- [x] Create source archive and push private GitHub repository

Native validation: MINIX 3.3.0/i386, all four build checks succeeded, all three
deterministic tests passed, and 173 CSV lines were archived with checksums. The
extent run used a disposable vnode-backed image; all 18 rows had zero data
verification errors and MFS reported zero allocation fallbacks.

Report QA: 9,906 words and 27 rendered pages. Presentation QA: 12 rendered
slides.
