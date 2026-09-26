#!/usr/bin/env python3
"""
CFRU Backsprite Editor
======================
Ferramenta gráfica para adicionar novos backsprites de treinador ao
projeto CFRU-expansion (https://github.com/Shiny-Miner/CFRU-expansion)
de forma automática, seguindo exatamente o tutorial "Trainer Backsprites"
da documentação oficial (CFRU_Documentation.pdf, pág. 60-61):

    1. graphics/Backsprites/<arquivo>.png  (64x64 por frame, indexado, min 4 frames)
    2. include/constants/trainers.h        (nova constante TRAINER_BACK_PIC_X)
    3-10. src/Tables/back_pic_tables.c      (paleta, animação, coords, tabela de
           frames e template do sprite)

Requisitos: Python 3.9+, Pillow (pip install pillow)

Autor: gerado para uso pessoal do usuário.
"""

import os
import re
import sys
import shutil
import datetime
import traceback
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

try:
    from PIL import Image
except ImportError:
    Image = None

APP_TITLE = "CFRU Backsprite Editor"
APP_VERSION = "1.0"

# ---------------------------------------------------------------------------
# Cores / tema (visual escuro, profissional e simples)
# ---------------------------------------------------------------------------
BG = "#1e1f26"
BG_PANEL = "#262832"
BG_INPUT = "#2f3140"
FG = "#e8e8ec"
FG_MUTED = "#9a9bab"
ACCENT = "#5b8cff"
ACCENT_DARK = "#4570dd"
OK = "#4caf7d"
WARN = "#e0a23c"
ERR = "#e05c5c"
BORDER = "#3a3c4a"

REQUIRED_FILES = [
    os.path.join("include", "constants", "trainers.h"),
    os.path.join("src", "Tables", "back_pic_tables.c"),
    os.path.join("graphics", "Backsprites"),
]


# ---------------------------------------------------------------------------
# Lógica de edição (separada da UI para poder ser testada/entendida sozinha)
# ---------------------------------------------------------------------------
class BackspriteError(Exception):
    pass


class RepoNotFoundError(BackspriteError):
    pass


class AnchorNotFoundError(BackspriteError):
    pass


def validate_repo(path):
    """Confere se a pasta apontada é realmente uma cópia do CFRU-expansion."""
    if not path or not os.path.isdir(path):
        raise RepoNotFoundError("A pasta indicada não existe.")
    missing = [f for f in REQUIRED_FILES if not os.path.exists(os.path.join(path, f))]
    if missing:
        raise RepoNotFoundError(
            "Essa pasta não parece ser o repositório CFRU-expansion.\n"
            "Faltam: " + ", ".join(missing)
        )
    return True


def sanitize_name(raw):
    """Transforma o nome digitado pelo usuário em um identificador C válido
    (formato usado pelo próprio projeto, ex: 'Brendan', 'BigMo', 'PokeKid')."""
    cleaned = re.sub(r"[^A-Za-z0-9]", " ", raw).strip()
    if not cleaned:
        return ""
    parts = cleaned.split()
    name = "".join(p[0].upper() + p[1:] for p in parts if p)
    if name and name[0].isdigit():
        name = "T" + name
    return name


def to_upper_snake(name):
    """CamelCase -> UPPER_SNAKE (Brendan -> BRENDAN, BigMo -> BIG_MO)."""
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name)
    return s.upper()


def parse_existing_trainers(repo_path):
    """Lê include/constants/trainers.h e retorna [(nome_upper, id)] + próximo id livre."""
    path = os.path.join(repo_path, "include", "constants", "trainers.h")
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    entries = re.findall(r"#define\s+TRAINER_BACK_PIC_(\w+)\s+([0-9xXA-Fa-f]+)", text)
    ids = []
    for _, val in entries:
        try:
            ids.append(int(val, 0))
        except ValueError:
            pass
    next_id = (max(ids) + 1) if ids else 0
    return entries, next_id


