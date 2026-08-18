import fs from "node:fs/promises";
import { fileURLToPath } from "node:url";
import {
  Presentation,
  PresentationFile,
} from "file:///C:/Users/Rodina/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const outputDir = new URL("../project-deliverables/deck-render/", import.meta.url);
const pptxPath = new URL("../project-deliverables/CSE335_MINIX_Project_Presentation.pptx", import.meta.url);
await fs.mkdir(outputDir, { recursive: true });

const W = 1280;
const H = 720;
const INK = "#000000";
const MUTED = "#555B65";
const PANEL = "#EDEDED";
const RULE = "#B8BCC4";
const BLUE = "#3D8DFF";
const CYAN = "#6DCBF4";
const PALE = "#D0EDFA";
const WHITE = "#FFFFFF";

const deck = Presentation.create({ slideSize: { width: W, height: H } });

function box(slide, x, y, w, h, fill = PANEL, line = "none", radius = false) {
  return slide.shapes.add({
    geometry: radius ? "roundRect" : "rect",
    position: { left: x, top: y, width: w, height: h },
    fill,
    line: { style: "solid", fill: line, width: line === "none" ? 0 : 1 },
  });
}

function text(slide, value, x, y, w, h, size = 20, opts = {}) {
  const shape = slide.shapes.add({
    geometry: "textbox",
    name: opts.name,
    position: { left: x, top: y, width: w, height: h },
    fill: "none",
    line: { style: "solid", fill: "none", width: 0 },
  });
  shape.text = value;
  shape.text.style = {
    fontSize: size,
    typeface: "Arial",
    bold: opts.bold ?? false,
    color: opts.color ?? INK,
    alignment: opts.align ?? "left",
    verticalAlignment: opts.vAlign ?? "top",
  };
  return shape;
}

function title(slide, value, kicker, number) {
  slide.background.fill = WHITE;
  text(slide, kicker.toUpperCase(), 48, 30, 760, 24, 13, { bold: true, color: MUTED });
  text(slide, value, 48, 64, 1160, 74, 38, { bold: true, name: `slide-${number}-title` });
  box(slide, 48, 145, 1184, 2, RULE);
  text(slide, String(number).padStart(2, "0"), 1176, 665, 56, 22, 13, { color: MUTED, align: "right" });
}

function footer(slide, value) {
  text(slide, value, 48, 664, 1000, 22, 12, { color: MUTED });
}

function metric(slide, x, y, w, label, value, sub) {
  box(slide, x, y, w, 174, PANEL);
  text(slide, label.toUpperCase(), x + 22, y + 20, w - 44, 24, 12, { bold: true, color: MUTED });
  text(slide, value, x + 22, y + 55, w - 44, 58, 42, { bold: true, color: BLUE });
  text(slide, sub, x + 22, y + 119, w - 44, 42, 16, { color: MUTED });
}

// 1 — editorial cover.
{
  const s = deck.slides.add();
  s.background.fill = WHITE;
  text(s, "CSE335  ·  SUMMER 2026", 48, 38, 500, 28, 15, { bold: true, color: MUTED });
  text(s, "Scheduling, paging,\nand extent-aware storage", 48, 182, 1040, 184, 64, { bold: true, name: "cover-title" });
  box(s, 48, 402, 470, 8, BLUE);
  text(s, "Three reproducible experiments inside MINIX 3.3.0", 48, 438, 800, 44, 27, { color: MUTED });
  text(s, "Rodina and project team", 48, 608, 500, 30, 18, { bold: true });
  text(s, "Source · tests · report · presentation", 48, 642, 700, 24, 15, { color: MUTED });
}

// 2 — scope.
{
  const s = deck.slides.add();
  title(s, "One clean baseline, three controlled interventions", "Project scope", 2);
  metric(s, 48, 202, 360, "Requirement 1", "4 policies", "RR · SJF · priority · MLFQ");
  metric(s, 460, 202, 360, "Requirement 2", "2 policies", "FIFO · exact LRU");
  metric(s, 872, 202, 360, "Requirement 3", "6 extents", "1 · 2 · 4 · 8 · 16 · 32 blocks");
  text(s, "Baseline tag", 48, 436, 180, 24, 14, { color: MUTED });
  text(s, "baseline-v3.3.0", 48, 466, 340, 36, 25, { bold: true });
  text(s, "Every experiment reads text configuration and writes CSV. All three native matrices and their validation log are archived with the submission.", 460, 436, 772, 100, 22);
  footer(s, "Repository history makes every change reviewable against the untouched release.");
}

