"""Cuts cover/thumbnail stand-ins out of the IMGS/ mockups as raw RGB565."""
import struct
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
IMGS = ROOT.parent.parent / "IMGS"
OUT = ROOT / "assets"
CROPS = {
    "cover":     ("principal.png", (346, 323, 605, 582), 300),
    "pl_rap":    ("Playlists.png", (362, 390, 448, 470), 64),
    "pl_treino": ("Playlists.png", (362, 490, 448, 572), 64),
    "pl_desc":   ("Playlists.png", (362, 592, 448, 672), 64),
    "pl_chill":  ("Playlists.png", (362, 695, 448, 775), 64),
    "art_xama":  ("busca.png",     (357, 467, 452, 562), 160),
}
OUT.mkdir(exist_ok=True)
for name, (src, box, size) in CROPS.items():
    im = Image.open(IMGS / src).convert("RGB").crop(box).resize((size, size), Image.LANCZOS)
    data = bytearray(struct.pack("<HH", size, size))
    for r, g, b in im.getdata():
        data += struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    (OUT / f"{name}.rgb").write_bytes(bytes(data))
