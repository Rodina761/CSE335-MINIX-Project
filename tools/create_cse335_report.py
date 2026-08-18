from pathlib import Path

from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "project-deliverables" / "CSE335_MINIX_Project_Report.docx"
ASSETS = ROOT / "project-deliverables" / "report-assets"
BLUE, DARK, GRAY = "2E74B5", "1F4D78", "F2F4F7"


def shade(cell, color):
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), color)
    cell._tc.get_or_add_tcPr().append(shd)


def set_width(cell, width):
    props = cell._tc.get_or_add_tcPr()
    tcw = props.first_child_found_in("w:tcW")
    if tcw is None:
        tcw = OxmlElement("w:tcW")
        props.append(tcw)
    tcw.set(qn("w:w"), str(width))
    tcw.set(qn("w:type"), "dxa")


def table(doc, headers, rows, widths):
    result = doc.add_table(rows=1, cols=len(headers))
    result.style, result.autofit = "Table Grid", False
    result.alignment = WD_TABLE_ALIGNMENT.LEFT
    for i, header in enumerate(headers):
        cell = result.rows[0].cells[i]
        cell.text = header
        shade(cell, GRAY)
        set_width(cell, widths[i])
        for run in cell.paragraphs[0].runs:
            run.bold, run.font.size = True, Pt(8.5)
    for values in rows:
        cells = result.add_row().cells
        for i, value in enumerate(values):
            cells[i].text = str(value)
            set_width(cells[i], widths[i])
            cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for run in cells[i].paragraphs[0].runs:
                run.font.size = Pt(8.5)
    return result


def para(doc, text):
    doc.add_paragraph(text)


def code(doc, lines):
    for text in lines:
        p = doc.add_paragraph(style="No Spacing")
        p.paragraph_format.left_indent = Inches(0.22)
        p.paragraph_format.space_after = Pt(2)
        run = p.add_run(text)
        run.font.name, run.font.size = "Consolas", Pt(8)
        run._element.rPr.rFonts.set(qn("w:ascii"), "Consolas")
        run._element.rPr.rFonts.set(qn("w:hAnsi"), "Consolas")


def figure(doc, filename, caption, width=6.25):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(4)
    p.add_run().add_picture(str(ASSETS / filename), width=Inches(width))
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(9)
    run = p.add_run(caption)
    run.italic, run.font.size = True, Pt(8.5)
    run.font.color.rgb = RGBColor(90, 90, 90)


def page_number(p):
    p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    p.add_run("Page ")
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"), "PAGE")
    p._p.append(field)


doc = Document()
section = doc.sections[0]
section.top_margin = section.bottom_margin = Inches(0.72)
section.left_margin = section.right_margin = Inches(0.82)
section.header_distance = section.footer_distance = Inches(0.35)
normal = doc.styles["Normal"]
normal.font.name, normal.font.size = "Calibri", Pt(10.2)
normal.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
normal.paragraph_format.space_after = Pt(6)
normal.paragraph_format.line_spacing = 1.12
for name, size, color, before, after in [("Heading 1", 16, BLUE, 15, 7), ("Heading 2", 12.5, BLUE, 10, 5), ("Heading 3", 11, DARK, 7, 3)]:
    style = doc.styles[name]
    style.font.name, style.font.size, style.font.bold = "Calibri", Pt(size), True
    style.font.color.rgb = RGBColor.from_string(color)
    style.paragraph_format.space_before, style.paragraph_format.space_after = Pt(before), Pt(after)
    style.paragraph_format.keep_with_next = True
header = section.header.paragraphs[0]
header.text = "CSE335 Operating Systems Project  |  MINIX 3.3.0"
header.runs[0].font.size = Pt(8.5)
header.runs[0].font.color.rgb = RGBColor(95, 99, 104)
page_number(section.footer.paragraphs[0])

# Cover
p = doc.add_paragraph()
p.paragraph_format.space_before = Pt(105)
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("CSE335 OPERATING SYSTEMS PROJECT")
r.bold, r.font.size = True, Pt(12)
r.font.color.rgb = RGBColor.from_string(BLUE)
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("MINIX 3.3.0\nImplementation and Native Test Report")
r.bold, r.font.size = True, Pt(27)
r.font.color.rgb = RGBColor.from_string(DARK)
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(60)
r = p.add_run("Scheduling • Hierarchical Paging • Extent-Aware MFS Allocation")
r.italic, r.font.size = True, Pt(12)
table(doc, ["Course", "Prepared by", "Term", "Validated"], [["CSE335", "Rodina and project team", "Summer 2026", "MINIX 3.3.0 VM"]], [1700, 3000, 1800, 2800])
para(doc, "This concise report explains what was changed, how it was built and tested, and what the native MINIX run produced. Raw CSV files and the complete validation log are included under project-deliverables/native-results.")
doc.add_page_break()