// 3 — architecture.
{
  const s = deck.slides.add();
  title(s, "The design respects MINIX subsystem boundaries", "Architecture", 3);
  const xs = [48, 455, 862];
  const heads = ["REAL WORKERS", "SOFTWARE VM MODEL", "REAL MFS PATH"];
  const bodies = [
    "fork + pipes\ncontrolled dispatch\nlogical policy metrics\nwall-time confirmation",
    "sparse hierarchy\nbyte-address trace\nFIFO / LRU frames\nnative CSV evidence",
    "zone bitmap scan\nexact allocation origin\nsafe fallback\nunmount counters",
  ];
  for (let i = 0; i < 3; i++) {
    box(s, xs[i], 192, 370, 334, i === 2 ? PALE : PANEL);
    text(s, heads[i], xs[i] + 24, 218, 322, 28, 14, { bold: true, color: MUTED });
    text(s, bodies[i], xs[i] + 24, 274, 322, 190, 25);
  }
  text(s, "Controlled experiment", 48, 558, 774, 32, 19, { bold: true, color: BLUE });
  text(s, "Production allocation policy", 862, 558, 370, 32, 19, { bold: true, color: BLUE });
  footer(s, "The configurable paging hierarchy does not claim to reprogram x86 MMU geometry.");
}

// 4 — scheduling mechanism.
{
  const s = deck.slides.add();
  title(s, "Real child processes make policy effects observable", "Requirement 1", 4);
  box(s, 48, 190, 306, 352, PALE);
  text(s, "PARENT\nDISPATCHER", 76, 230, 250, 78, 30, { bold: true });
  text(s, "select policy\nadvance logical time\nrecord metrics", 76, 344, 246, 130, 22);
  const ys = [184, 302, 420];
  for (let i = 0; i < 3; i++) {
    box(s, 500, ys[i], 270, 86, PANEL);
    text(s, `WORKER P${i + 1}`, 522, ys[i] + 16, 220, 24, 16, { bold: true });
    text(s, "CPU work → reply", 522, ys[i] + 46, 220, 24, 17, { color: MUTED });
    box(s, 385, ys[i] + 41, 95, 3, BLUE);
  }
  text(s, "Logical clock", 874, 198, 310, 32, 18, { bold: true, color: MUTED });
  text(s, "turnaround\nwaiting\nresponse\nmakespan\ndispatches", 874, 250, 310, 210, 27);
  text(s, "Wall clock confirms actual execution; it does not distort logical policy metrics.", 874, 484, 310, 72, 18, { color: MUTED });
  footer(s, "Children exist from startup but become eligible only at their configured logical arrival.");
}

// 5 — scheduling evidence.
{
  const s = deck.slides.add();
  title(s, "SJF minimizes mean waiting on the known workload", "Requirement 1 · native MINIX result", 5);
  s.charts.add("bar", {
    position: { left: 48, top: 174, width: 730, height: 430 },
    categories: ["RR", "SJF", "Priority", "MLFQ"],
    series: [
      { name: "Turnaround", values: [185, 137, 155, 191], fill: CYAN },
      { name: "Waiting", values: [133, 85, 103, 139], fill: BLUE },
    ],
    hasLegend: true,
    legend: { position: "bottom", overlay: false },
    dataLabels: { showValue: true },
    yAxis: { min: 0, max: 220, majorUnit: 50, majorGridlines: { style: "solid", fill: PANEL, width: 1 } },
    xAxis: { textStyle: { typeface: "Arial", fontSize: "13px", color: INK } },
    barOptions: { direction: "column", grouping: "clustered", gapWidth: 70 },
  });
  metric(s, 842, 188, 342, "Fastest response", "5 ms", "MLFQ average response");
  metric(s, 842, 394, 342, "Fewest dispatches", "5", "SJF and priority");
  footer(s, "Measured from known.conf in MINIX; test_known.sh verified the CSV.");
}

