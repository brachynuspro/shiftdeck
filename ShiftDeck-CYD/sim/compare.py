"""PPM -> PNG, plus side-by-side sheets (mockup | simulator) at 2x."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
IMGS = ROOT.parent.parent / "IMGS"
OUT = ROOT / "out"
PAIRS = {"1_boot": "inicializacao.png", "2_home": "principal.png", "3_playlists": "Playlists.png",
         "4d_search_results": "busca.png", "5a_settings": "config.png"}

def screen_of(path):
    """Crop the display area (the big dark rectangle) out of a mockup photo."""
    im = Image.open(path).convert("L")
    w, h = im.size
    px = im.load()
    dark_cols = [x for x in range(w) if sum(px[x, y] < 60 for y in range(0, h, 4)) > h / 4 * 0.45]
    dark_rows = [y for y in range(h) if sum(px[x, y] < 60 for x in range(0, w, 4)) > w / 4 * 0.4]
    box = (min(dark_cols) + 8, min(dark_rows) + 8, max(dark_cols) - 8, max(dark_rows) - 8)
    return Image.open(path).convert("RGB").crop(box).resize((640, 480), Image.LANCZOS)

for ppm in sorted(OUT.glob("*.ppm")):
    png = ppm.with_suffix(".png")
    Image.open(ppm).save(png)
    ppm.unlink()
for name, mock in PAIRS.items():
    sim = Image.open(OUT / f"{name}.png").resize((640, 480), Image.NEAREST)
    sheet = Image.new("RGB", (1300, 480), (40, 40, 40))
    sheet.paste(screen_of(IMGS / mock), (0, 0))
    sheet.paste(sim, (660, 0))
    sheet.save(OUT / f"compare_{name}.png")
print("sheets in", OUT)
