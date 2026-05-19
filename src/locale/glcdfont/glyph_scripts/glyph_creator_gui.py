#!/usr/bin/env python3
# Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
# SPDX-License-Identifier: GPL-3.0-only

"""
Simple 5x7 glyph creator/editor GUI for Adafruit_GFX glcdfont files.
Features:
- Load a glcdfont C file and parse the 256*5 bytes array
- Edit a single glyph as a 5x7 grid (click to toggle)
- Show current bytes in hex and binary
- Save edited glyph back into the glcdfont file (in-place)
- Export glyph bytes to clipboard or file

Usage: python scripts/glyph_creator.py
"""

import re
try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    TK_AVAILABLE = True
except Exception:
    TK_AVAILABLE = False
    # Create a minimal dummy `tk` fallback so class can be defined in non-GUI (CLI) environments
    import types
    class _DummyIntVar:
        def __init__(self, value=0): self._v = value
        def set(self, v): self._v = v
        def get(self): return self._v
    class _DummyTkBase: pass
    tk = types.SimpleNamespace(Tk=_DummyTkBase, IntVar=_DummyIntVar)
from pathlib import Path
from datetime import datetime

FONT_ARRAY_RE = re.compile(r"static\s+const\s+unsigned\s+char\s+font\[\]\s+PROGMEM\s*=\s*\{([\s\S]*?)\};")
HEX_RE = re.compile(r"0x[0-9A-Fa-f]{1,2}")

DEFAULT_FONT = Path(__file__).parent.parent / 'glcdfont_Latin.c'

# Lithuanian mapping: character -> font index used in this project's glcdfont_LT
LT_UTF_MAP = {
    'Ą':0xB7,'ą':0xB8,'Č':0xC9,'č':0xCA,'Ė':0xB9,'ė':0xBA,
    'Į':0xD7,'į':0xFE,'Š':0xC2,'š':0xC3,'Ų':0xD6,'ų':0xFF,
    'Ū':0xE4,'ū':0xE6,'Ž':0xC7,'ž':0xC8
}
# reverse map: index -> character (for previewing the current glyph)
INDEX_TO_CHAR = {v: k for k, v in LT_UTF_MAP.items()}

# ── palette + theme ─────────────────────────────────────────────────────────

BG          = '#f0f0f0'
SEC         = '#ffffff'
BORDER      = '#c0c0c0'
FG          = '#1a1a1a'
FG_DIM      = '#555555'
ACCENT      = '#1565c0'
GRID_LINE   = '#999999'
PIXEL_ON    = '#000000'

HEX_FONT    = ('Courier New', 10)
INFO_FONT   = ('Segoe UI', 9)
HEADER_FONT = ('Segoe UI', 10, 'bold')
STATUS_FONT = ('Segoe UI', 9)

def apply_theme(root):
    if not TK_AVAILABLE:
        return
    style = ttk.Style(root)
    style.theme_use('clam')
    style.configure('.',                 background=BG,       foreground=FG,    font=INFO_FONT)
    style.configure('TFrame',            background=BG)
    style.configure('TLabelframe',       background=BG,       foreground=FG)
    style.configure('TLabelframe.Label', background=BG,       foreground=ACCENT, font=HEADER_FONT)
    style.configure('TLabel',            background=BG,       foreground=FG)
    style.configure('TButton',           background='#e0e0e0', foreground=FG, relief='flat', padding=(8, 4))
    style.map('TButton',
              background=[('active', ACCENT), ('pressed', '#0d47a1')],
              foreground=[('active', SEC),    ('pressed', SEC)])
    style.configure('TEntry',     fieldbackground=SEC, foreground=FG)
    style.configure('TSpinbox',   fieldbackground=SEC, foreground=FG, arrowsize=14)
    style.configure('TScrollbar', background='#d0d0d0', troughcolor=BG, arrowsize=14)
    style.configure('TSeparator', background=BORDER)


def index_to_char(idx: int) -> str:
    """Return the corresponding character for a font index or '?' if unknown."""
    if idx < 0:
        return '?'
    if idx < 128:
        try:
            return chr(idx)
        except Exception:
            return '?'
    return INDEX_TO_CHAR.get(idx, '?')

# helpers to safely get/set the 'current_index' whether it's a tk.IntVar or a plain int
def _get_index(editor) -> int:
    ci = getattr(editor, 'current_index', 0)
    if hasattr(ci, 'get'):
        return int(ci.get())
    return int(ci)

def _set_index(editor, val: int):
    ci = getattr(editor, 'current_index', None)
    if hasattr(ci, 'set'):
        ci.set(int(val))
    else:
        editor.current_index = int(val)


