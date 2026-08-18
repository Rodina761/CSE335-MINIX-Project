# Project status

| Requirement | Implementation | Deterministic test | MINIX VM evidence |
|---|---|---|---|
| 1. Scheduling | Complete | Included | Pending VM run |
| 2. Paging | Complete | Included | Pending VM run |
| 3. MFS extents | Complete | Included | Pending VM run |

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
- [ ] Build and execute in the student's MINIX 3.3 VM
- [ ] Import final measured CSV files into the report
- [x] Final visual/word-count QA of DOCX report and PPTX
- [ ] Create source archive and push private GitHub repository

Report QA: 9,956 words and 27 rendered pages. Presentation QA: 12 rendered
slides. The unchecked VM item is deliberately explicit: source inspection on Windows is
not a substitute for compiling and running against MINIX headers, libraries,
servers, and a scratch MFS device.