def prepare_image(source_paths, frame_count, out_path, log=None):
    """Recebe 1 imagem (strip vertical) ou N imagens (uma por frame),
    valida tamanho, converte para paleta indexada de 16 cores garantindo
    que a cor transparente fique no ÍNDICE 0 da paleta (é assim que o GBA
    identifica transparência em sprites — não existe canal alfa no hardware,
    "transparente" = "cor do índice 0 da paleta")."""
    if Image is None:
        raise BackspriteError(
            "A biblioteca Pillow não está instalada.\nInstale com: pip install pillow"
        )

    def log_(msg):
        if log:
            log(msg)

    if len(source_paths) == 1:
        img = Image.open(source_paths[0]).convert("RGBA")
        w, h = img.size
        if w != 64:
            raise BackspriteError(f"A imagem precisa ter 64 pixels de largura (tem {w}).")
        if h != 64 * frame_count:
            raise BackspriteError(
                f"Para {frame_count} frames a imagem precisa ter {64*frame_count}px de "
                f"altura (tem {h}). Cada frame deve medir 64x64."
            )
        strip = img
    else:
        if len(source_paths) != frame_count:
            raise BackspriteError(
                f"Você selecionou {len(source_paths)} imagens, mas escolheu "
                f"{frame_count} frames. Selecione exatamente {frame_count}."
            )
        frames = []
        for p in source_paths:
            im = Image.open(p).convert("RGBA")
            if im.size != (64, 64):
                raise BackspriteError(f"'{os.path.basename(p)}' precisa ser 64x64 (é {im.size[0]}x{im.size[1]}).")
            frames.append(im)
        strip = Image.new("RGBA", (64, 64 * frame_count))
        for i, im in enumerate(frames):
            strip.paste(im, (0, i * 64))

    # --- Descobre qual é a cor "transparente" -------------------------------
    alpha = strip.getchannel("A")
    has_alpha_info = alpha.getextrema()[0] < 255  # existe algum pixel não 100% opaco?

    TRANSPARENT_KEY = (248, 0, 248)  # magenta clássico usado em sprites GBA

    if has_alpha_info:
        # Qualquer pixel com alpha < 128 vira a cor-chave de transparência.
        rgb = strip.convert("RGB")
        pixels = rgb.load()
        alpha_px = alpha.load()
        w, h = rgb.size
        transparent_found = False
        for y in range(h):
            for x in range(w):
                if alpha_px[x, y] < 128:
                    pixels[x, y] = TRANSPARENT_KEY
                    transparent_found = True
        key_color = TRANSPARENT_KEY if transparent_found else pixels[0, 0]
        log_("Transparência detectada pelo canal alfa da imagem original.")
    else:
        # Sem canal alfa usável: segue a convenção de sprites GBA e usa
        # a cor do pixel do canto superior esquerdo como cor-chave.
        rgb = strip.convert("RGB")
        key_color = rgb.getpixel((0, 0))
        log_(f"Imagem sem transparência (alfa). Usando a cor do canto (0,0) "
             f"{key_color} como cor transparente, por convenção.")

    # --- Quantiza para 16 cores, garantindo que a cor-chave fique no índice 0
    indexed = rgb.quantize(colors=16, method=Image.MEDIANCUT)
    pal = indexed.getpalette()[:16 * 3]
    pal_colors = [tuple(pal[i:i + 3]) for i in range(0, len(pal), 3)]

    # Acha o índice de paleta mais próximo da cor-chave
    def dist(c1, c2):
        return sum((a - b) ** 2 for a, b in zip(c1, c2))

    key_index = min(range(len(pal_colors)), key=lambda i: dist(pal_colors[i], key_color))

    if key_index != 0:
        # Troca o índice 0 com o índice da cor-chave, remapeando os pixels
        px = indexed.load()
        w, h = indexed.size
        for y in range(h):
            for x in range(w):
                v = px[x, y]
                if v == 0:
                    px[x, y] = key_index
                elif v == key_index:
                    px[x, y] = 0
        pal_colors[0], pal_colors[key_index] = pal_colors[key_index], pal_colors[0]
        flat = [c for rgb_ in pal_colors for c in rgb_]
        indexed.putpalette(flat)

    indexed.save(out_path, format="PNG")
    log_(f"Imagem processada (transparência no índice 0 da paleta) e salva em: {out_path}")
    return out_path


def backup_file(path):
    """Cria um backup .bak (apenas na primeira vez do dia/execução se já não existir)."""
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    bak_path = f"{path}.bak_{stamp}"
    shutil.copy2(path, bak_path)
    return bak_path