doc.add_heading("1. Project summary", 1)
para(doc, "The project starts from the official MINIX 3.3.0 release and implements all three requirements as reproducible experiments. Requirement 1 compares four CPU-scheduling policies using real child processes. Requirement 2 models configurable hierarchical page tables and compares FIFO with LRU replacement. Requirement 3 modifies the real MFS zone-allocation path to prefer contiguous free runs, then tests file and directory operations on a disposable MFS image.")
table(doc, ["Requirement", "Implementation", "Evidence"], [
    ["1. Scheduling", "RR, SJF, priority, and MLFQ dispatcher with real workers", "Known-workload PASS; 80 data rows"],
    ["2. Paging", "Sparse configurable hierarchy with FIFO/LRU frames", "Known reference-string PASS; 72 data rows"],
    ["3. MFS extents", "Exact-origin bitmap allocation and free-run preference", "I/O PASS; 18 rows; zero data errors"],
], [1900, 4700, 2800])
doc.add_heading("Scope and interpretation", 2)
para(doc, "The scheduling command controls logical slices but does not replace the production kernel scheduler. The paging command is a native MINIX simulator because x86 hardware page geometry cannot be changed from a configuration file. The MFS work is a real server modification: it changes allocation preference without changing the on-disk format. These boundaries keep the claims accurate.")

doc.add_heading("2. How the project was completed", 1)
table(doc, ["Stage", "What was done"], [
    ["1. Clean baseline", "The untouched 3.3.0 release was recorded so every modification is reviewable with git diff."],
    ["2. Implement", "Three commands, parsers, matrix scripts, deterministic tests, and a focused MFS allocator change were added."],
    ["3. Transfer", "The source was copied to /usr/src in the MINIX VM for the native compiler and headers."],
    ["4. Build", "The commands and MFS server were compiled and installed from the submitted source."],
    ["5. Correctness gate", "Known workloads ran first; a failed assertion or data mismatch would stop collection."],
    ["6. Experiment", "Scripts varied one main parameter and wrote raw CSV. Extent tests remounted a disposable MFS image."],
    ["7. Preserve", "CSV matrices, checksums, validation log, and console screenshots were copied into the deliverables."],
], [1900, 7500])
para(doc, "Compilation and execution inside MINIX were the authoritative gate. Windows-side editing alone was never treated as proof, because only the VM has the correct compiler, headers, services, and runtime behavior.")

doc.add_page_break()
doc.add_heading("3. Requirement 1 — scheduling", 1)
doc.add_heading("Implementation", 2)
para(doc, "The parent parses the workload, creates one child per job, and uses pipes to grant logical CPU slices. Each child performs deterministic integer work and acknowledges the slice. The parent selects only arrived jobs, records first start and completion, and calculates turnaround, waiting, response, makespan, and dispatch count. RR uses a rotating cursor, SJF selects the smallest remaining burst, priority selects the smallest numeric priority, and MLFQ uses increasing quanta with demotion.")
figure(doc, "code-scheduling.png", "Figure 1. Edited scheduler loop: policy selection, idle-time advance, and slice calculation.")
doc.add_heading("Native result", 2)
table(doc, ["Algorithm", "Avg turnaround", "Avg waiting", "Avg response", "Makespan", "Dispatches"], [
    ["RR", "185 ms", "133 ms", "17 ms", "260 ms", "14"], ["SJF", "137 ms", "85 ms", "85 ms", "260 ms", "5"],
    ["Priority", "155 ms", "103 ms", "103 ms", "260 ms", "5"], ["MLFQ", "191 ms", "139 ms", "5 ms", "260 ms", "15"],
], [1300, 1700, 1500, 1500, 1500, 1900])
para(doc, "SJF produced the lowest average waiting and turnaround for this workload. MLFQ produced the quickest first response but required the most dispatches. Makespan is equal because every policy performs the same 260 logical milliseconds of work.")

doc.add_page_break()
doc.add_heading("4. Requirement 2 — hierarchical paging", 1)
doc.add_heading("Implementation", 2)
para(doc, "The configuration specifies address width, page size, hierarchy levels and index widths, frame count, replacement policy, and deterministic trace parameters. Index bits plus page-offset bits must equal the address width. The sparse hierarchy creates only paths reached by the trace, allowing nodes, entries, and allocated bytes to be measured.")
para(doc, "Every byte address is converted to a virtual page. A present page is a hit. On a fault, an unused frame is selected first; otherwise FIFO chooses the smallest load timestamp and LRU chooses the smallest last-access timestamp. First fills count as faults but not replacements.")
figure(doc, "code-paging.png", "Figure 2. Edited FIFO/LRU victim-selection logic.", 5.55)
doc.add_heading("Native result", 2)
table(doc, ["Page size", "FIFO faults", "LRU faults", "Observed result"], [
    ["1,024 B", "6,937", "6,791", "LRU lower by 146"], ["2,048 B", "4,735", "4,047", "LRU lower by 688"],
    ["4,096 B", "2,666", "1,655", "LRU lower by 1,011"], ["8,192 B", "1,378", "1,064", "LRU lower by 314"],
], [1800, 1800, 1800, 4000])
para(doc, "For the controlled locality trace with 64 frames, LRU faulted less than FIFO at every page size. Larger pages reduced faults because each frame covered more of the fixed byte working set; this does not imply that larger pages are universally better because fragmentation and transfer cost also increase.")

