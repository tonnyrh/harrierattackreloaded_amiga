"""Read-only browser for the checked-in original Amiga tile graphics."""

import re

from PIL import Image, ImageTk

from amiga_graphics_format import ROOT, TOWN_LAYOUTS, pillow_palette


def indexed_image(size, pixels):
    image = Image.new("P", size)
    image.putpalette(pillow_palette())
    image.putdata(pixels)
    image.info["transparency"] = 0
    return image


def load_original_graphics():
    raw = (ROOT / "amiga/assets/game_tiles.bpl").read_bytes()
    if len(raw) != 102 * 40:
        raise ValueError("Expected 102 original five-plane 8x8 tiles")
    tiles = []
    for tile in range(102):
        pixels = [sum(((raw[tile * 40 + y * 5 + p] >> (7 - x)) & 1) << p
                      for p in range(5)) for y in range(8) for x in range(8)]
        if max(pixels) > 15:
            raise ValueError(f"Original tile {tile} exceeds the editor's 16-colour palette")
        tiles.append(indexed_image((8, 8), pixels))
    atlas = indexed_image((128, 56), [0] * (128 * 56))
    for i, tile in enumerate(tiles):
        atlas.paste(tile, ((i % 16) * 8, (i // 16) * 8))
    graphics = {"All 102 original tiles (0–101)": atlas}
    for i, tile in enumerate(tiles):
        graphics[f"Tile {i:03d}"] = tile
    for i, layout in enumerate(TOWN_LAYOUTS):
        width = len(layout) // 3 * 8
        block = indexed_image((width, 24), [0] * (width * 24))
        for cell, tile in enumerate(layout):
            if tile not in (0, 1):
                block.paste(tiles[tile], (cell // 3 * 8, cell % 3 * 8))
        graphics[f"Original city block {i}"] = block
    # These converted CPC+ ships are stored separately from the 102 tiles.
    source = (ROOT / "amiga/assets/promoted_sprite_tiles.h").read_text()
    for label, name, wide, tall in (("Original carrier", "harCarrierTileData", 12, 3),
                                     ("Original gunship", "harGunshipTileData", 4, 2)):
        match = re.search(rf"{name}\[\d+\] = \{{(.*?)\}};", source, re.S)
        if not match:
            raise ValueError(f"Missing original graphic: {name}")
        data = list(map(int, re.findall(r"\d+", match[1])))
        if len(data) != wide * tall * 40:
            raise ValueError(f"Invalid original graphic size: {name}")
        pixels = [0] * (wide * 8 * tall * 8)
        for cell in range(wide * tall):
            for y in range(8):
                for x in range(8):
                    row = cell * 40 + y * 5
                    pen = sum(((data[row + p] >> (7 - x)) & 1) << p for p in range(4))
                    if not data[row + 4] & (0x80 >> x):
                        pen = 0
                    px, py = cell % wide * 8 + x, cell // wide * 8 + y
                    pixels[py * wide * 8 + px] = pen
        graphics[label] = indexed_image((wide * 8, tall * 8), pixels)
    return graphics


class OriginalGraphicsBrowser:
    def __init__(self, editor):
        self.editor = editor
        tk, ttk = editor.tk, editor.ttk
        self.graphics = load_original_graphics()
        self.names = list(self.graphics)
        self.window = tk.Toplevel(editor.root)
        self.window.title("Original graphics — read-only copy library")
        self.window.geometry("1000x700")
        self.choice = tk.StringVar(value=self.names[0])
        self.zoom = tk.IntVar(value=6)
        self.selection = self.anchor = None
        toolbar = ttk.Frame(self.window, padding=8)
        toolbar.pack(fill="x")
        chooser = ttk.Combobox(toolbar, textvariable=self.choice, values=self.names,
                               state="readonly", width=38)
        chooser.pack(side="left")
        chooser.bind("<<ComboboxSelected>>", self.change_graphic)
        for label, step in (("Previous", -1), ("Next", 1)):
            ttk.Button(toolbar, text=label, command=lambda step=step: self.step(step)).pack(side="left", padx=3)
        ttk.Label(toolbar, text="Zoom").pack(side="left", padx=(10, 3))
        zoom = ttk.Combobox(toolbar, textvariable=self.zoom, values=(2, 4, 6, 8, 12, 16),
                            state="readonly", width=3)
        zoom.pack(side="left")
        zoom.bind("<<ComboboxSelected>>", lambda _event: self.redraw())
        ttk.Button(toolbar, text="Copy selection", command=self.copy).pack(side="left", padx=8)
        ttk.Button(toolbar, text="Select all", command=self.select_all).pack(side="left")
        self.status = tk.StringVar(value="Drag a rectangle, Ctrl+C. Return to the editor and Ctrl+V to paste.")
        ttk.Label(self.window, textvariable=self.status, padding=8).pack(fill="x", side="bottom")
        frame = ttk.Frame(self.window)
        frame.pack(fill="both", expand=True)
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        self.canvas = tk.Canvas(frame, background="#77aaff", highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        h = ttk.Scrollbar(frame, orient="horizontal", command=self.canvas.xview)
        v = ttk.Scrollbar(frame, orient="vertical", command=self.canvas.yview)
        h.grid(row=1, column=0, sticky="ew")
        v.grid(row=0, column=1, sticky="ns")
        self.canvas.configure(xscrollcommand=h.set, yscrollcommand=v.set)
        self.canvas.bind("<ButtonPress-1>", self.begin)
        self.canvas.bind("<Double-Button-1>", self.select_tile)
        self.canvas.bind("<B1-Motion>", self.drag)
        self.canvas.bind("<ButtonRelease-1>", lambda _event: setattr(self, "anchor", None))
        self.window.bind("<Control-c>", self.copy)
        self.window.bind("<Control-a>", self.select_all)
        self.redraw()

    @property
    def image(self):
        return self.graphics[self.choice.get()]

    def step(self, delta):
        self.choice.set(self.names[(self.names.index(self.choice.get()) + delta) % len(self.names)])
        self.change_graphic()

    def change_graphic(self, _event=None):
        self.selection = self.anchor = None
        self.canvas.xview_moveto(0)
        self.canvas.yview_moveto(0)
        self.redraw()

    def redraw(self):
        self.canvas.delete("all")
        z = self.zoom.get()
        display = self.image.copy()
        palette = [channel for rgb in self.editor.palette_rgb for channel in rgb]
        display.putpalette(palette + [0] * (768 - len(palette)))
        r, g, b = self.editor.palette_rgb[0]
        self.canvas.configure(background=f"#{r:02x}{g:02x}{b:02x}")
        self.photo = ImageTk.PhotoImage(display.convert("RGBA").resize(
            (self.image.width * z, self.image.height * z), Image.Resampling.NEAREST), master=self.window)
        self.canvas.create_image(8, 8, anchor="nw", image=self.photo)
        self.canvas.configure(scrollregion=(0, 0, self.image.width * z + 16, self.image.height * z + 16))
        for x in range(0, self.image.width + 1, 8):
            self.canvas.create_line(8 + x*z, 8, 8 + x*z, 8 + self.image.height*z, fill="#b0cce5")
        for y in range(0, self.image.height + 1, 8):
            self.canvas.create_line(8, 8 + y*z, 8 + self.image.width*z, 8 + y*z, fill="#b0cce5")
        self.draw_selection()

    def point(self, event):
        z = self.zoom.get()
        return (max(0, min(self.image.width - 1, int((self.canvas.canvasx(event.x) - 8) // z))),
                max(0, min(self.image.height - 1, int((self.canvas.canvasy(event.y) - 8) // z))))

    def begin(self, event):
        self.canvas.focus_set()
        self.anchor = self.point(event)
        self.selection = (*self.anchor, *self.anchor)
        self.draw_selection()

    def drag(self, event):
        if self.anchor:
            x, y = self.point(event)
            ax, ay = self.anchor
            self.selection = (min(x, ax), min(y, ay), max(x, ax), max(y, ay))
            self.draw_selection()

    def select_tile(self, event):
        if self.choice.get() != self.names[0]:
            return self.select_all()
        x, y = self.point(event)
        x, y = x // 8 * 8, y // 8 * 8
        if (y // 8) * 16 + x // 8 < 102:
            self.selection = (x, y, x + 7, y + 7)
            self.anchor = None
            self.draw_selection()
        return "break"

    def draw_selection(self):
        self.canvas.delete("selection")
        if self.selection:
            x0, y0, x1, y1 = self.selection
            z = self.zoom.get()
            self.canvas.create_rectangle(8+x0*z, 8+y0*z, 8+(x1+1)*z, 8+(y1+1)*z,
                                         outline="black", width=3, tags="selection")
            self.canvas.create_rectangle(8+x0*z, 8+y0*z, 8+(x1+1)*z, 8+(y1+1)*z,
                                         outline="white", dash=(4, 3), tags="selection")
            tile = (y0 // 8) * 16 + x0 // 8
            detail = f"; starts at tile {tile}" if self.choice.get() == self.names[0] and tile < 102 else ""
            self.status.set(f"Selected {x1-x0+1}x{y1-y0+1}{detail}. Ctrl+C to copy to editor.")

    def select_all(self, _event=None):
        self.selection = (0, 0, self.image.width-1, self.image.height-1)
        self.draw_selection()
        return "break"

    def copy(self, _event=None):
        if not self.selection:
            self.status.set("Drag a rectangle or choose Select all first")
            return "break"
        x0, y0, x1, y1 = self.selection
        cropped = self.image.crop((x0, y0, x1+1, y1+1))
        self.editor.copy_buffer = (cropped.width, cropped.height, list(cropped.getdata()))
        message = f"Copied {cropped.width}x{cropped.height} from {self.choice.get()}. Paste in the editor with Ctrl+V."
        self.status.set(message)
        self.editor.status.set(message)
        return "break"