def _replace_once(text, anchor, insertion, before=True, label=""):
    """Insere `insertion` imediatamente antes (ou depois) da primeira
    ocorrência exata de `anchor`. Lança erro claro se não encontrar."""
    idx = text.find(anchor)
    if idx == -1:
        raise AnchorNotFoundError(
            f"Não encontrei o ponto de inserção para '{label}'.\n"
            f"O arquivo pode já ter sido editado manualmente ou é de uma versão "
            f"diferente do CFRU-expansion. Nada foi alterado."
        )
    if before:
        return text[:idx] + insertion + text[idx:]
    else:
        end = idx + len(anchor)
        return text[:end] + insertion + text[end:]


def apply_backsprite(repo_path, display_name, frame_count, coords=8, y_offset=4,
                      unbound=False, log=None):
    """Executa as 10 etapas do tutorial de forma automática.
    `display_name` já deve ser um identificador C válido (ex: 'BrendanNew')."""

    def log_(msg):
        if log:
            log(msg)

    name = display_name
    upper = to_upper_snake(name)
    const_name = f"TRAINER_BACK_PIC_{upper}"

    trainers_h = os.path.join(repo_path, "include", "constants", "trainers.h")
    back_c = os.path.join(repo_path, "src", "Tables", "back_pic_tables.c")

    with open(trainers_h, "r", encoding="utf-8", errors="replace") as f:
        th_text = f.read()
    if re.search(rf"\bTRAINER_BACK_PIC_{re.escape(upper)}\b", th_text):
        raise BackspriteError(f"Já existe um backsprite chamado '{upper}'. Escolha outro nome.")

    _, next_id = parse_existing_trainers(repo_path)

    # ---- 1) trainers.h : nova constante -----------------------------------
    anchor = "#define TRAINER_BACK_PIC_OLD_MAN \t\t\t5\n"
    if anchor not in th_text:
        # fallback: insere após a última constante TRAINER_BACK_PIC_ encontrada
        matches = list(re.finditer(r"#define\s+TRAINER_BACK_PIC_\w+\s+[0-9xXA-Fa-f]+\n", th_text))
        if not matches:
            raise AnchorNotFoundError("Não encontrei nenhuma constante TRAINER_BACK_PIC_ em trainers.h.")
        anchor = matches[-1].group(0)
        new_th = _replace_once(
            th_text, anchor,
            f"#define {const_name} \t\t\t{next_id}\n",
            before=False, label="trainers.h (nova constante)"
        )
    else:
        new_th = _replace_once(
            th_text, anchor,
            f"#define {const_name} \t\t\t{next_id}\n",
            before=False, label="trainers.h (nova constante)"
        )
    log_(f"[1/8] Nova constante: #define {const_name} {next_id}")

    # ---- back_pic_tables.c --------------------------------------------------
    with open(back_c, "r", encoding="utf-8", errors="replace") as f:
        bc = f.read()

    frame_macro = "FIVE_FRAME_TABLE" if frame_count == 5 else "FOUR_FRAME_TABLE"
    anim_ptr = "0x8239F44" if frame_count == 5 else "0x8239F54"

    # 2) PAL_DEFINE(name) -----------------------------------------------------
    bc = _replace_once(
        bc, "PAL_DEFINE(Tessy)\n",
        f"PAL_DEFINE({name})\n",
        before=False, label="lista de PAL_DEFINE"
    )
    log_(f"[2/8] PAL_DEFINE({name}) adicionado.")

    # 3) entrada na tabela de paletas ----------------------------------------
    if unbound:
        bc = _replace_once(
            bc, "\tPAL_ENTRY(TESSY, Tessy)\n",
            f"\tPAL_ENTRY({upper}, {name})\n",
            before=False, label="gTrainerBackPicPaletteTable (Unbound)"
        )
    else:
        bc = _replace_once(
            bc, "\tPAL_ENTRY(OLD_MAN, OldMan)\n",
            f"\tPAL_ENTRY({upper}, {name})\n",
            before=False, label="gTrainerBackPicPaletteTable"
        )
    log_(f"[3/8] PAL_ENTRY({upper}, {name}) adicionado à gTrainerBackPicPaletteTable.")

    # 4) macro do ponteiro de animação ---------------------------------------
    bc = _replace_once(
        bc, "#define gTrainerBackAnims_Tessy (const union AnimCmd* const*) 0x8239F44\n",
        f"#define gTrainerBackAnims_{name} (const union AnimCmd* const*) {anim_ptr}\n",
        before=False, label="macro gTrainerBackAnims_"
    )
    log_(f"[4/8] gTrainerBackAnims_{name} = {anim_ptr} ({frame_count} frames).")

    # 5) entrada em gTrainerBackAnimsPtrTable --------------------------------
    if unbound:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_TESSY] =\tgTrainerBackAnims_Tessy,\n",
            f"\t[{const_name}] = gTrainerBackAnims_{name},\n",
            before=False, label="gTrainerBackAnimsPtrTable (Unbound)"
        )
    else:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_OLD_MAN] = gTrainerBackAnims_OldMan,\n",
            f"\t[{const_name}] = gTrainerBackAnims_{name},\n",
            before=False, label="gTrainerBackAnimsPtrTable"
        )
    log_(f"[5/8] Entrada adicionada em gTrainerBackAnimsPtrTable.")

    # 6) entrada em gTrainerBackPicCoords -------------------------------------
    if unbound:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_TESSY] =             {.coords = 8, .y_offset = 4},\n",
            f"\t[{const_name}] = {{.coords = {coords}, .y_offset = {y_offset}}},\n",
            before=False, label="gTrainerBackPicCoords (Unbound)"
        )
    else:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_OLD_MAN] = \t{.coords = 8, .y_offset = 4},\n",
            f"\t[{const_name}] = \t{{.coords = {coords}, .y_offset = {y_offset}}},\n",
            before=False, label="gTrainerBackPicCoords"
        )
    log_(f"[6/8] Coordenadas adicionadas (coords={coords}, y_offset={y_offset}).")

    # 7) tabela de frames (FOUR_FRAME_TABLE / FIVE_FRAME_TABLE) --------------
    if unbound:
        bc = _replace_once(
            bc, "FIVE_FRAME_TABLE(Tessy)\n",
            f"{frame_macro}({name})\n",
            before=False, label="lista de FRAME_TABLE (Unbound)"
        )
    else:
        bc = _replace_once(
            bc, "FOUR_FRAME_TABLE(May)\n",
            f"{frame_macro}({name})\n",
            before=False, label="lista de FRAME_TABLE"
        )
    log_(f"[7/8] {frame_macro}({name}) adicionado (declara os frames de imagem).")

    # 8) entrada em gSpriteTemplateTable_TrainerBackSprites -------------------
    if unbound:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_TESSY] = BACK_TEMPLATE(Tessy)\n",
            f"\t[{const_name}] = BACK_TEMPLATE({name})\n",
            before=False, label="gSpriteTemplateTable_TrainerBackSprites (Unbound)"
        )
    else:
        bc = _replace_once(
            bc, "\t[TRAINER_BACK_PIC_OLD_MAN] = BACK_TEMPLATE(OldMan)\n",
            f"\t[{const_name}] = BACK_TEMPLATE({name})\n",
            before=False, label="gSpriteTemplateTable_TrainerBackSprites"
        )
    log_(f"[8/8] Entrada adicionada em gSpriteTemplateTable_TrainerBackSprites.")

    # ---- grava tudo só depois que TODAS as etapas deram certo -------------
    backup_file(trainers_h)
    backup_file(back_c)
    with open(trainers_h, "w", encoding="utf-8") as f:
        f.write(new_th)
    with open(back_c, "w", encoding="utf-8") as f:
        f.write(bc)

    return {
        "const_name": const_name,
        "id": next_id,
        "name": name,
    }