doc.add_page_break()
doc.add_heading("5. Requirement 3 — extent-aware MFS", 1)
doc.add_heading("Implementation", 2)
para(doc, "Stock MFS allocates data zones through a bitmap. The change first scans for a free run matching mfs_extent_size. If found, its start becomes the exact origin passed to alloc_bit; otherwise the original behavior is retained and a fallback is counted. Only alloc_bit marks a zone allocated, so existing accounting and the on-disk format remain intact.")
figure(doc, "code-mfs-extents.png", "Figure 3. Edited MFS allocation path: preferred run, counters, and safe fallback.")
doc.add_heading("Native result", 2)
table(doc, ["Requested zones", "Searches", "Preferred-run hits", "Fallbacks"], [
    ["1", "0", "0", "0"], ["2", "6", "6", "0"], ["4", "6", "6", "0"], ["8", "6", "6", "0"], ["16", "6", "6", "0"], ["32", "6", "6", "0"],
], [2100, 2100, 3000, 2100])
para(doc, "The matrix used a disposable 64 MiB MFS image and produced 18 rows. Every file read matched its deterministic written pattern, so verify_errors stayed zero. All requested runs from 2 through 32 zones were found without fallback. Some short timings rounded to zero because of MINIX 3.3 timer resolution, so correctness and allocator counters are stronger evidence than tiny throughput differences.")

doc.add_page_break()
doc.add_heading("6. Native MINIX build and test evidence", 1)
doc.add_heading("Commands executed in the VM", 2)
code(doc, [
    "cd /usr/src && make includes",
    "cd /usr/src/minix/commands/schedexperiment && make && make install",
    "cd /usr/src/minix/commands/vmexperiment && make && make install",
    "cd /usr/src/minix/commands/extentexperiment && make && make install",
    "cd /usr/src/minix/fs/mfs && make && make install",
    "cd /usr/src/minix/commands/schedexperiment/tests && sh test_known.sh",
    "cd /usr/src/minix/commands/vmexperiment/tests && sh test_known.sh",
    "cd /usr/src/minix/commands/extentexperiment/tests && sh test_known.sh",
    "run_schedexperiments.sh /root/scheduling-matrix.csv",
    "run_vmexperiments.sh /root/paging-matrix.csv",
    "run_extent_matrix.sh /root/extent-matrix.csv",
])
para(doc, "The known tests are deterministic oracles: expected scheduling averages are checked exactly, FIFO/LRU totals are compared with a known reference string, and extent I/O is read back and byte-verified. Matrix collection ran only after all three tests passed.")
figure(doc, "minix-native-validation.png", "Figure 4. Actual VirtualBox MINIX console: PASS results, CSV row counts/checksums, and MFS counters.", 6.4)
doc.add_page_break()
doc.add_heading("MFS extent console output", 2)
figure(doc, "minix-mfs-extent-results.png", "Figure 5. Actual MINIX console: final extent-size counters recorded by MFS.", 6.4)

doc.add_heading("7. Changed files", 1)
table(doc, ["Area", "Main edited files", "Purpose"], [
    ["Scheduling", "minix/commands/schedexperiment/*", "Parser, dispatcher, metrics, test, and matrix script"],
    ["Paging", "minix/commands/vmexperiment/*", "Hierarchy, traces, FIFO/LRU, test, and matrix script"],
    ["MFS", "minix/fs/mfs/cache.c, super.c, main.c, mount.c", "Exact bitmap origin, free-run scan, option, counters"],
    ["Extent test", "minix/commands/extentexperiment/*", "Verified directory/file I/O and CSV metrics"],
    ["Integration", "minix/commands/Makefile; etc/Makefile; etc/*.conf", "Build/install entries and configurations"],
], [1600, 3900, 3900])
doc.add_heading("8. Conclusion", 1)
para(doc, "All three requirements were built and executed in MINIX 3.3.0. The native correctness gate passed for scheduling, FIFO/LRU paging, and extent file/directory I/O. The matrices contain 80 scheduling rows, 72 paging rows, and 18 extent rows; their checksums are preserved. Requirement 1 demonstrates responsiveness versus dispatch cost, Requirement 2 shows lower LRU faults for the chosen locality workload, and Requirement 3 confirms successful contiguous-run preference with safe fallback and correct data.")
para(doc, "Limitations are explicit: scheduler dispatches are experiment events rather than kernel context-switch counters; paging faults are simulated rather than hardware telemetry; and coarse MINIX timing weakens very short throughput measurements. These do not invalidate deterministic correctness or configuration-controlled comparisons.")
doc.add_heading("Submission evidence", 2)
for item in ["Raw results: project-deliverables/native-results/*.csv and native-validation.txt", "Source comparison: git diff baseline-v3.3.0..HEAD", "Report builder: tools/create_cse335_report.py"]:
    doc.add_paragraph(item, style="List Bullet")

doc.core_properties.title = "CSE335 MINIX 3.3.0 Implementation and Native Test Report"
doc.core_properties.author = "Rodina and project team"
doc.save(OUT)
print(f"Created {OUT}")