def _save_font_file(fp: Path, vals):
    """Save updated font bytes to file with minimal in-place token replacements.

    Returns Path to backup file if written, or None if no changes required.
    """
    txt = fp.read_text(encoding='utf-8')
    m = FONT_ARRAY_RE.search(txt)
    if not m:
        raise ValueError('font array not found')
    block_start = m.start(1)
    block_end = m.end(1)
    block = txt[block_start:block_end]

    # Find hex tokens while skipping /* */ and // comments
    token_matches = []  # list of (start, end, text)
    i = 0
    L = len(block)
    while i < L:
        if block.startswith('/*', i):
            j = block.find('*/', i+2)
            if j == -1:
                break
            i = j + 2
            continue
        if block.startswith('//', i):
            j = block.find('\n', i+2)
            if j == -1:
                break
            i = j + 1
            continue
        mhex = re.match(r'0x[0-9A-Fa-f]{1,2}', block[i:])
        if mhex:
            s = mhex.group(0)
            start = i
            end = i + len(s)
            token_matches.append((start, end, s))
            i = end
            continue
        i += 1

    orig_vals = [int(t[2], 16) for t in token_matches] if token_matches else []
    if len(orig_vals) < 256 * 5:
        orig_vals = (orig_vals + [0] * (256 * 5))[:256 * 5]
    if len(vals) < 256 * 5:
        vals = (vals + [0] * (256 * 5))[:256 * 5]

    changed = [i for i in range(256 * 5) if i < len(orig_vals) and vals[i] != orig_vals[i]]
    if not changed:
        return None

    block2 = block
    for idx in sorted(changed, reverse=True):
        if idx >= len(token_matches):
            continue
        start, end, orig_text = token_matches[idx]
        new_val = vals[idx]
        hex_digits = orig_text[2:]
        width = len(hex_digits)
        has_lower = any(c.islower() for c in hex_digits if c.isalpha())
        fmt = 'x' if has_lower else 'X'
        if width == 1:
            new_hex = format(new_val, fmt)
            new_text = '0x' + new_hex
        else:
            new_text = '0x' + format(new_val, '02' + fmt)
        block2 = block2[:start] + new_text + block2[end:]

    newtxt = txt[:block_start] + block2 + txt[block_end:]

    bak = fp.with_suffix(fp.suffix + '.bak.' + datetime.utcnow().strftime('%Y%m%d%H%M%S'))
    Path(fp).rename(bak)
    fp.write_text(newtxt, encoding='utf-8')
    return bak