# ---------------------------------------------------------------------------
# Interface gráfica
# ---------------------------------------------------------------------------
class BackspriteEditorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f"{APP_TITLE}  v{APP_VERSION}")
        self.geometry("880x700")
        self.minsize(760, 560)
        self.configure(bg=BG)

        self.repo_path = tk.StringVar()
        self.frame_count = tk.IntVar(value=4)
        self.mode = tk.StringVar(value="strip")  # strip | frames
        self.trainer_name = tk.StringVar()
        self.coords = tk.IntVar(value=8)
        self.y_offset = tk.IntVar(value=4)
        self.unbound = tk.BooleanVar(value=False)
        self.strip_path = tk.StringVar()
        self.frame_paths = []

        self._setup_style()
        self._build_ui()

    # -- estilo ---------------------------------------------------------
    def _setup_style(self):
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        style.configure(".", background=BG, foreground=FG, font=("Segoe UI", 10))
        style.configure("TFrame", background=BG)
        style.configure("Panel.TFrame", background=BG_PANEL)
        style.configure("TLabel", background=BG, foreground=FG)
        style.configure("Panel.TLabel", background=BG_PANEL, foreground=FG)
        style.configure("Muted.TLabel", background=BG_PANEL, foreground=FG_MUTED)
        style.configure("Header.TLabel", background=BG, foreground=FG,
                         font=("Segoe UI", 17, "bold"))
        style.configure("SubHeader.TLabel", background=BG, foreground=FG_MUTED,
                         font=("Segoe UI", 10))
        style.configure("Section.TLabel", background=BG_PANEL, foreground=ACCENT,
                         font=("Segoe UI", 11, "bold"))

        style.configure("TEntry", fieldbackground=BG_INPUT, foreground=FG,
                        insertcolor=FG, bordercolor=BORDER, lightcolor=BORDER,
                        darkcolor=BORDER)
        style.configure("TSpinbox", fieldbackground=BG_INPUT, foreground=FG,
                        bordercolor=BORDER, arrowsize=12)
        style.configure("TCheckbutton", background=BG_PANEL, foreground=FG)
        style.map("TCheckbutton", background=[("active", BG_PANEL)])
        style.configure("TRadiobutton", background=BG_PANEL, foreground=FG)
        style.map("TRadiobutton", background=[("active", BG_PANEL)])

        style.configure("Accent.TButton", background=ACCENT, foreground="white",
                        font=("Segoe UI", 10, "bold"), padding=8, borderwidth=0)
        style.map("Accent.TButton", background=[("active", ACCENT_DARK), ("disabled", "#555")])
        style.configure("Ghost.TButton", background=BG_INPUT, foreground=FG,
                        padding=6, borderwidth=0)
        style.map("Ghost.TButton", background=[("active", BORDER)])

    # -- construção da UI -------------------------------------------------
    def _build_ui(self):
        outer = ttk.Frame(self, padding=(24, 20, 24, 16))
        outer.pack(fill="both", expand=True)

        header = ttk.Frame(outer)
        header.pack(fill="x")
        ttk.Label(header, text="CFRU Backsprite Editor", style="Header.TLabel").pack(anchor="w")
        ttk.Label(
            header,
            text="Adiciona um novo backsprite de treinador ao CFRU-expansion automaticamente, "
                 "seguindo o tutorial oficial.",
            style="SubHeader.TLabel",
        ).pack(anchor="w", pady=(2, 14))

        # --- Painel 1: repositório ----------------------------------------
        p1 = self._panel(outer, "1. Pasta do projeto CFRU-expansion")
        row = ttk.Frame(p1, style="Panel.TFrame")
        row.pack(fill="x", pady=(4, 2))
        entry = ttk.Entry(row, textvariable=self.repo_path, font=("Segoe UI", 10))
        entry.pack(side="left", fill="x", expand=True, ipady=4)
        ttk.Button(row, text="Selecionar pasta...", style="Ghost.TButton",
                   command=self._choose_repo).pack(side="left", padx=(8, 0))
        self.repo_status = ttk.Label(p1, text="Nenhuma pasta selecionada.",
                                      style="Muted.TLabel")
        self.repo_status.pack(anchor="w", pady=(4, 0))

        # --- Painel 2: dados do backsprite ----------------------------------
        p2 = self._panel(outer, "2. Novo backsprite")

        grid = ttk.Frame(p2, style="Panel.TFrame")
        grid.pack(fill="x", pady=(4, 6))

        ttk.Label(grid, text="Nome do treinador:", style="Panel.TLabel").grid(
            row=0, column=0, sticky="w", pady=4)
        ttk.Entry(grid, textvariable=self.trainer_name, width=28).grid(
            row=0, column=1, sticky="w", padx=(8, 24), ipady=3)
        ttk.Label(grid, text="(ex: BrendanNew, RivalGreen)", style="Muted.TLabel").grid(
            row=0, column=2, sticky="w")

        ttk.Label(grid, text="Quadros (frames):", style="Panel.TLabel").grid(
            row=1, column=0, sticky="w", pady=4)
        fr = ttk.Frame(grid, style="Panel.TFrame")
        fr.grid(row=1, column=1, sticky="w", padx=(8, 24))
        ttk.Radiobutton(fr, text="4 frames", value=4, variable=self.frame_count).pack(side="left")
        ttk.Radiobutton(fr, text="5 frames", value=5, variable=self.frame_count).pack(side="left", padx=(12, 0))

        ttk.Label(grid, text="coords / y_offset:", style="Panel.TLabel").grid(
            row=2, column=0, sticky="w", pady=4)
        co = ttk.Frame(grid, style="Panel.TFrame")
        co.grid(row=2, column=1, sticky="w", padx=(8, 24))
        ttk.Spinbox(co, from_=0, to=32, textvariable=self.coords, width=5).pack(side="left")
        ttk.Label(co, text="  /  ", style="Panel.TLabel").pack(side="left")
        ttk.Spinbox(co, from_=0, to=32, textvariable=self.y_offset, width=5).pack(side="left")
        ttk.Label(grid, text="(padrão do tutorial: 8 / 4)", style="Muted.TLabel").grid(
            row=2, column=2, sticky="w")

        ttk.Checkbutton(
            grid, text="Meu projeto é baseado em Pokémon Unbound "
                       "(inserir dentro dos blocos #ifdef UNBOUND)",
            variable=self.unbound,
        ).grid(row=3, column=0, columnspan=3, sticky="w", pady=(6, 0))

        # --- Painel 3: imagem -------------------------------------------
        p3 = self._panel(outer, "3. Imagem do backsprite (64x64 por quadro, PNG)")
        modefr = ttk.Frame(p3, style="Panel.TFrame")
        modefr.pack(fill="x", pady=(4, 6))
        ttk.Radiobutton(modefr, text="Uma imagem única (tira vertical com todos os quadros)",
                        value="strip", variable=self.mode,
                        command=self._refresh_img_mode).pack(anchor="w")
        ttk.Radiobutton(modefr, text="Uma imagem separada para cada quadro",
                        value="frames", variable=self.mode,
                        command=self._refresh_img_mode).pack(anchor="w", pady=(2, 0))

        self.img_area = ttk.Frame(p3, style="Panel.TFrame")
        self.img_area.pack(fill="x", pady=(6, 0))
        self._refresh_img_mode()

        # --- Botões (fixados embaixo ANTES do log, para nunca ficarem
        # escondidos se a janela for pequena) ---------------------------
        actions = ttk.Frame(outer)
        actions.pack(side="bottom", fill="x", pady=(14, 0))
        ttk.Button(actions, text="Aplicar e preparar para compilar",
                   style="Accent.TButton",
                   command=self._on_apply).pack(side="right", ipadx=10, ipady=4)
        ttk.Button(actions, text="Limpar formulário", style="Ghost.TButton",
                   command=self._clear_form).pack(side="right", padx=(0, 8))

        # --- Painel 4: log ------------------------------------------------
        p4 = self._panel(outer, "Log", expand=True)
        self.log_box = tk.Text(p4, height=8, bg="#14151a", fg="#c9e8c9",
                                insertbackground=FG, relief="flat",
                                font=("Consolas", 9), wrap="word")
        self.log_box.pack(fill="both", expand=True, pady=(4, 0))
        self.log_box.configure(state="disabled")

    def _panel(self, parent, title, expand=False):
        wrap = tk.Frame(parent, bg=BG_PANEL, highlightbackground=BORDER,
                         highlightthickness=1)
        wrap.pack(fill="both" if expand else "x", expand=expand, pady=(0, 14))
        inner = ttk.Frame(wrap, style="Panel.TFrame", padding=14)
        inner.pack(fill="both" if expand else "x", expand=expand)
        ttk.Label(inner, text=title, style="Section.TLabel").pack(anchor="w")
        return inner

    def _refresh_img_mode(self):
        for w in self.img_area.winfo_children():
            w.destroy()
        if self.mode.get() == "strip":
            row = ttk.Frame(self.img_area, style="Panel.TFrame")
            row.pack(fill="x")
            ttk.Entry(row, textvariable=self.strip_path).pack(
                side="left", fill="x", expand=True, ipady=3)
            ttk.Button(row, text="Escolher imagem...", style="Ghost.TButton",
                       command=self._choose_strip).pack(side="left", padx=(8, 0))
            ttk.Label(self.img_area,
                      text="A imagem deve ter 64px de largura e 64×N px de altura "
                           "(N = número de quadros escolhido acima).",
                      style="Muted.TLabel").pack(anchor="w", pady=(4, 0))
        else:
            self.frame_paths = []
            self.frames_label = ttk.Label(self.img_area, text="Nenhum quadro selecionado.",
                                           style="Muted.TLabel")
            self.frames_label.pack(anchor="w")
            ttk.Button(self.img_area, text="Escolher quadros (na ordem)...",
                       style="Ghost.TButton",
                       command=self._choose_frames).pack(anchor="w", pady=(6, 0))

    # -- ações -------------------------------------------------------------
    def _choose_repo(self):
        d = filedialog.askdirectory(title="Selecione a pasta do CFRU-expansion")
        if not d:
            return
        self.repo_path.set(d)
        try:
            validate_repo(d)
            self.repo_status.configure(text="✓ Repositório reconhecido corretamente.", foreground=OK)
        except BackspriteError as e:
            self.repo_status.configure(text=f"⚠ {e}", foreground=WARN)

    def _choose_strip(self):
        f = filedialog.askopenfilename(
            title="Selecione a imagem do backsprite",
            filetypes=[("Imagens PNG", "*.png"), ("Todas as imagens", "*.png;*.bmp;*.gif")])
        if f:
            self.strip_path.set(f)

    def _choose_frames(self):
        files = filedialog.askopenfilenames(
            title="Selecione os quadros na ordem correta (1, 2, 3, 4...)",
            filetypes=[("Imagens PNG", "*.png")])
        if files:
            self.frame_paths = list(files)
            self.frames_label.configure(
                text=f"{len(files)} quadro(s) selecionado(s): " +
                     ", ".join(os.path.basename(f) for f in files)
            )

    def _log(self, msg):
        self.log_box.configure(state="normal")
        self.log_box.insert("end", msg + "\n")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")
        self.update_idletasks()

    def _clear_form(self):
        self.trainer_name.set("")
        self.strip_path.set("")
        self.frame_paths = []
        self.frame_count.set(4)
        self.coords.set(8)
        self.y_offset.set(4)
        self._refresh_img_mode()

    def _on_apply(self):
        try:
            repo = self.repo_path.get().strip()
            validate_repo(repo)

            raw_name = self.trainer_name.get().strip()
            name = sanitize_name(raw_name)
            if not name:
                raise BackspriteError("Digite um nome válido para o treinador.")

            frames = self.frame_count.get()
            if self.mode.get() == "strip":
                if not self.strip_path.get().strip():
                    raise BackspriteError("Selecione a imagem do backsprite.")
                sources = [self.strip_path.get().strip()]
            else:
                if not self.frame_paths:
                    raise BackspriteError("Selecione as imagens dos quadros.")
                sources = self.frame_paths

            self._log(f"— Iniciando: {raw_name}  →  identificador '{name}'  ({frames} frames) —")

            out_png = os.path.join(repo, "graphics", "Backsprites", f"gTrainerBackPic_{name}.png")
            if os.path.exists(out_png):
                if not messagebox.askyesno(
                    APP_TITLE,
                    f"O arquivo {os.path.basename(out_png)} já existe. Substituir?"):
                    return

            prepare_image(sources, frames, out_png, log=self._log)

            result = apply_backsprite(
                repo, name, frames,
                coords=self.coords.get(), y_offset=self.y_offset.get(),
                unbound=self.unbound.get(), log=self._log,
            )

            self._log(
                f"\n✔ Concluído! '{result['const_name']}' (id {result['id']}) está pronto.\n"
                f"Basta compilar o projeto normalmente (make) — nada mais precisa ser editado."
            )
            messagebox.showinfo(
                APP_TITLE,
                f"Backsprite '{name}' adicionado com sucesso!\n\n"
                f"Constante: {result['const_name']} (ID {result['id']})\n"
                f"Imagem: graphics/Backsprites/gTrainerBackPic_{name}.png\n\n"
                f"O projeto já está pronto para ser compilado."
            )
        except BackspriteError as e:
            self._log(f"\n✖ ERRO: {e}")
            messagebox.showerror(APP_TITLE, str(e))
        except Exception as e:  # noqa - mostra qualquer erro inesperado sem travar
            self._log(f"\n✖ ERRO INESPERADO: {e}\n{traceback.format_exc()}")
            messagebox.showerror(APP_TITLE, f"Erro inesperado:\n{e}")


def main():
    if Image is None:
        print("AVISO: Pillow não encontrado. Instale com: pip install pillow")
    app = BackspriteEditorApp()
    app.mainloop()


if __name__ == "__main__":
    main()
