#!/usr/bin/env python3
"""
treinadores_animados.py - janela para escolher quais sprites de treinador ficam animados

Abra com o treinadores_animados.bat (ou: python scripts//treinadores_animados.py na raiz do projeto).
  - mostra todos os sprites de treinador da ROM base (BPRE0.gba)
  - clique num sprite para ver quem usa, a animacao atual e as opcoes
  - "Escolher PNG da animacao...": aceita 64x128, 128x64 ou so o quadro 2 (64x64)
  - "Exportar modelo...": salva o sprite original em 64x128 para voce desenhar o quadro 2
Os arquivos vao para graphics/animated_trainers/ e a tabela e atualizada na hora.
Depois e so compilar com o a_makepy.bat.
"""

import base64
import io
import os
import shutil
import struct
import sys
import zlib

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import animated_trainers as at  # noqa: E402

THUMB = 64
CELL_W, CELL_H = 84, 96
TRANSPARENT_BG = '#d9d6e8'
COLOR_ANIMATED = '#e2463e'
COLOR_SELECTED = '#3b2f7e'
MODEL_FOLDER = 'modelos_treinadores'


def png_base64(rows, palette, transparent0=True):
    """PNG indexado (4 bits) em base64 para o Tk mostrar; cor 0 transparente."""
    h, w = len(rows), len(rows[0])
    raw = bytearray()
    for r in rows:
        raw.append(0)
        for x in range(0, w, 2):
            raw.append(((r[x] & 0xF) << 4) | ((r[x + 1] & 0xF) if x + 1 < w else 0))
    pal = list(palette[:16]) + [(0, 0, 0)] * (16 - min(16, len(palette)))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)

    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 4, 3, 0, 0, 0))
    data += chunk(b'PLTE', b''.join(bytes(c) for c in pal))
    if transparent0:
        data += chunk(b'tRNS', b'\x00')
    data += chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    return base64.b64encode(data).decode('ascii')


def nearest(color, palette5):
    c = at.rgb5(color)
    best, best_i = None, 1
    for i in range(1, 16):
        p = palette5[i]
        d = (c[0] - p[0]) ** 2 + (c[1] - p[1]) ** 2 + (c[2] - p[2]) ** 2
        if best is None or d < best:
            best, best_i = d, i
    return best_i, best == 0