if TK_AVAILABLE:
    class GlyphEditor(tk.Tk):
        def __init__(self):
            super().__init__()
            self.title('5x7 Glyph Creator')
            self.configure(bg=BG)
            apply_theme(self)
            self.font_path = None
            self.font_bytes = None  # list of ints, length 256*5
            self.current_index = tk.IntVar(value=0)
            self.char_var = tk.StringVar(value='')
            self.char_big_var = tk.StringVar(value='')
            # internal glyph clipboard (list of 5 ints)
            self._clipboard_glyph = None
            self.cell_size = 24
            self._build_ui()

            # if default font exists, load it
            if DEFAULT_FONT.exists():
                try:
                    self._load_file(DEFAULT_FONT)
                except Exception:
                    pass

            self.on_index_change()

        def _build_ui(self):
            frm = ttk.Frame(self, padding=8)
            frm.pack(fill='both', expand=True)

            # ── Font File ─────────────────────────────────────────────────────────
            file_lf = ttk.LabelFrame(frm, text='Font File', padding=(8, 4))
            file_lf.pack(fill='x', pady=(0, 6))
            ttk.Button(file_lf, text='Load font...',    command=self.load_font).pack(side='left')
            ttk.Button(file_lf, text='Save font',           command=self.save_font).pack(side='left', padx=4)
            ttk.Button(file_lf, text='Export bytes',        command=self.export_bytes).pack(side='left')
            ttk.Button(file_lf, text='Import bytes...',  command=self.import_bytes).pack(side='left', padx=4)
            ttk.Separator(file_lf, orient='vertical').pack(side='left', fill='y', padx=6)
            ttk.Button(file_lf, text='Copy glyph',          command=self.copy_glyph).pack(side='left')
            ttk.Button(file_lf, text='Paste glyph',         command=self.paste_glyph).pack(side='left', padx=4)
            ttk.Separator(file_lf, orient='vertical').pack(side='left', fill='y', padx=6)
            ttk.Button(file_lf, text='Glyph Grid',          command=self.show_glyph_grid).pack(side='left')

            # ── Glyph Navigation ──────────────────────────────────────────────────
            nav_lf = ttk.LabelFrame(frm, text='Glyph', padding=(8, 4))
            nav_lf.pack(fill='x', pady=(0, 6))
            ttk.Label(nav_lf, text='Codepoint (0\u2013255):').pack(side='left')
            self.idx_spin = ttk.Spinbox(nav_lf, from_=0, to=255, width=5,
                                        textvariable=self.current_index,
                                        command=self.on_index_change)
            self.idx_spin.pack(side='left', padx=4)
            ttk.Button(nav_lf, text='Load glyph',
                       command=self.on_index_change).pack(side='left', padx=4)
            ttk.Separator(nav_lf, orient='vertical').pack(side='left', fill='y', padx=8)
            tk.Label(nav_lf, textvariable=self.char_big_var, font=('Segoe UI', 18),
                     bg=BG, fg=FG, width=2).pack(side='left')
            ttk.Label(nav_lf, textvariable=self.char_var,
                      foreground=FG_DIM, font=INFO_FONT).pack(side='left', padx=6)

            # ── Editor + Bytes ────────────────────────────────────────────────────
            mid = ttk.Frame(frm)
            mid.pack(fill='both', expand=True, pady=(0, 4))

            ed_lf = ttk.LabelFrame(mid, text='Editor', padding=(8, 4))
            ed_lf.pack(side='left', fill='y', padx=(0, 6))
            self.canvas = tk.Canvas(ed_lf, width=self.cell_size * 5,
                                    height=self.cell_size * 7, bg='white',
                                    highlightthickness=1, highlightbackground=BORDER)
            self.canvas.pack()
            self.canvas.bind('<Button-1>', self.on_canvas_click)
            self._draw_grid()

            bytes_lf = ttk.LabelFrame(mid, text='Bytes', padding=(8, 4))
            bytes_lf.pack(side='left', fill='both', expand=True)
            ttk.Label(bytes_lf, text='Hex:', font=HEADER_FONT).grid(
                row=0, column=0, sticky='w')
            self.hex_var = tk.StringVar()
            ttk.Entry(bytes_lf, textvariable=self.hex_var, width=38,
                      font=HEX_FONT).grid(row=0, column=1, sticky='w', padx=(4, 0))
            ttk.Label(bytes_lf, text='Binary:', font=HEADER_FONT).grid(
                row=1, column=0, sticky='nw', pady=(6, 0))
            self.bin_text = tk.Text(bytes_lf, width=38, height=7,
                                    bg=SEC, fg=FG, font=HEX_FONT,
                                    relief='flat', bd=1, insertbackground=FG)
            self.bin_text.grid(row=1, column=1, sticky='w', padx=(4, 0), pady=(6, 0))

            # ── Menu ──────────────────────────────────────────────────────────────
            menubar = tk.Menu(self)
            filem = tk.Menu(menubar, tearoff=False)
            filem.add_command(label='Load font...', command=self.load_font)
            filem.add_command(label='Save font', command=self.save_font)
            filem.add_separator()
            filem.add_command(label='Exit', command=self.quit)
            menubar.add_cascade(label='File', menu=filem)
            self.config(menu=menubar)
            # keyboard shortcuts
            self.bind('<Control-c>', lambda e: self.copy_glyph())
            self.bind('<Control-C>', lambda e: self.copy_glyph())
            self.bind('<Control-v>', lambda e: self.paste_glyph())
            self.bind('<Control-V>', lambda e: self.paste_glyph())

        def _draw_grid(self):
            self.canvas.delete('all')
            for r in range(7):
                for c in range(5):
                    x0 = c*self.cell_size
                    y0 = r*self.cell_size
                    x1 = x0 + self.cell_size
                    y1 = y0 + self.cell_size
                    self.canvas.create_rectangle(x0, y0, x1, y1, outline='#999', fill='white', tags=f'cell_{r}_{c}')

        def load_font(self):
            p = filedialog.askopenfilename(title='Open glcdfont C file', filetypes=[('C files','*.c'), ('All files','*.*')])
            if not p:
                return
            self._load_file(Path(p))

        def _load_file(self, path:Path):
            text = path.read_text(encoding='utf-8')
            m = FONT_ARRAY_RE.search(text)
            if not m:
                messagebox.showerror('Error', 'Font array not found in file')
                return
            # remove block and line comments so '0x..' in comments (e.g., '(0xFB)') aren't counted
            block = m.group(1)
            block = re.sub(r'/\*[\s\S]*?\*/', '', block)
            block = re.sub(r'//.*', '', block)
            bytes_hex = HEX_RE.findall(block)
            arr = [int(x,16) for x in bytes_hex]
            if len(arr) < 256*5:
                arr += [0]*(256*5 - len(arr))
            self.font_path = path
            self.font_bytes = arr
            self.title(f'5x7 Glyph Creator - {path}')
            self.on_index_change()
            # Refresh or open glyph grid when a font is loaded
            try:
                if hasattr(self, '_grid_win') and getattr(self, '_grid_win') and self._grid_win.winfo_exists():
                    try:
                        self._refresh_grid_all()
                        self._highlight_grid_selection(int(self.current_index.get()))
                        self._grid_win.lift()
                    except Exception:
                        pass
                else:
                    self.show_glyph_grid()
            except Exception:
                pass

        def save_font(self):
            if not self.font_path:
                messagebox.showinfo('Save', 'No font loaded to save to')
                return
            # update array with current glyph
            idx = int(self.current_index.get())
            glyph = self._get_canvas_glyph()
            off = idx*5
            for i in range(5):
                self.font_bytes[off+i] = glyph[i]
            try:
                bak = _save_font_file(self.font_path, self.font_bytes)
                if not bak:
                    messagebox.showinfo('Saved', 'No changes detected; file not modified')
                else:
                    messagebox.showinfo('Saved', f'Font saved to {self.font_path}\nBackup: {bak}')
                # Refresh grid if open so saved glyphs are immediately visible
                try:
                    if hasattr(self, '_grid_win') and getattr(self, '_grid_win') and self._grid_win.winfo_exists():
                        # refresh only the changed glyph for speed
                        try:
                            self._refresh_grid_cell(idx)
                            self._highlight_grid_selection(idx)
                        except Exception:
                            # fallback: refresh whole grid
                            try:
                                self._refresh_grid_all()
                            except Exception:
                                pass
                except Exception:
                    pass
            except Exception as e:
                messagebox.showerror('Error', str(e))

        def export_bytes(self):
            glyph = self._get_canvas_glyph()
            s = ', '.join(f'0x{b:02X}' for b in glyph)
            self.clipboard_clear()
            self.clipboard_append(s)
            messagebox.showinfo('Export', f'Glyph bytes copied to clipboard:\n{s}')

        def import_bytes(self):
            p = filedialog.askopenfilename(title='Import a glcdfont C to copy glyph from', filetypes=[('C files','*.c'), ('All files','*.*')])
            if not p:
                return
            arr = self._parse_font_file(Path(p))
            if not arr:
                messagebox.showerror('Error','Could not parse file')
                return
            idx = int(self.current_index.get())
            glyph = arr[idx*5:idx*5+5]
            self._set_canvas_glyph(glyph)

        def _parse_font_file(self,path:Path):
            txt = path.read_text(encoding='utf-8')
            m = FONT_ARRAY_RE.search(txt)
            if not m:
                return None
            block = m.group(1)
            block = re.sub(r'/\*[\s\S]*?\*/', '', block)
            block = re.sub(r'//.*', '', block)
            bytes_hex = HEX_RE.findall(block)
            arr = [int(x,16) for x in bytes_hex]
            if len(arr) < 256*5:
                arr += [0]*(256*5 - len(arr))
            return arr
        def copy_glyph(self):
            """Copy current glyph to internal buffer and system clipboard as hex bytes."""
            glyph = self._get_canvas_glyph()
            self._clipboard_glyph = glyph[:]
            s = ','.join(f'0x{b:02X}' for b in glyph)
            try:
                self.clipboard_clear()
                self.clipboard_append(s)
                messagebox.showinfo('Copied', f'Glyph copied to clipboard:\n{s}')
            except Exception:
                # silent fallback
                messagebox.showinfo('Copied', f'Glyph copied to internal buffer:\n{s}')

        def paste_glyph(self):
            """Paste glyph from system clipboard or internal buffer and apply to canvas."""
            txt = ''
            try:
                txt = self.clipboard_get()
            except Exception:
                txt = ''
            if not txt and self._clipboard_glyph:
                glyph = self._clipboard_glyph[:]
                self._set_canvas_glyph(glyph)
                messagebox.showinfo('Pasted', 'Glyph pasted from internal buffer')
                return
            if txt:
                # parse hex numbers
                m = HEX_RE.findall(txt)
                if len(m) >= 5:
                    glyph = [int(x,16) for x in m[:5]]
                    self._set_canvas_glyph(glyph)
                    messagebox.showinfo('Pasted', f'Glyph pasted from clipboard: {",".join(f"0x{b:02X}" for b in glyph)}')
                    return
                # fallback: parse decimals
                try:
                    parts = re.findall(r"-?\d+", txt)
                    if len(parts) >= 5:
                        glyph = [int(x,0) & 0xFF for x in parts[:5]]
                        self._set_canvas_glyph(glyph)
                        messagebox.showinfo('Pasted', 'Glyph pasted from clipboard (decimal)')
                        return
                except Exception:
                    pass
            messagebox.showerror('Paste failed', 'No valid glyph bytes found in clipboard or internal buffer')

        def on_index_change(self):
            if self.font_bytes is None:
                # clear canvas
                self._set_canvas_glyph([0,0,0,0,0])
                return
            idx = int(self.current_index.get())
            off = idx*5
            glyph = self.font_bytes[off:off+5]
            self._set_canvas_glyph(glyph)
            # update character preview label (show 'char' and Unicode codepoint)
            ch = index_to_char(idx)
            if ch != '?':
                self.char_var.set(f"'{ch}' U+{ord(ch):04X} ({idx})")
                self.char_big_var.set(ch)
            else:
                self.char_var.set(f"(unknown) 0x{idx:02X}")
                self.char_big_var.set('')

        def _set_canvas_glyph(self, glyph):
            self._glyph = glyph[:]  # 5 ints
            # update canvas pixels
            self.canvas.delete('pix')
            for r in range(7):
                for c in range(5):
                    b = (glyph[c] >> r) & 1
                    x0 = c*self.cell_size
                    y0 = r*self.cell_size
                    x1 = x0 + self.cell_size
                    y1 = y0 + self.cell_size
                    if b:
                        self.canvas.create_rectangle(x0+2,y0+2,x1-2,y1-2, fill='black', outline='', tags='pix')
            # update hex and binary displays
            self.hex_var.set(', '.join(f'0x{b:02X}' for b in glyph))
            self.bin_text.delete('1.0','end')
            for r in range(7):
                row = ''.join('#' if ((glyph[c]>>r)&1) else ' ' for c in range(5))
                self.bin_text.insert('end', row + '\n')

        def _get_canvas_glyph(self):
            return self._glyph[:]

        def on_canvas_click(self, event):
            c = event.x // self.cell_size
            r = event.y // self.cell_size
            if c<0 or c>=5 or r<0 or r>=7:
                return
            # toggle bit
            mask = 1 << r
            self._glyph[c] ^= mask
            self._set_canvas_glyph(self._glyph)

        def render_preview(self):
            if self.font_bytes is None:
                messagebox.showinfo('Preview','No font loaded')
                return
            s = self.sample_entry.get()
            # simple mapper - use known mappings for Lithuanian glyphs
            utf_map = {'Ą':0xB7,'ą':0xB8,'Č':0xC9,'č':0xCA,'Ė':0xB9,'ė':0xBA,'Į':0xD7,'į':0xFE,'Š':0xC2,'š':0xC3,'Ų':0xD6,'ų':0xFF,'Ū':0xE4,'ū':0xE6,'Ž':0xC7,'ž':0xC8}
            rows = ['']*7
            for ch in s:
                if ch in utf_map:
                    idx = utf_map[ch]
                elif ord(ch) < 128:
                    idx = ord(ch)
                else:
                    idx = ord('?')
                off = idx*5
                glyph = self.font_bytes[off:off+5]
                for r in range(7):
                    rows[r] += ''.join('#' if ((glyph[c]>>r)&1) else ' ' for c in range(5)) + ' '
            self.preview.delete('1.0','end')
            self.preview.insert('end','\n'.join(rows))

        def show_glyph_grid(self, cols=20, cell=8):
            """Open glyph grid window (select and keep open)."""
            # If already open, bring to front
            if hasattr(self, '_grid_win') and getattr(self, '_grid_win') and self._grid_win.winfo_exists():
                try:
                    self._grid_win.lift()
                except Exception:
                    pass
                return
            # Ensure a font is loaded before rendering the grid
            if not self.font_bytes or all(b == 0 for b in self.font_bytes):
                if DEFAULT_FONT.exists():
                    try:
                        self._load_file(DEFAULT_FONT)
                        messagebox.showinfo('Glyph Grid', f'Loaded default font: {DEFAULT_FONT.name}')
                    except Exception as e:
                        messagebox.showerror('Glyph Grid', f'Could not load default font: {e}')
                else:
                    messagebox.showwarning('Glyph Grid', 'No font loaded and default font not found. Use Load font in the main window first.')

            win = tk.Toplevel(self)
            win.title('Glyph Grid')
            self._grid_win = win
            frm = ttk.Frame(win)
            frm.pack(fill='both', expand=True)
            win.configure(bg=BG)
            win.resizable(True, True)
            ttk.Label(frm, text=f"Font: {self.font_path.name if self.font_path else '(none)'}",
                      foreground=ACCENT, font=HEADER_FONT).pack(anchor='w', padx=6, pady=4)

            canvas = tk.Canvas(frm, bg=BG, highlightthickness=0)
            vbar = ttk.Scrollbar(frm, orient='vertical', command=canvas.yview)
            canvas.configure(yscrollcommand=vbar.set)
            vbar.pack(side='right', fill='y')
            canvas.pack(side='left', fill='both', expand=True)

            # keep references so we can refresh/update the grid later
            self._grid_canvas = canvas
            container = tk.Frame(canvas, bg=BG)
            self._grid_container = container
            self._grid_cwin_id = canvas.create_window((0, 0), window=container, anchor='nw')
            inner = tk.Frame(container, bg=BG)
            inner.place(relx=0.5, y=0, anchor='n')
            self._grid_inner = inner

            # prepare storage for single-selection highlighting
            self._grid_cells = {}
            self._grid_selected_idx = None
            self._grid_cell_size = cell
            self._grid_built_cols = None

            inner.bind('<Configure>', lambda e: self.after_idle(self._recentre_grid))
            canvas.bind('<Configure>', self._on_grid_configure)
            canvas.bind_all('<MouseWheel>', lambda e: canvas.yview_scroll(
                -1 * (e.delta // 120), 'units'))

            self._rebuild_grid_cells(cols)

            # Highlight current selection
            try:
                cur_idx = int(self.current_index.get())
            except Exception:
                cur_idx = getattr(self, 'current_index', 0)
            self._highlight_grid_selection(cur_idx)

            # Initial window size: cols wide, 10 rows visible
            slot_w = cell * 5 + 2 + 2    # canvas + 2*highlight + 2*padx
            slot_h = cell * 7 + 2 + 18   # canvas + 2*highlight + label row
            try:
                scr_w = self.winfo_screenwidth()
                scr_h = self.winfo_screenheight()
                win_w = min(cols * slot_w + 22, scr_w - 40)
                win_h = min(10 * slot_h + 80,  scr_h - 80)
                win.geometry(f'{win_w}x{win_h}')
                win.minsize(slot_w * 2 + 24, slot_h * 2 + 80)
            except Exception:
                pass

        def _rebuild_grid_cells(self, cols):
            """Destroy and recreate all 256 glyph cell canvases for the given column count."""
            if cols == self._grid_built_cols:
                return
            self._grid_built_cols = cols
            self._grid_cells = {}
            inner = self._grid_inner
            cell  = self._grid_cell_size
            for widget in inner.winfo_children():
                widget.destroy()
            for idx in range(256):
                r = idx // cols
                c = idx % cols
                cc = tk.Canvas(inner, width=cell * 5, height=cell * 7,
                               bg='white', highlightthickness=1,
                               highlightbackground='#cccccc')
                cc.grid(row=r * 2, column=c, padx=1, pady=1)
                self._grid_cells[idx] = cc
                glyph = self.font_bytes[idx*5:idx*5+5] if self.font_bytes else [0]*5
                for ci in range(5):
                    for ri in range(7):
                        if (glyph[ci] >> ri) & 1:
                            x0, y0 = ci * cell, ri * cell
                            # fill entire cell to make pixels touch on both sides
                            cc.create_rectangle(x0, y0, x0+cell, y0+cell,
                                                fill='black', outline='')
                cc.bind('<Button-1>', lambda e, i=idx: self._select_glyph_from_grid(i))
                # display decimal slot number instead of hex
                tk.Label(inner, text=str(idx), anchor='center',
                          font=('Courier New', 8), foreground=FG_DIM, bg=BG
                          ).grid(row=r * 2 + 1, column=c)
            inner.update_idletasks()
            sel = getattr(self, '_grid_selected_idx', None)
            if sel is not None:
                self._highlight_grid_selection(sel)
            self.after_idle(self._recentre_grid)

        def _on_grid_configure(self, event):
            if event.width < 10:
                return
            self._last_grid_canvas_w = event.width
            canvas = self._grid_canvas
            canvas.itemconfigure(self._grid_cwin_id, width=event.width)
            slot = self._grid_cell_size * 5 + 4
            new_cols = max(1, event.width // slot)
            self._rebuild_grid_cells(new_cols)
            self.after_idle(self._recentre_grid)

        def _recentre_grid(self):
            canvas = getattr(self, '_grid_canvas', None)
            if not canvas:
                return
            w = getattr(self, '_last_grid_canvas_w', 0) or canvas.winfo_width()
            if w < 10:
                return
            inner = self._grid_inner
            h = inner.winfo_reqheight()
            if h < 1:
                return
            self._grid_container.configure(height=h)
            canvas.configure(scrollregion=(0, 0, w, h))

        def _select_glyph_from_grid(self, idx):
            # Set current index and update editor preview; keep grid open
            try:
                self.current_index.set(idx)
            except Exception:
                self.current_index = idx
            self.on_index_change()
            # update highlighted selection
            try:
                self._highlight_grid_selection(idx)
            except Exception:
                pass

        def _refresh_grid_cell(self, idx):
            """Redraw a single glyph cell in the glyph grid."""
            cells = getattr(self, '_grid_cells', {})
            if idx not in cells:
                return
            c = cells[idx]
            try:
                c.delete('all')
            except Exception:
                pass
            if self.font_bytes:
                off = idx * 5
                glyph = self.font_bytes[off:off+5]
            else:
                glyph = [0,0,0,0,0]
            cell = getattr(self, '_grid_cell_size', 8)
            for col in range(5):
                for row_pix in range(7):
                    if (glyph[col] >> row_pix) & 1:
                        x0 = col*cell
                        y0 = row_pix*cell
                        x1 = x0 + cell - 1
                        y1 = y0 + cell - 1
                        try:
                            c.create_rectangle(x0, y0, x1, y1, fill='black', outline='')
                        except Exception:
                            pass
            # re-apply selection highlight if needed
            if getattr(self, '_grid_selected_idx', None) == idx:
                try:
                    c.configure(highlightthickness=2,
                                highlightbackground=ACCENT)
                except Exception:
                    pass

        def _refresh_grid_all(self):
            """Redraw all glyph cells in the glyph grid and update scrollregion."""
            for idx in list(getattr(self, '_grid_cells', {}).keys()):
                try:
                    self._refresh_grid_cell(idx)
                except Exception:
                    pass
            try:
                if getattr(self, '_grid_inner', None):
                    self._grid_inner.update_idletasks()
                self.after_idle(self._recentre_grid)
            except Exception:
                pass

        def _highlight_grid_selection(self, idx):
            prev  = getattr(self, '_grid_selected_idx', None)
            cells = getattr(self, '_grid_cells', {})
            if prev is not None and prev in cells and prev != idx:
                try:
                    cells[prev].configure(highlightthickness=1,
                                          highlightbackground='#cccccc')
                except Exception:
                    pass
            if idx in cells:
                try:
                    cells[idx].configure(highlightthickness=2,
                                         highlightbackground=ACCENT)
                except Exception:
                    pass
            self._grid_selected_idx = idx

if not TK_AVAILABLE:
    class GlyphEditor:
        def __init__(self):
            self.font_path=None
            self.font_bytes=None
            self.current_index = 0
            self._glyph=[0,0,0,0,0]
            self._clipboard_glyph = None
        def _load_file(self,path):
            txt = Path(path).read_text(encoding='utf-8')
            m = FONT_ARRAY_RE.search(txt)
            if not m:
                raise RuntimeError('Font array not found')
            block = m.group(1)
            block = re.sub(r'/\*[\s\S]*?\*/', '', block)
            block = re.sub(r'//.*', '', block)
            bytes_hex = HEX_RE.findall(block)
            arr=[int(x,16) for x in bytes_hex]
            if len(arr) < 256*5:
                arr += [0]*(256*5 - len(arr))
            self.font_path = Path(path)
            self.font_bytes = arr
        def copy_glyph(self):
            g = self._get_canvas_glyph()
            self._clipboard_glyph = g[:]
            # try to copy to system clipboard using pyperclip if installed
            try:
                import pyperclip
                pyperclip.copy(','.join(f'0x{b:02X}' for b in g))
            except Exception:
                pass
            return g
        
        def save_font(self, path=None):
            """Save current font bytes back to file (minimal edits)."""
            if path is None:
                path = self.font_path
            if not path:
                raise RuntimeError('No font loaded to save to')
            bak = _save_font_file(Path(path), self.font_bytes)
            return bak
        def paste_from_str(self, s: str):
            m = HEX_RE.findall(s)
            if len(m) >= 5:
                glyph = [int(x,16) for x in m[:5]]
                self._set_canvas_glyph(glyph)
                return True
            parts = re.findall(r"-?\d+", s)
            if len(parts) >= 5:
                glyph = [int(x,0) & 0xFF for x in parts[:5]]
                self._set_canvas_glyph(glyph)
                return True
            return False
        def paste_glyph(self):
            # try internal buffer
            if self._clipboard_glyph:
                self._set_canvas_glyph(self._clipboard_glyph[:])
                return True
            # try system clipboard via pyperclip
            try:
                import pyperclip
                txt = pyperclip.paste()
                return self.paste_from_str(txt)
            except Exception:
                return False
        def on_index_change(self):
            idx = int(self.current_index)
            off = idx*5
            glyph = self.font_bytes[off:off+5]
            self._set_canvas_glyph(glyph)
        def _set_canvas_glyph(self,glyph):
            self._glyph = glyph[:]
        def _get_canvas_glyph(self):
            return self._glyph[:]
        def save_font(self):
            if not self.font_path:
                raise RuntimeError('No font loaded')
            idx = int(self.current_index)
            off = idx*5
            for i in range(5):
                self.font_bytes[off+i] = self._glyph[i]
            text = self.font_path.read_text(encoding='utf-8')
            m = FONT_ARRAY_RE.search(text)
            pieces = [f'0x{b:02X}' for b in self.font_bytes]
            lines = []
            for i in range(0, len(pieces), 16):
                lines.append('    ' + ', '.join(pieces[i:i+16]) + ',')
            new_block = '\n'.join(lines)
            new_text = text[:m.start(1)] + '\n' + new_block + '\n' + text[m.end(1):]
            self.font_path.write_text(new_text, encoding='utf-8')

if __name__ == '__main__':
    import sys
    # Support a simple CLI mode when stdin is redirected or '--cli' passed
    if not sys.stdin.isatty() or '--cli' in sys.argv:
        editor = GlyphEditor()
        def run_cli():
            data = sys.stdin.read().splitlines()
            for line in data:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split()
                cmd = parts[0].upper()
                try:
                    if cmd == 'LOAD':
                        path = ' '.join(parts[1:])
                        editor._load_file(Path(path))
                        print(f'Loaded {path}')
                    elif cmd in ('IDX','INDEX'):
                        v = int(parts[1],0)
                        _set_index(editor, v)
                        editor.on_index_change()
                        print(f'Index set to {_get_index(editor)}')
                    elif cmd in ('SHOW','PRINT'):
                        glyph = editor._get_canvas_glyph()
                        print('Glyph bytes:', ', '.join(f'0x{b:02X}' for b in glyph))
                        idx = _get_index(editor)
                        ch = index_to_char(idx)
                        if ch != '?':
                            print(f"Char: '{ch}' U+{ord(ch):04X} (idx {idx})")
                        else:
                            print(f"Char: (unknown) 0x{idx:02X}")
                    elif cmd == 'EXPORT':
                        glyph = editor._get_canvas_glyph()
                        print(','.join(f'0x{b:02X}' for b in glyph))
                    elif cmd == 'COPY':
                        glyph = editor.copy_glyph()
                        print('Copied:', ','.join(f'0x{b:02X}' for b in glyph))
                    elif cmd == 'PASTE':
                        # PASTE with args: PASTE 0x.. 0x.. ... ; or PASTE CLIP
                        if len(parts) > 1:
                            arg = parts[1].upper()
                            if arg == 'CLIP':
                                ok = editor.paste_glyph()
                                print('Pasted from clipboard' if ok else 'Paste failed')
                            else:
                                # try parse bytes on the rest of the line
                                rest = ' '.join(parts[1:])
                                ok = editor.paste_from_str(rest)
                                print('Pasted' if ok else 'Paste failed')
                        else:
                            ok = editor.paste_glyph()
                            print('Pasted from clipboard' if ok else 'Paste failed')
                    elif cmd == 'TOGGLE':
                        c = int(parts[1]); r = int(parts[2])
                        editor._glyph[c] ^= (1<<r)
                        editor._set_canvas_glyph(editor._glyph)
                        print(f'Toggled {c},{r}')
                    elif cmd == 'SET':
                        glyph = [int(x,0) for x in parts[1:6]]
                        editor._set_canvas_glyph(glyph)
                        print('Set glyph bytes')
                    elif cmd == 'SAVE':
                        if editor.font_path:
                            bak = editor.font_path.with_suffix(editor.font_path.suffix + '.bak')
                            if not bak.exists():
                                bak.write_bytes(editor.font_path.read_bytes())
                                print(f'Backup written to {bak}')
                        editor.save_font()
                    elif cmd == 'EXIT':
                        break
                    else:
                        print('Unknown command:', cmd)
                except Exception as e:
                    print('Error executing', cmd, '-', e)
        run_cli()
    else:
        app = GlyphEditor()
        app.mainloop()
