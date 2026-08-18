from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "project-deliverables" / "report-assets"
OUT.mkdir(parents=True, exist_ok=True)

FONT = Path(r"C:\Windows\Fonts\consola.ttf")
BOLD = Path(r"C:\Windows\Fonts\consolab.ttf")


def code_image(source, start, end, destination, title):
    lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
    body = [f"{number:4}  {lines[number - 1].expandtabs(4)}" for number in range(start, end + 1)]
    draw_panel(body, destination, title, source.relative_to(ROOT).as_posix())


def draw_panel(lines, destination, title, subtitle):
    font = ImageFont.truetype(str(FONT), 25)
    title_font = ImageFont.truetype(str(BOLD), 29)
    small_font = ImageFont.truetype(str(FONT), 20)
    line_height = 34
    width = 1880
    height = 125 + line_height * len(lines) + 45
    image = Image.new("RGB", (width, height), "#0d1117")
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, width - 1, height - 1), outline="#30363d", width=3)
    draw.rectangle((0, 0, width, 82), fill="#181818")
    draw.rectangle((0, 0, 8, 82), fill="#007acc")
    draw.text((28, 17), title, font=title_font, fill="#f0f6fc")
    draw.text((28, 51), subtitle, font=small_font, fill="#8b949e")
    y = 102
    for line in lines:
        number, _, code = line.partition("  ")
        draw.text((25, y), number, font=font, fill="#6e7681")
        draw.text((105, y), code, font=font, fill="#c9d1d9")
        y += line_height
    image.save(destination, optimize=True)


code_image(
    ROOT / "minix/commands/schedexperiment/scheduler.c", 314, 349,
    OUT / "code-scheduling.png", "Requirement 1 — scheduler dispatch loop",
)
code_image(
    ROOT / "minix/commands/vmexperiment/simulator.c", 267, 292,
    OUT / "code-paging.png", "Requirement 2 — FIFO/LRU frame selection",
)
code_image(
    ROOT / "minix/fs/mfs/cache.c", 101, 130,
    OUT / "code-mfs-extents.png", "Requirement 3 — MFS extent preference",
)

print(f"Created report screenshots in {OUT}")
