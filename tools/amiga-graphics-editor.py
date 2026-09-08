#!/usr/bin/env python3
"""Small palette-locked editor for Harrier's Enhanced Amiga graphics."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from datetime import datetime
from uuid import uuid4
from pathlib import Path

from amiga_graphics_format import (
    ASSETS,
    AssetSpec,
    build_runtime_banks,
    load_and_validate,
    load_palette_words,
    project_palette_rgb,
    save_indexed_master,
    pixel_is_editable,
    ROOT,
    editor_palette_words,
    rgb12_to_rgb24,
)


def validate_and_pack(check_only: bool) -> str:
    banks = build_runtime_banks()
    changed = []
    for path, packed in banks.items():
        current = path.read_bytes() if path.is_file() else None
        if current == packed:
            continue
        if check_only:
            raise ValueError(f"Runtime bank is stale or missing: {path.name}")
        path.write_bytes(packed)
        changed.append(path.name)
    return "Packed " + ", ".join(changed) if changed else "Runtime banks are current"


def find_external_editor() -> str | None:
    candidates = (
        "libresprite.exe",
        "aseprite.exe",
        "grafx2.exe",
        r"C:\Program Files\LibreSprite\LibreSprite.exe",
        r"C:\Program Files\Aseprite\Aseprite.exe",
        r"C:\Program Files\GrafX2\grafx2.exe",
    )
    for candidate in candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
        path = Path(candidate)
        if path.is_file():
            return str(path)
    return None


class GraphicsEditor:
    def __init__(self) -> None:
        import tkinter as tk
        from tkinter import messagebox, ttk

        self.tk = tk
        self.messagebox = messagebox
        self.ttk = ttk
        self.root = tk.Tk()
        self.root.title("Harrier Attack Reloaded - Amiga graphics editor")
        self.root.minsize(820, 650)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        self.spec: AssetSpec | None = None
        self.pixels: list[int] = []
        self.saved_pixels: list[int] = []
        self.saved_source_bytes = None
        self.source_palette_words = load_palette_words()
        self.undo_stack: list[list[int]] = []
        self.redo_stack: list[list[int]] = []
        self.selected_pen = 10
        self.zoom = tk.IntVar(value=24)
        self.preview_zoom = tk.IntVar(value=4)
        self.preview_background = tk.StringVar(value="Sky")
        self.lighting = tk.StringVar(value="Day")
        self.original_browser = None
        self.status = tk.StringVar(value="Ready")
        self.external_editor = find_external_editor()
        self.palette_words = editor_palette_words(self.lighting.get())
        self.palette_rgb = [rgb12_to_rgb24(word) for word in self.palette_words]
        self.pixel_items: list[int] = []
        self.stroke_changed = False
        self.tool = tk.StringVar(value="paint")
        self.selection = None
        self.selection_anchor = None
        self.copy_buffer = None
        self.paste_pending = False
        self.paste_origin = None
        self.skip_transparent = tk.BooleanVar(value=False)

        self._build_ui()
        self.load_asset(ASSETS[0])

    @property
    def dirty(self) -> bool:
        return self.pixels != self.saved_pixels

    def _build_ui(self) -> None:
        tk, ttk = self.tk, self.ttk
        toolbar = ttk.Frame(self.root, padding=8)
        toolbar.pack(fill="x")
        ttk.Button(toolbar, text="Save + pack", command=self.save).pack(side="left")
        ttk.Button(toolbar, text="Revert", command=self.revert).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="Undo", command=self.undo).pack(side="left", padx=(14, 0))
        ttk.Button(toolbar, text="Redo", command=self.redo).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="Mirror H", command=lambda: self.mirror(True)).pack(side="left", padx=(14, 0))
        ttk.Button(toolbar, text="Mirror V", command=lambda: self.mirror(False)).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="Validate", command=self.validate).pack(side="left", padx=(14, 0))
        external_text = "Open in external editor" if self.external_editor else "External editor not found"
        self.external_button = ttk.Button(
            toolbar, text=external_text, command=self.open_external,
            state="normal" if self.external_editor else "disabled"
        )
        self.external_button.pack(side="left", padx=(14, 0))

        ttk.Label(toolbar, text="Zoom").pack(side="left", padx=(18, 4))
        zoom = ttk.Combobox(toolbar, textvariable=self.zoom, width=4,
                            values=(6, 8, 12, 16, 20, 24, 28, 32), state="readonly")
        zoom.pack(side="left")
        zoom.bind("<<ComboboxSelected>>", lambda _event: self.redraw())

        selection_bar = ttk.Frame(self.root, padding=(8, 0, 8, 6))
        selection_bar.pack(fill="x")
        ttk.Button(selection_bar, text="Original graphics…", command=self.open_original_graphics).pack(side="left", padx=(0, 10))
        for label, value in (("Paint", "paint"), ("Select rectangle", "select")):
            ttk.Radiobutton(selection_bar, text=label, variable=self.tool, value=value,
                            command=self.cancel_selection).pack(side="left", padx=(0, 8))
        ttk.Button(selection_bar, text="Copy (Ctrl+C)", command=self.copy_selection).pack(side="left")
        ttk.Button(selection_bar, text="Paste (Ctrl+V)", command=self.start_paste).pack(side="left", padx=6)
        ttk.Button(selection_bar, text="Cancel (Esc)", command=self.cancel_selection).pack(side="left")
        ttk.Checkbutton(selection_bar, text="Skip transparent pixels", variable=self.skip_transparent,
                        command=self.draw_selection).pack(side="left", padx=10)

        body = ttk.Panedwindow(self.root, orient="horizontal")
        body.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        sidebar = ttk.Frame(body, padding=8)
        editor = ttk.Frame(body, padding=8)
        body.add(sidebar, weight=1)
        body.add(editor, weight=4)

        ttk.Label(sidebar, text="Editable masters").pack(anchor="w")
        self.asset_list = tk.Listbox(sidebar, exportselection=False, height=min(12, len(ASSETS)), width=32)
        self.asset_list.pack(fill="x", pady=(5, 12))
        for asset in ASSETS:
            self.asset_list.insert("end", f"{asset.label}  ({asset.width}x{asset.height})")
        self.asset_list.bind("<<ListboxSelect>>", self.select_asset)
        self.asset_list.selection_set(0)

        instructions = (
            "Paint: left drag. Erase: right drag.\n"
            "Shift+click: fill. Ctrl+S: save + pack.\n"
            "Ctrl+Z / Ctrl+Y: undo / redo.\n\n"
            "Select rectangle: drag, then Ctrl+C.\n"
            "Ctrl+V, click to place top-left.\n"
            "Copy works across images here.\n"
            "Esc: cancel selection / paste.\n\n"
            "16 colours; pen 0 = transparent.\n"
            "Light checks = transparent pixels.\n"
            "Blue-grey cells = locked city space."
        )
        ttk.Label(sidebar, text=instructions, justify="left").pack(anchor="w")
        preview_toolbar = ttk.Frame(sidebar)
        preview_toolbar.pack(anchor="w", pady=(18, 4))
        ttk.Label(preview_toolbar, text="Preview zoom:").pack(side="left")
        preview_zoom = ttk.Combobox(
            preview_toolbar, textvariable=self.preview_zoom, width=3,
            values=(1, 2, 4, 8), state="readonly",
        )
        preview_zoom.pack(side="left", padx=(6, 2))
        ttk.Label(preview_toolbar, text="x").pack(side="left")
        preview_zoom.bind("<<ComboboxSelected>>", lambda _event: self.redraw_preview())
        preview_options = ttk.Frame(sidebar)
        preview_options.pack(anchor="w", pady=(0, 5))
        ttk.Label(preview_options, text="Background:").pack(side="left")
        background = ttk.Combobox(preview_options, textvariable=self.preview_background,
                                  values=("Sky", "White", "Checks"), width=8, state="readonly")
        background.pack(side="left", padx=6)
        background.bind("<<ComboboxSelected>>", lambda _event: self.redraw_preview())
        lighting_frame = ttk.Frame(sidebar)
        lighting_frame.pack(anchor="w", pady=(0, 5))
        ttk.Label(lighting_frame, text="Game lighting:").pack(side="left")
        lighting = ttk.Combobox(lighting_frame, textvariable=self.lighting,
                               values=("Day", "Dusk", "Night", "Dawn", "PNG"), width=8, state="readonly")
        lighting.pack(side="left", padx=6)
        lighting.bind("<<ComboboxSelected>>", self.change_lighting)
        self.preview = tk.Canvas(sidebar, width=72, height=72, background="#777777",
                                 highlightthickness=1, highlightbackground="#444444")
        self.preview.pack(anchor="w")

        canvas_frame = ttk.Frame(editor)
        canvas_frame.pack(fill="both", expand=True)
        canvas_frame.rowconfigure(0, weight=1)
        canvas_frame.columnconfigure(0, weight=1)
        self.canvas = tk.Canvas(canvas_frame, background="#252525", highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        horizontal = ttk.Scrollbar(canvas_frame, orient="horizontal", command=self.canvas.xview)
        vertical = ttk.Scrollbar(canvas_frame, orient="vertical", command=self.canvas.yview)
        horizontal.grid(row=1, column=0, sticky="ew")
        vertical.grid(row=0, column=1, sticky="ns")
        self.canvas.configure(xscrollcommand=horizontal.set, yscrollcommand=vertical.set)
        self.canvas.bind("<ButtonPress-1>", self.left_press)
        self.canvas.bind("<B1-Motion>", self.left_drag)
        self.canvas.bind("<ButtonRelease-1>", self.left_release)
        self.canvas.bind("<Motion>", self.paste_motion)
        self.canvas.bind("<Leave>", self.paste_leave)
        self.canvas.bind("<ButtonPress-3>", lambda event: self.begin_stroke(event, 0))
        self.canvas.bind("<B3-Motion>", lambda event: self.paint(event, 0))
        self.canvas.bind("<ButtonRelease-3>", self.end_stroke)

        palette_frame = ttk.Frame(editor)
        palette_frame.pack(fill="x", pady=(8, 0))
        ttk.Label(palette_frame, text="Game palette:").pack(side="left", padx=(0, 8))
        self.pen_buttons = []
        for index, (red, green, blue) in enumerate(self.palette_rgb):
            colour = f"#{red:02x}{green:02x}{blue:02x}"
            button = tk.Button(
                palette_frame, width=3, height=2, bg=colour,
                activebackground=colour, relief="sunken" if index == self.selected_pen else "raised",
                text=str(index), fg="white" if red + green + blue < 340 else "black",
                command=lambda pen=index: self.select_pen(pen),
            )
            button.pack(side="left", padx=1)
            self.pen_buttons.append(button)

        self.palette_detail = ttk.Label(editor)
        self.palette_detail.pack(anchor="w", pady=(6, 0))
        self.select_pen(self.selected_pen)
        ttk.Label(self.root, textvariable=self.status, relief="sunken", anchor="w",
                  padding=(6, 3)).pack(fill="x", side="bottom")

        self.root.bind("<Control-s>", lambda _event: self.save())
        self.root.bind("<Control-z>", lambda _event: self.undo())
        self.root.bind("<Control-y>", lambda _event: self.redo())
        self.root.bind("<Control-c>", self.copy_selection)
        self.root.bind("<Control-v>", self.start_paste)
        self.root.bind("<Escape>", self.cancel_selection)

    def change_lighting(self, _event=None):
        self.palette_words = editor_palette_words(self.lighting.get())
        self.palette_rgb = [rgb12_to_rgb24(word) for word in self.palette_words]
        for pen, button in enumerate(self.pen_buttons):
            r, g, b = self.palette_rgb[pen]
            colour = f"#{r:02x}{g:02x}{b:02x}"
            button.configure(bg=colour, activebackground=colour,
                             fg="white" if r + g + b < 340 else "black")
        self.select_pen(self.selected_pen)
        self.redraw()
        if self.original_browser and self.original_browser.window.winfo_exists():
            self.original_browser.redraw()
        self.status.set(f"{self.lighting.get()} colour preview only; pixel indices and PNG palette are unchanged")

    def open_original_graphics(self):
        from amiga_graphics_library import OriginalGraphicsBrowser
        if self.original_browser and self.original_browser.window.winfo_exists():
            self.original_browser.window.deiconify()
            self.original_browser.window.lift()
            return
        try:
            self.original_browser = OriginalGraphicsBrowser(self)
        except (OSError, ValueError) as error:
            self.messagebox.showerror("Original graphics", str(error))

    def cancel_selection(self, _event=None):
        self.selection = self.selection_anchor = self.paste_origin = None
        self.paste_pending = False
        self.draw_selection()
        return "break"

    def coordinates_at(self, event, clamp=False):
        if not self.spec:
            return None
        zoom = int(self.zoom.get())
        ox, oy = self.canvas_origin
        x = int((self.canvas.canvasx(event.x) - ox) // zoom)
        y = int((self.canvas.canvasy(event.y) - oy) // zoom)
        if clamp:
            return max(0, min(x, self.spec.width - 1)), max(0, min(y, self.spec.height - 1))
        return (x, y) if 0 <= x < self.spec.width and 0 <= y < self.spec.height else None

    def left_press(self, event):
        self.canvas.focus_set()
        point = self.coordinates_at(event)
        if self.paste_pending:
            if point:
                self.place_paste(*point)
        elif self.tool.get() == "select":
            self.selection_anchor = point
            self.selection = (*point, *point) if point else None
            self.draw_selection()
        else:
            self.begin_stroke(event, self.selected_pen)

    def left_drag(self, event):
        if self.paste_pending:
            self.paste_motion(event)
        elif self.tool.get() == "select":
            if self.selection_anchor:
                x, y = self.coordinates_at(event, clamp=True)
                ax, ay = self.selection_anchor
                self.selection = (min(x, ax), min(y, ay), max(x, ax), max(y, ay))
                self.draw_selection()
        else:
            self.paint(event, self.selected_pen)

    def left_release(self, event):
        if self.tool.get() == "select" or self.paste_pending:
            self.selection_anchor = None
        else:
            self.end_stroke(event)

    def copy_selection(self, _event=None):
        if not self.selection or not self.spec:
            self.status.set("Choose Select rectangle and drag an area before copying")
            return "break"
        x0, y0, x1, y1 = self.selection
        self.copy_buffer = (x1 - x0 + 1, y1 - y0 + 1,
                            [self.pixels[y * self.spec.width + x]
                             for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)])
        self.status.set(f"Copied {x1 - x0 + 1}x{y1 - y0 + 1}. Select any image, then Paste")
        return "break"

    def start_paste(self, _event=None):
        if not self.copy_buffer:
            self.status.set("Copy a rectangle first")
            return "break"
        self.tool.set("select")
        self.paste_pending = True
        self.selection_anchor = self.paste_origin = None
        self.status.set("Click the destination top-left corner to paste. Esc cancels")
        self.draw_selection()
        return "break"

    def paste_motion(self, event):
        if self.paste_pending:
            self.paste_origin = self.coordinates_at(event)
            self.draw_selection()

    def paste_leave(self, _event=None):
        self.paste_origin = None
        self.draw_selection()

    def paste_error(self, x, y):
        width, height, pixels = self.copy_buffer
        if x < 0 or y < 0 or x + width > self.spec.width or y + height > self.spec.height:
            return "Paste must fit inside the image; choose another position or copy a smaller area"
        if any(pen and not pixel_is_editable(self.spec, x + i % width, y + i // width)
               for i, pen in enumerate(pixels)):
            return "Paste overlaps locked city cells; choose another position"
        return None

    def place_paste(self, x, y):
        error = self.paste_error(x, y)
        if error:
            self.status.set(error)
            return
        width, height, pixels = self.copy_buffer
        updated = self.pixels.copy()
        for i, pen in enumerate(pixels):
            if not pen and self.skip_transparent.get():
                continue
            px, py = x + i % width, y + i // width
            if pixel_is_editable(self.spec, px, py):
                updated[py * self.spec.width + px] = pen
        if updated != self.pixels:
            self.undo_stack.append(self.pixels.copy())
            self.undo_stack = self.undo_stack[-100:]
            self.redo_stack.clear()
            self.pixels = updated
        self.paste_pending = False
        self.paste_origin = None
        self.selection = (x, y, x + width - 1, y + height - 1)
        self.redraw()
        self.status.set(f"Pasted {width}x{height}; Ctrl+Z undoes the paste")

    def draw_selection(self):
        self.canvas.delete("selection")
        if not self.spec:
            return
        bounds = self.selection
        colour = "#00ffff"
        if self.paste_pending and self.paste_origin:
            x, y = self.paste_origin
            width, height, _ = self.copy_buffer
            bounds = (x, y, x + width - 1, y + height - 1)
            colour = "#ff5555" if self.paste_error(x, y) else "#00ffff"
            if not self.paste_error(x, y):
                ox, oy = self.canvas_origin
                z = int(self.zoom.get())
                for i, pen in enumerate(self.copy_buffer[2]):
                    if pen == 0 and self.skip_transparent.get():
                        continue
                    px, py = x + i % width, y + i // width
                    if not pixel_is_editable(self.spec, px, py):
                        continue
                    rgb = self.palette_rgb[pen]
                    fill = f"#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}" if pen else (
                        "#d8e0e8" if (px + py) % 2 else "#ffffff")
                    self.canvas.create_rectangle(
                        ox + px * z, oy + py * z, ox + (px + 1) * z, oy + (py + 1) * z,
                        fill=fill, outline="#777777", tags="selection")
        if bounds:
            x0, y0, x1, y1 = bounds
            ox, oy = self.canvas_origin
            z = int(self.zoom.get())
            coords = (ox + x0 * z, oy + y0 * z, ox + (x1 + 1) * z, oy + (y1 + 1) * z)
            self.canvas.create_rectangle(*coords, outline="#000000", width=4, tags="selection")
            self.canvas.create_rectangle(*coords, outline=colour, width=2, dash=(5, 3), tags="selection")

    def select_pen(self, pen: int) -> None:
        self.selected_pen = pen
        for index, button in enumerate(self.pen_buttons):
            button.configure(relief="sunken" if index == pen else "raised")
        rgb = self.palette_rgb[pen]
        self.palette_detail.configure(
            text=f"Pen {pen}: Amiga ${self.palette_words[pen]:03X}  "
                 f"RGB #{rgb[0]:02X}{rgb[1]:02X}{rgb[2]:02X}"
                 + ("  (transparent)" if pen == 0 else "")
                 + ("  (changes with mission lighting)" if pen in (5, 15) else "")
                 + ("  (fixed powerup colour in game)" if pen == 14 else "")
        )

    def confirm_discard(self) -> bool:
        if not self.dirty:
            return True
        answer = self.messagebox.askyesnocancel(
            "Unsaved graphics", "Save the current asset before continuing?"
        )
        if answer is None:
            return False
        if answer:
            return self.save()
        return True

    def select_asset(self, _event=None) -> None:
        selection = self.asset_list.curselection()
        if not selection:
            return
        new_spec = ASSETS[selection[0]]
        if self.spec == new_spec:
            return
        if not self.confirm_discard():
            current = ASSETS.index(self.spec) if self.spec else 0
            self.asset_list.selection_clear(0, "end")
            self.asset_list.selection_set(current)
            return
        self.load_asset(new_spec)

    def load_asset(self, spec: AssetSpec) -> None:
        try:
            source_bytes = spec.path.read_bytes()
            image = load_and_validate(spec)
            if spec.path.read_bytes() != source_bytes:
                raise ValueError("Image changed while loading; try again")
        except (OSError, ValueError) as error:
            self.messagebox.showerror("Cannot load asset", str(error))
            return
        self.spec = spec
        self.selection = self.selection_anchor = self.paste_origin = None
        self.paste_pending = False
        if spec.width > 16 or spec.height > 16:
            self.zoom.set(12)
        self.canvas.xview_moveto(0)
        self.canvas.yview_moveto(0)
        self.pixels = [int(pixel) for pixel in image.getdata()]
        self.saved_pixels = self.pixels.copy()
        self.saved_source_bytes = source_bytes
        self.undo_stack.clear()
        self.redo_stack.clear()
        index = ASSETS.index(spec)
        self.asset_list.selection_clear(0, "end")
        self.asset_list.selection_set(index)
        self.status.set(f"Loaded {spec.filename}")
        self.redraw()

    def redraw(self) -> None:
        if not self.spec:
            return
        zoom = int(self.zoom.get())
        width, height = self.spec.width, self.spec.height
        self.canvas.delete("all")
        self.pixel_items = []
        canvas_width, canvas_height = self.canvas.winfo_width(), self.canvas.winfo_height()
        origin_x = max(10, (canvas_width - width * zoom) // 2)
        origin_y = max(10, (canvas_height - height * zoom) // 2)
        self.canvas_origin = (origin_x, origin_y)
        self.canvas.configure(scrollregion=(0, 0, width * zoom + 20, height * zoom + 20))
        for y in range(height):
            for x in range(width):
                pen = self.pixels[y * width + x]
                colour = self.palette_rgb[pen]
                fill = f"#{colour[0]:02x}{colour[1]:02x}{colour[2]:02x}"
                if pen == 0:
                    fill = "#d8e0e8" if (x + y) % 2 else "#ffffff"
                if not pixel_is_editable(self.spec, x, y):
                    fill = "#9aaec2"
                item = self.canvas.create_rectangle(
                    origin_x + x * zoom, origin_y + y * zoom,
                    origin_x + (x + 1) * zoom, origin_y + (y + 1) * zoom,
                    fill=fill, outline="#777777", width=1,
                )
                self.pixel_items.append(item)
        for x in range(0, width + 1, 8):
            self.canvas.create_line(origin_x + x * zoom, origin_y,
                                    origin_x + x * zoom, origin_y + height * zoom,
                                    fill="#b0b0b0", width=2)
        for y in range(0, height + 1, 8):
            self.canvas.create_line(origin_x, origin_y + y * zoom,
                                    origin_x + width * zoom, origin_y + y * zoom,
                                    fill="#b0b0b0", width=2)
        self.redraw_preview()
        self.draw_selection()

    def redraw_preview(self) -> None:
        if not self.spec:
            return
        self.preview.delete("all")
        background = self.preview_background.get()
        r, g, b = self.palette_rgb[0]
        backdrop = f"#{r:02x}{g:02x}{b:02x}" if background == "Sky" else "#ffffff"
        self.preview.configure(background=backdrop)
        scale = int(self.preview_zoom.get())
        preview_width = max(72, self.spec.width * scale + 24)
        preview_height = max(72, self.spec.height * scale + 24)
        self.preview.configure(width=preview_width, height=preview_height)
        origin_x = (preview_width - self.spec.width * scale) // 2
        origin_y = (preview_height - self.spec.height * scale) // 2
        for y in range(self.spec.height):
            for x in range(self.spec.width):
                pen = self.pixels[y * self.spec.width + x]
                if pen == 0:
                    if background != "Checks":
                        continue
                    colour = "#d8e0e8" if (x + y) % 2 else "#ffffff"
                else:
                    red, green, blue = self.palette_rgb[pen]
                    colour = f"#{red:02x}{green:02x}{blue:02x}"
                self.preview.create_rectangle(
                    origin_x + x * scale, origin_y + y * scale,
                    origin_x + (x + 1) * scale, origin_y + (y + 1) * scale,
                    fill=colour, outline="",
                )

    def pixel_at(self, event) -> int | None:
        if not self.spec:
            return None
        zoom = int(self.zoom.get())
        origin_x, origin_y = self.canvas_origin
        x = int((self.canvas.canvasx(event.x) - origin_x) // zoom)
        y = int((self.canvas.canvasy(event.y) - origin_y) // zoom)
        if 0 <= x < self.spec.width and 0 <= y < self.spec.height:
            if pixel_is_editable(self.spec, x, y):
                return y * self.spec.width + x
        return None

    def begin_stroke(self, event, pen: int) -> None:
        self.undo_stack.append(self.pixels.copy())
        self.undo_stack = self.undo_stack[-100:]
        self.redo_stack.clear()
        self.stroke_changed = False
        index = self.pixel_at(event)
        if index is not None and event.state & 0x0001:
            self.flood_fill(index, pen)
        else:
            self.paint(event, pen)

    def paint(self, event, pen: int) -> None:
        index = self.pixel_at(event)
        if index is None or self.pixels[index] == pen:
            return
        self.pixels[index] = pen
        self.stroke_changed = True
        self.redraw()
        self.status.set(f"Editing {self.spec.filename} *")

    def end_stroke(self, _event=None) -> None:
        if not self.stroke_changed and self.undo_stack:
            self.undo_stack.pop()

    def flood_fill(self, start: int, pen: int) -> None:
        if not self.spec:
            return
        source = self.pixels[start]
        if source == pen:
            return
        width, height = self.spec.width, self.spec.height
        pending = [start]
        seen = {start}
        while pending:
            index = pending.pop()
            if not pixel_is_editable(self.spec, index % width, index // width):
                continue
            if self.pixels[index] != source:
                continue
            self.pixels[index] = pen
            x, y = index % width, index // width
            for neighbour_x, neighbour_y in ((x - 1, y), (x + 1, y),
                                               (x, y - 1), (x, y + 1)):
                if 0 <= neighbour_x < width and 0 <= neighbour_y < height:
                    neighbour = neighbour_y * width + neighbour_x
                    if neighbour not in seen:
                        seen.add(neighbour)
                        pending.append(neighbour)
        self.stroke_changed = True
        self.redraw()
        self.status.set(f"Editing {self.spec.filename} *")

    def mirror(self, horizontal: bool) -> None:
        if not self.spec:
            return
        if self.spec.key.startswith("town_"):
            self.status.set("City blocks keep their gameplay layout; mirror is disabled")
            return
        self.undo_stack.append(self.pixels.copy())
        self.undo_stack = self.undo_stack[-100:]
        self.redo_stack.clear()
        width, height = self.spec.width, self.spec.height
        rows = [self.pixels[y * width : (y + 1) * width] for y in range(height)]
        if horizontal:
            rows = [list(reversed(row)) for row in rows]
        else:
            rows = list(reversed(rows))
        self.pixels = [pixel for row in rows for pixel in row]
        self.redraw()
        self.status.set(f"Mirrored {'horizontally' if horizontal else 'vertically'} *")

    def undo(self) -> None:
        if not self.undo_stack:
            return
        self.redo_stack.append(self.pixels.copy())
        self.pixels = self.undo_stack.pop()
        self.redraw()
        self.status.set("Undo")

    def redo(self) -> None:
        if not self.redo_stack:
            return
        self.undo_stack.append(self.pixels.copy())
        self.pixels = self.redo_stack.pop()
        self.redraw()
        self.status.set("Redo")

    def save(self) -> bool:
        if not self.spec:
            return False
        if self.paste_pending:
            self.status.set("Click to place the pasted preview, or press Esc to cancel it, before saving")
            return False
        try:
            if load_palette_words() != self.source_palette_words:
                raise ValueError("The project palette changed on disk. Keep this window open and restart the editor before saving.")
            current = self.spec.path.read_bytes() if self.spec.path.exists() else None
            history = ROOT / ".tmp/graphics-history"
            history.mkdir(parents=True, exist_ok=True)
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + uuid4().hex[:8]
            if current != self.saved_source_bytes:
                draft = history / f"{self.spec.key}-{stamp}-unsaved.png"
                save_indexed_master(draft, (self.spec.width, self.spec.height), self.pixels)
                raise ValueError(
                    "This image changed on disk after it was opened, possibly in another editor window. "
                    "Nothing was overwritten. Your current work remains in this window and was copied to:\n"
                    f"{draft}\nUse Revert to load the disk version, or copy your changes first."
                )
            if current is not None:
                (history / f"{self.spec.key}-{stamp}-before.png").write_bytes(current)
            save_indexed_master(
                self.spec.path, (self.spec.width, self.spec.height), self.pixels
            )
            self.saved_source_bytes = self.spec.path.read_bytes()
            result = validate_and_pack(check_only=False)
            self.saved_pixels = self.pixels.copy()
            self.status.set(f"Saved {self.spec.filename}. {result}")
            return True
        except (OSError, ValueError) as error:
            self.messagebox.showerror("Save failed", str(error))
            return False

    def revert(self) -> None:
        if self.spec and self.confirm_discard():
            self.load_asset(self.spec)

    def validate(self) -> None:
        try:
            result = validate_and_pack(check_only=True)
            self.status.set(f"Validation passed. {result}")
            self.messagebox.showinfo("Enhanced graphics", "All PNG masters and runtime banks are valid.")
        except (OSError, ValueError) as error:
            self.messagebox.showerror("Validation failed", str(error))

    def open_external(self) -> None:
        if not self.external_editor or not self.spec:
            return
        if self.dirty and not self.save():
            return
        try:
            subprocess.Popen([self.external_editor, str(self.spec.path)])
            self.status.set(
                "Opened externally. Save there, then use Revert here to reload and Validate."
            )
        except OSError as error:
            self.messagebox.showerror("External editor", str(error))

    def close(self) -> None:
        if self.confirm_discard():
            self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="validate PNG masters and packed banks without opening the GUI")
    parser.add_argument("--asset", choices=[spec.key for spec in ASSETS],
                        help="select an asset when opening the editor")
    parser.add_argument("--originals", action="store_true",
                        help="also open the read-only original graphics library")
    args = parser.parse_args()
    if args.check:
        try:
            print(validate_and_pack(check_only=True))
            return 0
        except (OSError, ValueError) as error:
            print(f"Enhanced graphics error: {error}", file=sys.stderr)
            return 1
    editor = GraphicsEditor()
    if args.asset:
        editor.load_asset(next(spec for spec in ASSETS if spec.key == args.asset))
    if args.originals:
        editor.open_original_graphics()
    editor.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
