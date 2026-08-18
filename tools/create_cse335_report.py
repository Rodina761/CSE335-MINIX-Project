from pathlib import Path
import re

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
    p.paragraph_format.space_after = Pt(9)
    p.add_run().add_picture(str(ASSETS / filename), width=Inches(width))


def page_number(p):
    p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    p.add_run("Page ")
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"), "PAGE")
    p._p.append(field)


def clean_markdown(text):
    """Convert the small inline-Markdown subset used by the project manuals."""
    text = re.sub(r"!\[([^]]*)\]\([^)]+\)", r"\1", text)
    text = re.sub(r"\[([^]]+)\]\([^)]+\)", r"\1", text)
    return text.replace("**", "").replace("__", "").replace("`", "").strip()


def append_markdown(doc, path, title=None):
    """Append a project manual while preserving headings, lists, tables and code."""
    if title:
        doc.add_heading(title, 1)
    lines = path.read_text(encoding="utf-8").splitlines()
    i, in_code, code_lines = 0, False, []
    while i < len(lines):
        raw = lines[i].rstrip()
        stripped = raw.strip()
        if stripped.startswith("```"):
            if in_code:
                code(doc, code_lines)
                code_lines = []
            in_code = not in_code
            i += 1
            continue
        if in_code:
            code_lines.append(raw)
            i += 1
            continue
        if not stripped or stripped == "---":
            i += 1
            continue
        if stripped.startswith("|") and i + 1 < len(lines) and re.match(
            r"^\s*\|?\s*:?-{3,}", lines[i + 1]
        ):
            headers = [clean_markdown(x) for x in stripped.strip("|").split("|")]
            i += 2
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append([
                    clean_markdown(x) for x in lines[i].strip().strip("|").split("|")
                ])
                i += 1
            usable = 9400
            widths = [usable // len(headers)] * len(headers)
            table(doc, headers, rows, widths)
            continue
        heading = re.match(r"^(#{1,3})\s+(.+)$", stripped)
        if heading:
            level = min(len(heading.group(1)) + (1 if title else 0), 3)
            doc.add_heading(clean_markdown(heading.group(2)), level)
            i += 1
            continue
        bullet = re.match(r"^[-*]\s+(.+)$", stripped)
        numbered = re.match(r"^\d+[.)]\s+(.+)$", stripped)
        if bullet or numbered:
            item = bullet.group(1) if bullet else numbered.group(1)
            doc.add_paragraph(
                clean_markdown(item), style="List Bullet" if bullet else "List Number"
            )
            i += 1
            continue
        paragraph = [stripped]
        i += 1
        while i < len(lines):
            candidate = lines[i].strip()
            if (
                not candidate
                or candidate.startswith(("#", "```", "|", "- ", "* "))
                or re.match(r"^\d+[.)]\s+", candidate)
            ):
                break
            paragraph.append(candidate)
            i += 1
        para(doc, clean_markdown(" ".join(paragraph)))
    if code_lines:
        code(doc, code_lines)


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
header.text = ""
page_number(section.footer.paragraphs[0])

# Cover
p = doc.add_paragraph()
p.paragraph_format.space_before = Pt(10)
p.paragraph_format.space_after = Pt(12)
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.add_run().add_picture(str(ASSETS / "ain-shams-cover-header.png"), width=Inches(6.55))
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("TERM PROJECT REPORT")
r.bold, r.font.size = True, Pt(12)
r.font.color.rgb = RGBColor.from_string(BLUE)
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(4)
r = p.add_run("MINIX 3.3.0\nImplementation and Native Test Report")
r.bold, r.font.size = True, Pt(23)
r.font.color.rgb = RGBColor.from_string(DARK)
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(10)
r = p.add_run("Scheduling • Hierarchical Paging • Extent-Aware MFS Allocation")
r.italic, r.font.size = True, Pt(12)
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_before = Pt(4)
p.paragraph_format.space_after = Pt(5)
r = p.add_run("TEAM MEMBERS")
r.bold, r.font.size = True, Pt(11)
r.font.color.rgb = RGBColor.from_string(BLUE)
table(doc, ["Name", "Student ID"], [
    ["Jana Ahmed Saieed", "24P0410"],
    ["Mohamed Ehab Abdelbary Ibrahem", "2300570"],
    ["Abdelrahman Ashour Hassan", "2101736"],
    ["Mostafa Hamdy Mohamed Elzoghby", "2300672"],
], [6600, 2700])
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_before = Pt(7)
p.paragraph_format.space_after = Pt(0)
r = p.add_run("Validated on MINIX 3.3.0 • Oracle VirtualBox")
r.italic, r.font.size = True, Pt(9)
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

doc.add_page_break()
doc.add_heading("9. Research basis and MINIX internal structure", 1)
para(doc, "MINIX 3 follows a microkernel architecture in which the kernel retains mechanisms that require privileged execution, while many services traditionally placed in a monolithic kernel execute as isolated user-mode processes. The process manager, virtual file-system service, memory-related services, device drivers, and concrete file-system servers communicate through explicit interprocess communication. This organization reduces the trusted kernel surface and provides fault isolation, but it also means that a modification must be placed at the correct architectural boundary. The project therefore treats the scheduler and paging requirements as controlled native experiments and places the persistent allocation-policy change in the MFS server. This interpretation is consistent with the MINIX 3 design literature and source organization [1, 2].")
para(doc, "CPU scheduling is evaluated with turnaround time, waiting time, response time, makespan, and dispatch count. Turnaround is completion minus arrival; waiting is turnaround minus demanded CPU service; response is first start minus arrival. Round Robin emphasizes fairness through bounded quanta. Shortest Job First minimizes average waiting for a known batch under its ideal assumptions, but it can delay long jobs. Static priority scheduling expresses importance directly but can starve low-priority work without aging. A multilevel feedback queue adapts priority from observed behavior, favoring short or interactive jobs while moving CPU-bound jobs toward longer quanta. These definitions and trade-offs follow standard operating-systems treatments [3, 4].")
para(doc, "Hierarchical paging divides a virtual address into a page offset and one index per configured level. Only leaves reached by the trace need to exist in the sparse experimental structure. FIFO replaces the page that has resided in memory longest, whereas LRU replaces the page whose latest reference is oldest. FIFO requires little history but can exhibit Belady's anomaly; stack algorithms such as LRU do not have that anomaly under the ideal reference model. The experiment reports faults, replacements, hits, remaining empty frames, hierarchy nodes, and lookup work so that replacement behavior and translation-structure cost are not confused [3, 5].")
para(doc, "MINIX File System uses allocation bitmaps to record free and used zones. A file's inode identifies data through direct and indirect zone references, while directories are files containing name-to-inode mappings. The extent preference implemented here does not replace this format. It searches the existing zone bitmap for a user-requested contiguous free run and selects the beginning of that run as the next allocation origin. The normal bitmap allocator still performs the actual state change, which preserves existing accounting and compatibility. The design is intentionally a preference layer rather than a new persistent extent tree [1, 2].")

append_markdown(doc, ROOT / "discussion-guides" / "requirement-1-scheduling.md", "10. Detailed Requirement 1 implementation and discussion manual")
append_markdown(doc, ROOT / "discussion-guides" / "requirement-2-paging.md", "11. Detailed Requirement 2 implementation and discussion manual")
append_markdown(doc, ROOT / "discussion-guides" / "requirement-3-mfs-extents.md", "12. Detailed Requirement 3 implementation and discussion manual")
append_markdown(doc, ROOT / "discussion-guides" / "README.md", "13. Installation, configuration, validation and demonstration runbook")

doc.add_heading("14. References", 1)
for reference in [
    "[1] A. S. Tanenbaum and A. S. Woodhull, Operating Systems: Design and Implementation, 3rd ed. Upper Saddle River, NJ: Pearson Prentice Hall, 2006.",
    "[2] MINIX 3 Project, MINIX 3.3.0 source tree and official documentation, Vrije Universiteit Amsterdam, release 3.3.0. The submitted source baseline is preserved under tag baseline-v3.3.0.",
    "[3] A. Silberschatz, P. B. Galvin, and G. Gagne, Operating System Concepts, 10th ed. Hoboken, NJ: Wiley, 2018.",
    "[4] A. S. Tanenbaum and H. Bos, Modern Operating Systems, 4th ed. Boston, MA: Pearson, 2015.",
    "[5] L. A. Belady, A study of replacement algorithms for a virtual-storage computer, IBM Systems Journal, vol. 5, no. 2, pp. 78-101, 1966.",
]:
    para(doc, reference)

doc.core_properties.title = "CSE335 MINIX 3.3.0 Implementation and Native Test Report"
doc.core_properties.author = (
    "Jana Ahmed Saieed; Mohamed Ehab Abdelbary Ibrahem; "
    "Abdelrahman Ashour Hassan; Mostafa Hamdy Mohamed Elzoghby"
)
doc.save(OUT)
print(f"Created {OUT}")