// 6 — paging translation.
{
  const s = deck.slides.add();
  title(s, "Address geometry is configurable—and validated exactly", "Requirement 2", 6);
  text(s, "32-bit virtual address", 48, 182, 500, 30, 20, { bold: true });
  const segments = [
    [48, 240, 382, CYAN, "LEVEL 1\n10 bits"],
    [430, 240, 382, BLUE, "LEVEL 2\n10 bits"],
    [812, 240, 420, PANEL, "PAGE OFFSET\n12 bits"],
  ];
  for (const [x, y, w, fill, label] of segments) {
    box(s, x, y, w, 128, fill);
    text(s, label, x + 22, y + 30, w - 44, 70, 24, { bold: true, color: fill === BLUE ? WHITE : INK });
  }
  text(s, "virtual page number", 48, 394, 764, 28, 17, { bold: true, color: MUTED, align: "center" });
  text(s, "byte offset", 812, 394, 420, 28, 17, { bold: true, color: MUTED, align: "center" });
  box(s, 48, 464, 1184, 1, RULE);
  text(s, "Invariant", 48, 500, 170, 28, 15, { bold: true, color: BLUE });
  text(s, "Σ level bits + log₂(page size) = address bits", 230, 493, 850, 42, 28, { bold: true });
  footer(s, "Sparse nodes are allocated only for hierarchy paths touched by the byte-address trace.");
}

// 7 — paging evidence.
{
  const s = deck.slides.add();
  title(s, "Larger pages reduce faults for this fixed-byte locality trace", "Requirement 2 · native MINIX matrix", 7);
  s.charts.add("line", {
    position: { left: 48, top: 174, width: 790, height: 430 },
    categories: ["1 KiB", "2 KiB", "4 KiB", "8 KiB"],
    series: [
      { name: "FIFO", values: [6937, 4735, 2666, 1378], fill: CYAN, line: { fill: CYAN, width: 3 } },
      { name: "LRU", values: [6791, 4047, 1655, 1064], fill: BLUE, line: { fill: BLUE, width: 3 } },
    ],
    hasLegend: true,
    legend: { position: "bottom", overlay: false },
    dataLabels: { showValue: true },
    yAxis: { min: 0, max: 8000, majorUnit: 2000, majorGridlines: { style: "solid", fill: PANEL, width: 1 } },
    xAxis: { textStyle: { typeface: "Arial", fontSize: "13px", color: INK } },
  });
  text(s, "What it means", 888, 194, 300, 28, 16, { bold: true, color: BLUE });
  text(s, "Each frame covers more of the same byte working set.\n\nLRU benefits more because the trace has locality.\n\nThis does not prove that the largest page is universally best.", 888, 244, 302, 282, 22);
  footer(s, "64 simulated frames · 10,000 references · measured in the native MINIX matrix.");
}

// 8 — MFS allocation path.
{
  const s = deck.slides.add();
  title(s, "MFS prefers a free run without changing its disk format", "Requirement 3", 8);
  const labels = [
    [48, "WRITE NEEDS\nA NEW ZONE"],
    [344, "SCAN BITMAP\nFOR N FREE"],
    [640, "ALLOCATE AT\nEXACT ORIGIN"],
    [936, "FALL BACK\nIF N/A"],
  ];
  for (let i = 0; i < labels.length; i++) {
    const [x, label] = labels[i];
    box(s, x, 228, 248, 178, i === 2 ? PALE : PANEL);
    text(s, label, x + 22, 272, 204, 86, 23, { bold: true });
    if (i < labels.length - 1) box(s, x + 248, 315, 48, 3, BLUE);
  }
  text(s, "Consistency rule", 48, 468, 250, 28, 15, { bold: true, color: BLUE });
  text(s, "The scan is read-only. Only the original allocator marks a zone allocated, so there are no invisible reservations to leak after a crash.", 298, 460, 886, 74, 22);
  text(s, "On-disk inode and indirect-zone structures remain unchanged.", 298, 548, 886, 32, 19, { bold: true, color: MUTED });
  footer(s, "mfs_extent_size=1 preserves stock behavior; allowed range is 1–1,024 zones.");
}

// 9 — extent benchmark.
{
  const s = deck.slides.add();
  title(s, "Correctness is measured before throughput", "Requirement 3 · benchmark", 9);
  const stages = ["mkdir", "create", "write", "fsync", "read", "verify", "unlink", "rmdir"];
  for (let i = 0; i < stages.length; i++) {
    const x = 48 + i * 148;
    box(s, x, 212, 126, 82, i === 5 ? PALE : PANEL);
    text(s, stages[i], x + 8, 238, 110, 28, 18, { bold: true, align: "center" });
    if (i < stages.length - 1) box(s, x + 126, 252, 22, 3, BLUE);
  }
  metric(s, 48, 358, 344, "Correctness gate", "0 errors", "byte-for-byte pattern check");
  metric(s, 468, 358, 344, "Matrix", "18 rows", "6 preferences × 3 runs");
  metric(s, 888, 358, 344, "Raw metrics", "4 clocks", "create · write · read · remove");
  footer(s, "Run only on a disposable secondary MFS mount with matching service and benchmark extent sizes.");
}