class App:
    def __init__(self, root, rom_path=None):
        self.root = root
        root.title('Treinadores animados - CFRU')
        root.geometry('1100x720')
        root.minsize(900, 600)

        project = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        os.chdir(project)
        rom_path = rom_path or at.find_rom_name()
        if not os.path.isfile(rom_path):
            rom_path = filedialog.askopenfilename(title='Escolha a ROM base do projeto (BPRE0.gba)',
                                                  filetypes=[('ROM de GBA', '*.gba'), ('Todos', '*.*')])
            if not rom_path:
                raise SystemExit
        self.rom = at.RomTrainers(rom_path)
        self.count = self.rom.count()
        self.users = self.rom.trainer_users()
        self.images = {}
        self.selected = None
        self.anim_job = None
        self.anim_frame = 0
        self.filter = tk.StringVar()

        self.build_ui()
        self.rebuild_table(quiet=True)
        self.draw_grid()
        self.select(self.first_visible())

    # ------------------------------------------------------------------ UI
    def build_ui(self):
        style = ttk.Style()
        try:
            style.theme_use('clam')
        except tk.TclError:
            pass
        top = ttk.Frame(self.root, padding=(10, 8))
        top.pack(fill='x')
        ttk.Label(top, text='Sprites de treinador', font=('Segoe UI', 13, 'bold')).pack(side='left')
        ttk.Label(top, text='   Buscar (numero ou nome):').pack(side='left')
        entry = ttk.Entry(top, textvariable=self.filter, width=24)
        entry.pack(side='left', padx=6)
        self.filter.trace_add('write', lambda *a: self.draw_grid())
        self.only_anim = tk.BooleanVar(value=False)
        ttk.Checkbutton(top, text='so os animados', variable=self.only_anim,
                        command=self.draw_grid).pack(side='left', padx=8)
        ttk.Label(top, text='  ■ vermelho = animado', foreground=COLOR_ANIMATED).pack(side='left')

        body = ttk.Frame(self.root)
        body.pack(fill='both', expand=True)

        left = ttk.Frame(body)
        left.pack(side='left', fill='both', expand=True)
        self.canvas = tk.Canvas(left, background='#f4f3f9', highlightthickness=0)
        sb = ttk.Scrollbar(left, orient='vertical', command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=sb.set)
        sb.pack(side='right', fill='y')
        self.canvas.pack(side='left', fill='both', expand=True)
        self.canvas.bind('<Configure>', lambda e: self.draw_grid())
        self.canvas.bind_all('<MouseWheel>', lambda e: self.canvas.yview_scroll(int(-e.delta / 120), 'units'))
        self.canvas.bind_all('<Button-4>', lambda e: self.canvas.yview_scroll(-3, 'units'))
        self.canvas.bind_all('<Button-5>', lambda e: self.canvas.yview_scroll(3, 'units'))

        right = ttk.Frame(body, padding=12, width=360)
        right.pack(side='right', fill='y')
        right.pack_propagate(False)
        self.title = ttk.Label(right, text='', font=('Segoe UI', 14, 'bold'))
        self.title.pack(anchor='w')
        self.users_lbl = ttk.Label(right, text='', wraplength=330, foreground='#555')
        self.users_lbl.pack(anchor='w', pady=(2, 8))

        view = ttk.Frame(right)
        view.pack(anchor='w')
        self.preview = tk.Canvas(view, width=192, height=192, background=TRANSPARENT_BG, highlightthickness=1,
                                 highlightbackground='#bbb')
        self.preview.grid(row=0, column=0, rowspan=4, sticky='n')
        self.f1 = tk.Canvas(view, width=64, height=64, background=TRANSPARENT_BG, highlightthickness=0)
        self.f2 = tk.Canvas(view, width=64, height=64, background=TRANSPARENT_BG, highlightthickness=0)
        ttk.Label(view, text='quadro 1').grid(row=0, column=1, padx=(12, 0), sticky='sw')
        self.f1.grid(row=1, column=1, padx=(12, 0), sticky='nw')
        ttk.Label(view, text='quadro 2').grid(row=2, column=1, padx=(12, 0), sticky='sw')
        self.f2.grid(row=3, column=1, padx=(12, 0), sticky='nw')
        self.state_lbl = ttk.Label(right, text='', font=('Segoe UI', 10, 'bold'))
        self.state_lbl.pack(anchor='w', pady=(6, 0))

        opts = ttk.Frame(right)
        opts.pack(anchor='w', pady=(6, 2))
        ttk.Label(opts, text='Velocidade (frames por quadro):').grid(row=0, column=0, sticky='w')
        self.speed = tk.IntVar(value=at.DEFAULT_SPEED)
        ttk.Spinbox(opts, from_=1, to=255, textvariable=self.speed, width=6,
                    command=self.restart_preview).grid(row=0, column=1, padx=6)
        ttk.Label(opts, text='Repeticoes (0 = sempre):').grid(row=1, column=0, sticky='w', pady=4)
        self.plays = tk.IntVar(value=0)
        ttk.Spinbox(opts, from_=0, to=255, textvariable=self.plays, width=6).grid(row=1, column=1, padx=6)

        ttk.Button(right, text='Escolher PNG da animacao...', command=self.choose_png).pack(fill='x', pady=(8, 3))
        ttk.Button(right, text='Aplicar velocidade / repeticoes', command=self.apply_options).pack(fill='x', pady=3)
        ttk.Button(right, text='Exportar modelo para desenhar...', command=self.export_model).pack(fill='x', pady=3)
        ttk.Button(right, text='Remover animacao', command=self.remove_anim).pack(fill='x', pady=3)
        ttk.Label(right, text='Depois compile com o a_makepy.bat.', foreground='#555').pack(anchor='w', pady=(10, 0))

        self.status = ttk.Label(self.root, text='', padding=(10, 4), relief='sunken', anchor='w')
        self.status.pack(fill='x', side='bottom')

    # ---------------------------------------------------------------- data
    def animated_files(self):
        """{sprite: (arquivo, velocidade, repeticoes)} a partir dos nomes em graphics/animated_trainers."""
        res = {}
        if not os.path.isdir(at.SPRITE_FOLDER):
            return res
        for f in sorted(os.listdir(at.SPRITE_FOLDER)):
            if f.lower().endswith('.png'):
                pid, speed, plays = at.parse_name(f[:-4])
                if pid is not None and pid not in res:
                    res[pid] = (f, speed or at.DEFAULT_SPEED, plays or 0)
        return res

    def thumb(self, pid, zoom=1):
        key = (pid, zoom)
        if key not in self.images:
            p = self.rom.pic(pid)
            if p is None:
                return None
            img = tk.PhotoImage(data=png_base64(p[0], p[1]))
            if zoom > 1:
                img = img.zoom(zoom)
            self.images[key] = img
        return self.images[key]

    def visible_ids(self):
        q = self.filter.get().strip().lower()
        anim = self.animated_files()
        ids = []
        for pid in range(self.count):
            if self.only_anim.get() and pid not in anim:
                continue
            if q:
                names = ' '.join(self.users.get(pid, [])).lower()
                num_match = q in (str(pid), hex(pid), '0x%x' % pid)
                if not (num_match or q in names):
                    continue
            ids.append(pid)
        return ids

    def first_visible(self):
        ids = self.visible_ids()
        return ids[0] if ids else 0

    # ---------------------------------------------------------------- grid
    def draw_grid(self):
        c = self.canvas
        c.delete('all')
        width = max(c.winfo_width(), CELL_W)
        cols = max(1, width // CELL_W)
        anim = self.animated_files()
        ids = self.visible_ids()
        for n, pid in enumerate(ids):
            x, y = (n % cols) * CELL_W + 10, (n // cols) * CELL_H + 8
            tag = 'pic%d' % pid
            color = COLOR_SELECTED if pid == self.selected else (COLOR_ANIMATED if pid in anim else '#cfcbe0')
            width_line = 3 if (pid == self.selected or pid in anim) else 1
            c.create_rectangle(x - 3, y - 3, x + THUMB + 3, y + THUMB + 3, outline=color, width=width_line,
                               fill=TRANSPARENT_BG, tags=tag)
            img = self.thumb(pid)
            if img is not None:
                c.create_image(x, y, image=img, anchor='nw', tags=tag)
            c.create_text(x + THUMB // 2, y + THUMB + 12, text='%d (0x%X)' % (pid, pid),
                          font=('Segoe UI', 8), fill=COLOR_ANIMATED if pid in anim else '#333', tags=tag)
            c.tag_bind(tag, '<Button-1>', lambda e, p=pid: self.select(p, scroll=False))
        rows = (len(ids) + cols - 1) // cols
        c.configure(scrollregion=(0, 0, cols * CELL_W, rows * CELL_H + 10))
        self.grid_layout = (ids, cols, rows)
        if not ids:
            c.create_text(20, 20, anchor='nw', text='Nenhum sprite corresponde a busca.', fill='#555')

    # ------------------------------------------------------------- details
    def scroll_to(self, pid):
        ids, cols, rows = getattr(self, 'grid_layout', ([], 1, 0))
        if pid not in ids or rows == 0:
            return
        row = ids.index(pid) // cols
        top, bottom = self.canvas.yview()
        total = rows * CELL_H + 10
        y0, y1 = row * CELL_H / total, (row + 1) * CELL_H / total
        if y0 < top or y1 > bottom:
            self.canvas.yview_moveto(max(0.0, y0 - (bottom - top) / 3))

    def select(self, pid, scroll=True):
        self.selected = pid
        users = self.users.get(pid, [])
        self.title.configure(text='Sprite %d (0x%X)' % (pid, pid))
        if users:
            shown = ', '.join(users[:8]) + (' e mais %d' % (len(users) - 8) if len(users) > 8 else '')
            self.users_lbl.configure(text='Usado por: ' + shown)
        else:
            self.users_lbl.configure(text='Nenhum treinador da ROM usa este sprite.')
        anim = self.animated_files()
        if pid in anim:
            f, speed, plays = anim[pid]
            self.speed.set(speed)
            self.plays.set(plays)
        else:
            self.speed.set(at.DEFAULT_SPEED)
            self.plays.set(0)
        self.draw_grid()
        if scroll:
            self.scroll_to(pid)
        self.restart_preview()

    def current_frames(self):
        pid = self.selected
        p = self.rom.pic(pid)
        if p is None:
            return None, None, None
        anim = self.animated_files()
        if pid in anim:
            try:
                w, h, rows, pal = at.read_png(os.path.join(at.SPRITE_FOLDER, anim[pid][0]))
                if (w, h) == (64, 128):
                    return rows[:64], rows[64:], p[1]  # o jogo usa a paleta do treinador
            except Exception:
                pass
        return p[0], None, p[1]

    def restart_preview(self):
        if self.anim_job is not None:
            self.root.after_cancel(self.anim_job)
            self.anim_job = None
        f1, f2, pal = self.current_frames()
        for cv in (self.preview, self.f1, self.f2):
            cv.delete('all')
        if f1 is None:
            self.state_lbl.configure(text='(sprite sem dados)')
            return
        self.pv = [tk.PhotoImage(data=png_base64(f1, pal)).zoom(3)]
        self.small = [tk.PhotoImage(data=png_base64(f1, pal))]
        self.f1.create_image(0, 0, image=self.small[0], anchor='nw')
        if f2 is not None:
            self.pv.append(tk.PhotoImage(data=png_base64(f2, pal)).zoom(3))
            self.small.append(tk.PhotoImage(data=png_base64(f2, pal)))
            self.f2.create_image(0, 0, image=self.small[1], anchor='nw')
            self.state_lbl.configure(text='ANIMADO', foreground=COLOR_ANIMATED)
        else:
            self.f2.create_text(32, 32, text='-', font=('Segoe UI', 16))
            self.state_lbl.configure(text='sem animacao', foreground='#555')
        self.anim_frame = 0
        self.preview_item = self.preview.create_image(0, 0, image=self.pv[0], anchor='nw')
        self.tick()

    def tick(self):
        if len(self.pv) > 1:
            self.anim_frame ^= 1
            self.preview.itemconfigure(self.preview_item, image=self.pv[self.anim_frame])
        try:
            speed = max(1, int(self.speed.get()))
        except (tk.TclError, ValueError):
            speed = at.DEFAULT_SPEED
        self.anim_job = self.root.after(int(speed * 1000 / 60), self.tick)

    # ------------------------------------------------------------- actions
    def target_name(self, pid, speed, plays):
        name = 'Treinador_%d' % pid
        if speed != at.DEFAULT_SPEED:
            name += '_v%d' % speed
        if plays:
            name += '_r%d' % plays
        return name + '.png'

    def options(self):
        try:
            speed = min(255, max(1, int(self.speed.get())))
            plays = min(255, max(0, int(self.plays.get())))
        except (tk.TclError, ValueError):
            speed, plays = at.DEFAULT_SPEED, 0
        return speed, plays

    def choose_png(self, path=None):
        pid = self.selected
        if path is None:
            path = filedialog.askopenfilename(title='PNG da animacao do sprite %d' % pid,
                                              filetypes=[('PNG', '*.png')])
            if not path:
                return
        try:
            w, h, rows, pal = at.read_png(path)
        except Exception as e:
            messagebox.showerror('PNG invalido', '%s\n\nO PNG precisa estar indexado (paleta de ate 16 cores).' % e)
            return False
        base_rows, rom_pal = self.rom.pic(pid)
        rom5 = [at.rgb5(c) for c in rom_pal]

        if (w, h) == (128, 64):
            rows = [r[:64] for r in rows] + [r[64:] for r in rows]
        elif (w, h) == (64, 64):
            rows = None, rows  # so o quadro 2
        elif (w, h) != (64, 128):
            messagebox.showerror('Tamanho errado', 'O PNG tem %dx%d.\n\nUse 64x128 (quadro 1 em cima, quadro 2 embaixo), '
                                 '128x64 (lado a lado) ou 64x64 (so o quadro 2).' % (w, h))
            return False

        # cores na ordem da paleta do treinador (a mais parecida quando nao existe igual)
        approx = set()
        cache = {0: 0}

        def remap(v):
            if v not in cache:
                if v >= len(pal) or (pal and at.rgb5(pal[v]) == at.rgb5(pal[0])):
                    cache[v] = 0
                else:
                    i, exact = nearest(pal[v], rom5)
                    cache[v] = i
                    if not exact:
                        approx.add(v)
            return cache[v]

        if isinstance(rows, tuple):
            frame2 = [[remap(v) for v in r] for r in rows[1]]
            final = [list(r) for r in base_rows] + frame2
        else:
            final = [[remap(v) for v in r] for r in rows]

        speed, plays = self.options()
        os.makedirs(at.SPRITE_FOLDER, exist_ok=True)
        self.remove_files_of(pid, backup=True)
        at.write_png(os.path.join(at.SPRITE_FOLDER, self.target_name(pid, speed, plays)), final, rom_pal)
        msg = 'Sprite %d animado com %s.' % (pid, os.path.basename(path))
        if approx:
            msg += ' %d cor(es) nao existiam na paleta do treinador e viraram a mais parecida.' % len(approx)
        self.rebuild_table()
        self.set_status(msg)
        self.draw_grid()
        self.restart_preview()
        return True

    def apply_options(self):
        pid = self.selected
        anim = self.animated_files()
        if pid not in anim:
            self.set_status('O sprite %d ainda nao tem animacao. Escolha um PNG primeiro.' % pid)
            return
        speed, plays = self.options()
        old = os.path.join(at.SPRITE_FOLDER, anim[pid][0])
        new = os.path.join(at.SPRITE_FOLDER, self.target_name(pid, speed, plays))
        if old != new:
            os.replace(old, new)
        self.rebuild_table()
        self.set_status('Sprite %d: troca a cada %d frames, %s.' % (pid, speed, 'repete sempre' if plays == 0 else '%d vez(es)' % plays))
        self.draw_grid()
        self.restart_preview()

    def remove_files_of(self, pid, backup):
        for f, _, _ in [v for k, v in self.animated_files().items() if k == pid]:
            path = os.path.join(at.SPRITE_FOLDER, f)
            if backup:
                at.backup(path)
            os.remove(path)

    def remove_anim(self):
        pid = self.selected
        if pid not in self.animated_files():
            self.set_status('O sprite %d nao tem animacao.' % pid)
            return
        if not messagebox.askyesno('Remover animacao', 'Remover a animacao do sprite %d?\n'
                                   '(uma copia fica em %s/)' % (pid, at.BACKUP_FOLDER)):
            return
        self.remove_files_of(pid, backup=True)
        self.rebuild_table()
        self.set_status('Animacao do sprite %d removida.' % pid)
        self.draw_grid()
        self.restart_preview()

    def export_model(self, path=None):
        pid = self.selected
        rows, pal = self.rom.pic(pid)
        if path is None:
            os.makedirs(MODEL_FOLDER, exist_ok=True)
            path = filedialog.asksaveasfilename(title='Salvar modelo do sprite %d' % pid,
                                                initialdir=os.path.abspath(MODEL_FOLDER),
                                                initialfile='Treinador_%d_modelo.png' % pid,
                                                defaultextension='.png', filetypes=[('PNG', '*.png')])
            if not path:
                return
        at.write_png(path, [list(r) for r in rows] + [list(r) for r in rows], pal)
        self.set_status('Modelo salvo em %s. Desenhe o quadro 2 (metade de baixo) e use "Escolher PNG".' % path)
        return path

    def rebuild_table(self, quiet=False):
        buf = io.StringIO()
        old = sys.stdout
        sys.stdout = buf
        try:
            at.main()
        finally:
            sys.stdout = old
        last = [l for l in buf.getvalue().splitlines() if l.startswith('==')]
        if not quiet and last:
            self.set_status(last[-1].strip('= '))
        return buf.getvalue()

    def set_status(self, text):
        self.status.configure(text=text)


def main():
    root = tk.Tk()
    try:
        App(root)
    except SystemExit:
        root.destroy()
        return
    root.mainloop()


if __name__ == '__main__':
    main()