// 10 — reproducibility.
{
  const s = deck.slides.add();
  title(s, "Three smoke tests guard the experiment matrices", "Verification", 10);
  const rows = [
    ["Scheduling", "Known averages", "CSV rows + child exit", "test_known.sh"],
    ["Paging", "Known reference string", "FIFO / LRU fault totals", "test_known.sh"],
    ["Extents", "Two real file runs", "zero verification errors", "test_known.sh"],
  ];
  const cols = [48, 330, 632, 964];
  const widths = [282, 302, 332, 268];
  const heads = ["AREA", "ORACLE", "GATE", "SCRIPT"];
  for (let c = 0; c < 4; c++) {
    box(s, cols[c], 184, widths[c], 56, c === 0 ? PALE : PANEL);
    text(s, heads[c], cols[c] + 14, 202, widths[c] - 28, 22, 14, { bold: true, color: MUTED });
  }
  for (let r = 0; r < rows.length; r++) {
    for (let c = 0; c < 4; c++) {
      box(s, cols[c], 240 + r * 92, widths[c], 92, WHITE, RULE);
      text(s, rows[r][c], cols[c] + 14, 264 + r * 92, widths[c] - 28, 46, 18, { bold: c === 0 });
    }
  }
  text(s, "Fail fast", 48, 552, 180, 28, 16, { bold: true, color: BLUE });
  text(s, "No performance matrix is accepted after a compile failure, oracle mismatch, abnormal child exit, or nonzero data-verification count.", 230, 544, 1002, 58, 21);
  footer(s, "Raw CSV and console logs are preserved before summaries or graphs are created.");
}

// 11 — status.
{
  const s = deck.slides.add();
  title(s, "Native verification is complete", "Submission status", 11);
  metric(s, 48, 184, 350, "Source", "Complete", "three requirements committed");
  metric(s, 465, 184, 350, "Report", "1,256", "words · 7 rendered pages");
  metric(s, 882, 184, 350, "VM evidence", "Complete", "3 tests · 170 data rows");
  text(s, "Archived native evidence", 48, 424, 580, 34, 23, { bold: true });
  text(s, "1  scheduling matrix · 80 rows\n2  paging matrix · 72 rows\n3  extent matrix · 18 rows · zero errors\n4  MFS preferred-run hits · zero fallbacks", 48, 476, 650, 142, 21);
  box(s, 778, 428, 454, 154, PALE);
  text(s, "VALIDATED PLATFORM", 804, 450, 390, 24, 14, { bold: true, color: MUTED });
  text(s, "MINIX 3.3.0\ni386 · VirtualBox", 804, 492, 390, 66, 27, { bold: true });
  footer(s, "Raw CSV, checksums, test output, and MFS logs are included.");
}

// 12 — close.
{
  const s = deck.slides.add();
  s.background.fill = WHITE;
  text(s, "FINAL TAKEAWAY", 48, 42, 400, 24, 14, { bold: true, color: MUTED });
  text(s, "Policy only means something\nwhen workload, mechanism,\nand evidence stay aligned.", 48, 168, 1040, 238, 58, { bold: true });
  box(s, 48, 446, 680, 8, BLUE);
  text(s, "Real processes. Configurable page structures. Real MFS allocation. Reproducible CSV.", 48, 488, 1040, 68, 24, { color: MUTED });
  text(s, "Ready to submit: final commit, source archive, report, slides, and raw evidence.", 48, 622, 1050, 30, 19, { bold: true });
}

async function writeBlob(url, blob) {
  await fs.writeFile(url, new Uint8Array(await blob.arrayBuffer()));
}

for (const [index, slide] of deck.slides.items.entries()) {
  const stem = `slide-${String(index + 1).padStart(2, "0")}`;
  await writeBlob(new URL(`${stem}.png`, outputDir), await deck.export({ slide, format: "png", scale: 1 }));
  const layout = await slide.export({ format: "layout" });
  await fs.writeFile(new URL(`${stem}.layout.json`, outputDir), await layout.text());
}
await writeBlob(new URL("deck-montage.webp", outputDir), await deck.export({ format: "webp", montage: true, scale: 1 }));
const pptx = await PresentationFile.exportPptx(deck);
await pptx.save(fileURLToPath(pptxPath));
console.log(fileURLToPath(pptxPath));
