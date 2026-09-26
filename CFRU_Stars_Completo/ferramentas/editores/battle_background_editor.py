#!/usr/bin/env python3
"""
CFRU Battle Background Editor
=============================
Ferramenta gráfica para adicionar OU substituir os cenários (backgrounds)
de batalha do projeto CFRU-expansion
(https://github.com/Shiny-Miner/CFRU-expansion), deixando tudo pronto
para compilar.

IMPORTANTE - sobre a documentação:
A seção "Battle Terrain" do CFRU_Documentation.pdf (pág. 62-63) só ensina a
registrar o COMPORTAMENTO de jogo de um terreno (Camouflage / Secret Power /
Nature Power / Burmy) e assume que a imagem em si já foi inserida "usando o
tutorial aqui" - um link para um tópico externo do fórum PokeCommunity que
NÃO está incluído no PDF nem é acessível por esta ferramenta.

Em vez disso, este programa foi construído estudando diretamente o código-fonte
real do repositório (arquivos include/battle.h e
src/Tables/battle_background_tables.c, protegidos pela flag
NEW_BATTLE_BACKGROUNDS em src/config.h), que É o sistema realmente usado pelo
motor para carregar os cenários de batalha, e automatiza exatamente os passos
que esse sistema exige:

    1. graphics/Backgrounds/Battle_backgrounds/BG_<Nome>.png   (256x512, indexado)
    2. (opcional) .../Palette_Only/BG_<Nome>_Evening.png e _Night.png
    3. include/battle.h                  -> nova constante BATTLE_TERRAIN_X
    4. src/Tables/battle_background_tables.c -> externs + entradas nas 3
       tabelas (Dia / Entardecer / Noite)

Também é possível SUBSTITUIR um cenário já existente, sem tocar em nenhum
código: o programa descobre automaticamente qual arquivo de imagem
corresponde ao cenário escolhido e apenas o substitui.

Requisitos: Python 3.9+, Pillow (pip install pillow)
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

APP_TITLE = "CFRU Battle Background Editor"
APP_VERSION = "1.0"

# ---------------------------------------------------------------------------
# Tema visual (mesmo padrão do Backsprite Editor)
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

BATTLE_H = os.path.join("include", "battle.h")
TABLES_C = os.path.join("src", "Tables", "battle_background_tables.c")
BG_DIR = os.path.join("graphics", "Backgrounds", "Battle_backgrounds")
PAL_DIR = os.path.join(BG_DIR, "Palette_Only")

REQUIRED_FILES = [BATTLE_H, TABLES_C, BG_DIR]

IMG_W, IMG_H = 256, 512
PALETTE_COLORS = 48  # -pe48 no gritflags.txt da pasta


# ---------------------------------------------------------------------------
# Exceções
# ---------------------------------------------------------------------------
class BgError(Exception):
    pass


class RepoNotFoundError(BgError):
    pass


class AnchorNotFoundError(BgError):
    pass


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------
def validate_repo(path):
    if not path or not os.path.isdir(path):
        raise RepoNotFoundError("A pasta indicada não existe.")
    missing = [f for f in REQUIRED_FILES if not os.path.exists(os.path.join(path, f))]
    if missing:
        raise RepoNotFoundError(
            "Essa pasta não parece ser o repositório CFRU-expansion.\n"
            "Faltam: " + ", ".join(missing)
        )
    with open(os.path.join(path, "src", "config.h"), "r", encoding="utf-8", errors="replace") as f:
        cfg = f.read()
    m = re.search(r"^\s*//\s*#define\s+NEW_BATTLE_BACKGROUNDS", cfg, re.M)
    if m:
        raise BgError(
            "A flag NEW_BATTLE_BACKGROUNDS está desativada (comentada) em "
            "src/config.h. Ative-a (remova o // antes de #define) para poder "
            "usar cenários de batalha customizados."
        )
    return True


def sanitize_name(raw):
    cleaned = re.sub(r"[^A-Za-z0-9]", " ", raw).strip()
    if not cleaned:
        return ""
    parts = cleaned.split()
    name = "".join(p[0].upper() + p[1:] for p in parts if p)
    if name and name[0].isdigit():
        name = "T" + name
    return name


def to_upper_snake(name):
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name)
    return s.upper()


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        return f.read()


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def backup_file(path):
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    bak_path = f"{path}.bak_{stamp}"
    shutil.copy2(path, bak_path)
    return bak_path


def _insert_before(text, anchor, insertion, label=""):
    idx = text.find(anchor)
    if idx == -1:
        raise AnchorNotFoundError(
            f"Não encontrei o ponto de inserção para '{label}'.\n"
            f"O arquivo pode já ter sido editado manualmente ou é de uma "
            f"versão diferente do CFRU-expansion. Nada foi alterado."
        )
    return text[:idx] + insertion + text[idx:]


def _insert_after(text, anchor, insertion, label=""):
    idx = text.find(anchor)
    if idx == -1:
        raise AnchorNotFoundError(
            f"Não encontrei o ponto de inserção para '{label}'.\n"
            f"O arquivo pode já ter sido editado manualmente ou é de uma "
            f"versão diferente do CFRU-expansion. Nada foi alterado."
        )
    end = idx + len(anchor)
    return text[:end] + insertion + text[end:]


# ---------------------------------------------------------------------------
# Leitura dos cenários já existentes (para a lista de "substituir")
# ---------------------------------------------------------------------------
def parse_existing_backgrounds(repo_path):
    """Retorna [(const_name_sem_prefixo, base_symbol)] lendo a tabela do dia,
    e o próximo id livre (para o modo 'adicionar')."""
    bh_text = read_text(os.path.join(repo_path, BATTLE_H))
    # Considera apenas os ids dentro do bloco NEW_BATTLE_BACKGROUNDS (o mesmo
    # bloco onde a nova constante será inserida), ignorando o bloco separado
    # do Unbound e sentinelas como BATTLE_TERRAIN_RANDOM (0xFF).
    block_start = bh_text.find("#define BATTLE_TERRAIN_GRASS")
    block_end = bh_text.find("#endif", bh_text.find("#ifdef NEW_BATTLE_BACKGROUNDS"))
    if block_start == -1 or block_end == -1:
        block_text = bh_text
    else:
        block_text = bh_text[block_start:block_end]
    ids = [int(v, 0) for _, v in re.findall(
        r"#define\s+BATTLE_TERRAIN_(\w+)\s+([0-9xXA-Fa-f]+)", block_text)]
    next_id = (max(ids) + 1) if ids else 0

    tc_text = read_text(os.path.join(repo_path, TABLES_C))
    i = tc_text.find("const struct BattleBackground gBattleTerrainTable[]")
    j = tc_text.find("const struct BattleBackground gBattleTerrainTableEvening[]")
    if i == -1 or j == -1:
        raise AnchorNotFoundError("Não consegui localizar a tabela de cenários do dia.")
    day_block = tc_text[i:j]
    entries = re.findall(
        r"\[BATTLE_TERRAIN_(\w+)\]\s*=\s*\{[^}]*?\.tileset\s*=\s*(\w+?)Tiles",
        day_block, re.S)
    return entries, next_id


# ---------------------------------------------------------------------------
# Processamento de imagem
# ---------------------------------------------------------------------------
def prepare_background_image(source_path, out_path, log=None):
    if Image is None:
        raise BgError("A biblioteca Pillow não está instalada.\nInstale com: pip install pillow")

    def log_(msg):
        if log:
            log(msg)

    img = Image.open(source_path).convert("RGB")
    if img.size != (IMG_W, IMG_H):
        raise BgError(
            f"A imagem precisa ter exatamente {IMG_W}x{IMG_H} pixels "
            f"(recebi {img.size[0]}x{img.size[1]})."
        )
    indexed = img.quantize(colors=PALETTE_COLORS, method=Image.MEDIANCUT)
    indexed.save(out_path, format="PNG")
    log_(f"Imagem processada ({PALETTE_COLORS} cores) e salva em: {out_path}")
    return out_path


# ---------------------------------------------------------------------------
# Modo "adicionar novo cenário"
# ---------------------------------------------------------------------------
def add_new_background(repo_path, display_name, day_image, evening_image=None,
                        night_image=None, log=None):
    def log_(msg):
        if log:
            log(msg)

    name = display_name
    upper = to_upper_snake(name)
    const_name = f"BATTLE_TERRAIN_{upper}"
    base = f"BG_{name}"

    bh_path = os.path.join(repo_path, BATTLE_H)
    tc_path = os.path.join(repo_path, TABLES_C)

    bh_text = read_text(bh_path)
    if re.search(rf"\bBATTLE_TERRAIN_{re.escape(upper)}\b", bh_text):
        raise BgError(f"Já existe um cenário chamado '{upper}'. Escolha outro nome.")

    _, next_id = parse_existing_backgrounds(repo_path)

    # 1) Nova constante em include/battle.h -----------------------------------
    anchor_h = "#define BATTLE_TERRAIN_FOREST_PERADON\t0x1B\n"
    new_bh = _insert_after(
        bh_text, anchor_h,
        f"#define {const_name}\t0x{next_id:X}\n",
        label="include/battle.h (constante BATTLE_TERRAIN_)"
    )
    log_(f"[1/5] Nova constante: #define {const_name} 0x{next_id:X}")

    tc_text = read_text(tc_path)

    # 2) Externs (grupo do dia) -----------------------------------------------
    anchor_day_extern = "extern const u8 BG_Forest_PeradonPal[];\r\n\r\n// Evening (just palettes)"
    day_extern_ins = (
        f"extern const u8 {base}Tiles[];\r\n"
        f"extern const u8 {base}Map[];\r\n"
        f"extern const u8 {base}Pal[];\r\n"
    )
    tc_text = _insert_after(
        tc_text, "extern const u8 BG_Forest_PeradonPal[];\r\n",
        day_extern_ins, label="declarações extern (grupo Dia)"
    )
    log_(f"[2/5] Declarações extern de {base}Tiles/Map/Pal adicionadas.")

    evening_pal_symbol = f"{base}Pal"
    night_pal_symbol = f"{base}Pal"

    if evening_image:
        evening_pal_symbol = f"{base}_EveningPal"
        tc_text = _insert_after(
            tc_text, "extern const u8 BG_Forest_Peradon_EveningPal[];\r\n",
            f"extern const u8 {evening_pal_symbol}[];\r\n",
            label="declaração extern (paleta Entardecer)"
        )
        log_(f"[3/5] Paleta de Entardecer própria: {evening_pal_symbol}")
    else:
        log_("[3/5] Sem imagem de Entardecer: usando a paleta do dia.")

    if night_image:
        night_pal_symbol = f"{base}_NightPal"
        tc_text = _insert_after(
            tc_text, "extern const u8 BG_Forest_Peradon_NightPal[];\r\n",
            f"extern const u8 {night_pal_symbol}[];\r\n",
            label="declaração extern (paleta Noite)"
        )
        log_(f"[4/5] Paleta de Noite própria: {night_pal_symbol}")
    else:
        log_("[4/5] Sem imagem de Noite: usando a paleta do dia.")

    # 3) Entradas nas 3 tabelas ------------------------------------------------
    def entry_block(const_name, base, pal_symbol):
        return (
            f"\t[{const_name}] =\r\n"
            f"\t{{\r\n"
            f"\t\t.tileset = {base}Tiles,\r\n"
            f"\t\t.tilemap = {base}Map,\r\n"
            f"\t\t.entryTileset = gBattleTerrainAnimTiles_TallGrass,\r\n"
            f"\t\t.entryTilemap = gBattleTerrainAnimTilemap_TallGrass,\r\n"
            f"\t\t.palette = {pal_symbol},\r\n"
            f"\t}},\r\n"
        )

    tc_text = _insert_before(
        tc_text,
        "};\r\n\r\nconst struct BattleBackground gBattleTerrainTableEvening[]",
        entry_block(const_name, base, f"{base}Pal"),
        label="tabela do Dia (gBattleTerrainTable)"
    )
    tc_text = _insert_before(
        tc_text,
        "};\r\n\r\nconst struct BattleBackground gBattleTerrainTableNight[]",
        entry_block(const_name, base, evening_pal_symbol),
        label="tabela de Entardecer (gBattleTerrainTableEvening)"
    )
    tc_text = _insert_before(
        tc_text,
        "};\r\n#endif\t",
        entry_block(const_name, base, night_pal_symbol),
        label="tabela de Noite (gBattleTerrainTableNight)"
    )
    log_(f"[5/5] Entradas adicionadas em gBattleTerrainTable / Evening / Night.")

    # --- Salva as imagens processadas -----------------------------------------
    os.makedirs(os.path.join(repo_path, BG_DIR), exist_ok=True)
    os.makedirs(os.path.join(repo_path, PAL_DIR), exist_ok=True)

    day_out = os.path.join(repo_path, BG_DIR, f"{base}.png")
    prepare_background_image(day_image, day_out, log=log_)

    if evening_image:
        ev_out = os.path.join(repo_path, PAL_DIR, f"{base}_Evening.png")
        prepare_background_image(evening_image, ev_out, log=log_)
    if night_image:
        nt_out = os.path.join(repo_path, PAL_DIR, f"{base}_Night.png")
        prepare_background_image(night_image, nt_out, log=log_)

    # --- Só grava o código depois que TUDO deu certo --------------------------
    backup_file(bh_path)
    backup_file(tc_path)
    write_text(bh_path, new_bh)
    write_text(tc_path, tc_text)

    return {"const_name": const_name, "id": next_id, "base": base}


# ---------------------------------------------------------------------------
# Modo "substituir cenário existente" (não mexe em código nenhum)
# ---------------------------------------------------------------------------
def replace_background(repo_path, terrain_upper, day_image, evening_image=None,
                        night_image=None, log=None):
    def log_(msg):
        if log:
            log(msg)

    entries, _ = parse_existing_backgrounds(repo_path)
    entry_map = dict(entries)
    if terrain_upper not in entry_map:
        raise BgError(f"Cenário '{terrain_upper}' não encontrado.")
    base = entry_map[terrain_upper]

    shared_with = [k for k, v in entries if v == base and k != terrain_upper]
    if shared_with:
        log_(
            f"⚠ Atenção: a imagem '{base}.png' também é usada por: "
            f"{', '.join(shared_with)}. Substituí-la vai alterar todos eles também."
        )

    day_out = os.path.join(repo_path, BG_DIR, f"{base}.png")
    if not os.path.exists(day_out):
        raise BgError(f"Não encontrei o arquivo original {day_out} para substituir.")
    backup_file(day_out)
    prepare_background_image(day_image, day_out, log=log_)
    log_(f"[1/1] Imagem do cenário '{terrain_upper}' substituída ({base}.png).")

    replaced_extra = []
    if evening_image:
        ev_path = os.path.join(repo_path, PAL_DIR, f"{base}_Evening.png")
        if os.path.exists(ev_path):
            backup_file(ev_path)
            prepare_background_image(evening_image, ev_path, log=log_)
            replaced_extra.append("Entardecer")
        else:
            log_(
                f"ℹ '{terrain_upper}' não tem uma paleta de Entardecer própria "
                f"(usa a mesma do dia) — nada extra para substituir aqui."
            )
    if night_image:
        nt_path = os.path.join(repo_path, PAL_DIR, f"{base}_Night.png")
        if os.path.exists(nt_path):
            backup_file(nt_path)
            prepare_background_image(night_image, nt_path, log=log_)
            replaced_extra.append("Noite")
        else:
            log_(
                f"ℹ '{terrain_upper}' não tem uma paleta de Noite própria "
                f"(usa a mesma do dia) — nada extra para substituir aqui."
            )

    return {"base": base, "extra": replaced_extra, "shared_with": shared_with}


# ---------------------------------------------------------------------------
# Interface gráfica
# ---------------------------------------------------------------------------
class BattleBgEditorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f"{APP_TITLE}  v{APP_VERSION}")
        self.geometry("900x760")
        self.minsize(780, 600)
        self.configure(bg=BG)

        self.repo_path = tk.StringVar()
        self.mode = tk.StringVar(value="add")  # add | replace
        self.trainer_name = tk.StringVar()  # nome do novo cenário
        self.selected_existing = tk.StringVar()
        self.day_path = tk.StringVar()
        self.evening_path = tk.StringVar()
        self.night_path = tk.StringVar()

        self._existing = []  # [(UPPER, base)]

        self._setup_style()
        self._build_ui()

    # -- estilo (idêntico ao Backsprite Editor) --------------------------
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
        style.configure("TCombobox", fieldbackground=BG_INPUT, foreground=FG,
                        background=BG_INPUT)
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

    def _panel(self, parent, title, expand=False):
        wrap = tk.Frame(parent, bg=BG_PANEL, highlightbackground=BORDER,
                         highlightthickness=1)
        wrap.pack(fill="both" if expand else "x", expand=expand, pady=(0, 14))
        inner = ttk.Frame(wrap, style="Panel.TFrame", padding=14)
        inner.pack(fill="both" if expand else "x", expand=expand)
        ttk.Label(inner, text=title, style="Section.TLabel").pack(anchor="w")
        return inner

    # -- construção da UI -------------------------------------------------
    def _build_ui(self):
        outer = ttk.Frame(self, padding=(24, 20, 24, 16))
        outer.pack(fill="both", expand=True)

        header = ttk.Frame(outer)
        header.pack(fill="x")
        ttk.Label(header, text="CFRU Battle Background Editor", style="Header.TLabel").pack(anchor="w")
        ttk.Label(
            header,
            text="Adiciona um novo cenário de batalha, ou substitui um já existente, "
                 "no CFRU-expansion.",
            style="SubHeader.TLabel",
        ).pack(anchor="w", pady=(2, 14))

        # Botões fixados embaixo antes do log, para nunca ficarem escondidos
        actions = ttk.Frame(outer)
        actions.pack(side="bottom", fill="x", pady=(14, 0))
        ttk.Button(actions, text="Aplicar e preparar para compilar",
                   style="Accent.TButton",
                   command=self._on_apply).pack(side="right", ipadx=10, ipady=4)
        ttk.Button(actions, text="Limpar formulário", style="Ghost.TButton",
                   command=self._clear_form).pack(side="right", padx=(0, 8))

        # --- Painel 1: repositório -------------------------------------
        p1 = self._panel(outer, "1. Pasta do projeto CFRU-expansion")
        row = ttk.Frame(p1, style="Panel.TFrame")
        row.pack(fill="x", pady=(4, 2))
        ttk.Entry(row, textvariable=self.repo_path, font=("Segoe UI", 10)).pack(
            side="left", fill="x", expand=True, ipady=4)
        ttk.Button(row, text="Selecionar pasta...", style="Ghost.TButton",
                   command=self._choose_repo).pack(side="left", padx=(8, 0))
        self.repo_status = ttk.Label(p1, text="Nenhuma pasta selecionada.",
                                      style="Muted.TLabel")
        self.repo_status.pack(anchor="w", pady=(4, 0))

        # --- Painel 2: modo -----------------------------------------------
        p2 = self._panel(outer, "2. O que você quer fazer?")
        modefr = ttk.Frame(p2, style="Panel.TFrame")
        modefr.pack(fill="x", pady=(4, 6))
        ttk.Radiobutton(modefr, text="Adicionar um cenário NOVO",
                        value="add", variable=self.mode,
                        command=self._refresh_mode).pack(anchor="w")
        ttk.Radiobutton(modefr, text="Substituir um cenário JÁ EXISTENTE",
                        value="replace", variable=self.mode,
                        command=self._refresh_mode).pack(anchor="w", pady=(2, 0))

        self.mode_area = ttk.Frame(p2, style="Panel.TFrame")
        self.mode_area.pack(fill="x", pady=(8, 0))

        # --- Painel 3: imagens ----------------------------------------------
        p3 = self._panel(outer, f"3. Imagens ({IMG_W}x{IMG_H} px, PNG)")
        self._img_row(p3, "Dia (obrigatória):", self.day_path, self._choose_day)
        self._img_row(p3, "Entardecer (opcional):", self.evening_path, self._choose_evening)
        self._img_row(p3, "Noite (opcional):", self.night_path, self._choose_night)
        ttk.Label(
            p3,
            text="Se você não fornecer imagens de Entardecer/Noite, a paleta do dia "
                 "será reaproveitada nesses horários (mesmo comportamento usado por "
                 "vários cenários originais do jogo).",
            style="Muted.TLabel", wraplength=760, justify="left",
        ).pack(anchor="w", pady=(6, 0))

        # --- Painel 4: log --------------------------------------------------
        p4 = self._panel(outer, "Log", expand=True)
        self.log_box = tk.Text(p4, height=8, bg="#14151a", fg="#c9e8c9",
                                insertbackground=FG, relief="flat",
                                font=("Consolas", 9), wrap="word")
        self.log_box.pack(fill="both", expand=True, pady=(4, 0))
        self.log_box.configure(state="disabled")

        self._refresh_mode()

    def _img_row(self, parent, label, var, cmd):
        row = ttk.Frame(parent, style="Panel.TFrame")
        row.pack(fill="x", pady=4)
        ttk.Label(row, text=label, style="Panel.TLabel", width=22).pack(side="left")
        ttk.Entry(row, textvariable=var).pack(side="left", fill="x", expand=True, ipady=3)
        ttk.Button(row, text="Escolher...", style="Ghost.TButton", command=cmd).pack(
            side="left", padx=(8, 0))

    def _refresh_mode(self):
        for w in self.mode_area.winfo_children():
            w.destroy()
        if self.mode.get() == "add":
            grid = ttk.Frame(self.mode_area, style="Panel.TFrame")
            grid.pack(fill="x")
            ttk.Label(grid, text="Nome do cenário:", style="Panel.TLabel").grid(
                row=0, column=0, sticky="w")
            ttk.Entry(grid, textvariable=self.trainer_name, width=28).grid(
                row=0, column=1, sticky="w", padx=(8, 24), ipady=3)
            ttk.Label(grid, text="(ex: SnowField, HauntedForest)", style="Muted.TLabel").grid(
                row=0, column=2, sticky="w")
        else:
            top = ttk.Frame(self.mode_area, style="Panel.TFrame")
            top.pack(fill="x")
            ttk.Label(top, text="Cenário existente:", style="Panel.TLabel").pack(side="left")
            self.existing_combo = ttk.Combobox(
                top, textvariable=self.selected_existing, state="readonly", width=40)
            self.existing_combo.pack(side="left", padx=(8, 8))
            ttk.Button(top, text="Recarregar lista", style="Ghost.TButton",
                       command=self._load_existing).pack(side="left")
            if self.repo_path.get():
                self._load_existing()

    def _load_existing(self):
        try:
            validate_repo(self.repo_path.get())
            entries, _ = parse_existing_backgrounds(self.repo_path.get())
            self._existing = entries
            labels = [f"{u}   (imagem: {b}.png)" for u, b in entries]
            self.existing_combo["values"] = labels
            if labels:
                self.existing_combo.current(0)
            self._log(f"{len(entries)} cenário(s) encontrado(s) no projeto.")
        except BgError as e:
            messagebox.showerror(APP_TITLE, str(e))

    # -- ações de arquivo ---------------------------------------------------
    def _choose_repo(self):
        d = filedialog.askdirectory(title="Selecione a pasta do CFRU-expansion")
        if not d:
            return
        self.repo_path.set(d)
        try:
            validate_repo(d)
            self.repo_status.configure(text="✓ Repositório reconhecido corretamente.", foreground=OK)
            if self.mode.get() == "replace":
                self._load_existing()
        except BgError as e:
            self.repo_status.configure(text=f"⚠ {e}", foreground=WARN)

    def _choose_img(self, var):
        f = filedialog.askopenfilename(
            title=f"Selecione a imagem ({IMG_W}x{IMG_H})",
            filetypes=[("Imagens PNG", "*.png")])
        if f:
            var.set(f)

    def _choose_day(self):
        self._choose_img(self.day_path)

    def _choose_evening(self):
        self._choose_img(self.evening_path)

    def _choose_night(self):
        self._choose_img(self.night_path)

    def _log(self, msg):
        self.log_box.configure(state="normal")
        self.log_box.insert("end", msg + "\n")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")
        self.update_idletasks()

    def _clear_form(self):
        self.trainer_name.set("")
        self.day_path.set("")
        self.evening_path.set("")
        self.night_path.set("")

    def _on_apply(self):
        try:
            repo = self.repo_path.get().strip()
            validate_repo(repo)

            if not self.day_path.get().strip():
                raise BgError("Selecione ao menos a imagem do Dia.")

            day_img = self.day_path.get().strip()
            eve_img = self.evening_path.get().strip() or None
            night_img = self.night_path.get().strip() or None

            if self.mode.get() == "add":
                raw_name = self.trainer_name.get().strip()
                name = sanitize_name(raw_name)
                if not name:
                    raise BgError("Digite um nome válido para o novo cenário.")
                self._log(f"— Adicionando novo cenário: '{raw_name}' → '{name}' —")
                result = add_new_background(
                    repo, name, day_img, eve_img, night_img, log=self._log)
                self._log(
                    f"\n✔ Concluído! '{result['const_name']}' (id 0x{result['id']:X}) "
                    f"está pronto. Basta compilar o projeto (make)."
                )
                messagebox.showinfo(
                    APP_TITLE,
                    f"Cenário '{name}' adicionado com sucesso!\n\n"
                    f"Constante: {result['const_name']}\n"
                    f"Imagem: graphics/Backgrounds/Battle_backgrounds/BG_{name}.png\n\n"
                    f"O projeto já está pronto para compilar."
                )
            else:
                if not self._existing:
                    raise BgError("Carregue a lista de cenários existentes primeiro.")
                label = self.selected_existing.get()
                if not label:
                    raise BgError("Selecione um cenário para substituir.")
                terrain_upper = label.split()[0]
                self._log(f"— Substituindo cenário existente: '{terrain_upper}' —")
                result = replace_background(
                    repo, terrain_upper, day_img, eve_img, night_img, log=self._log)
                self._log(f"\n✔ Concluído! Nenhum arquivo de código foi alterado — "
                          f"apenas a(s) imagem(ns) foi(ram) substituída(s).")
                msg = f"Cenário '{terrain_upper}' substituído com sucesso!"
                if result["shared_with"]:
                    msg += (f"\n\nAtenção: esta imagem também é usada por: "
                            f"{', '.join(result['shared_with'])}.")
                messagebox.showinfo(APP_TITLE, msg)
        except BgError as e:
            self._log(f"\n✖ ERRO: {e}")
            messagebox.showerror(APP_TITLE, str(e))
        except Exception as e:  # noqa
            self._log(f"\n✖ ERRO INESPERADO: {e}\n{traceback.format_exc()}")
            messagebox.showerror(APP_TITLE, f"Erro inesperado:\n{e}")


def main():
    if Image is None:
        print("AVISO: Pillow não encontrado. Instale com: pip install pillow")
    app = BattleBgEditorApp()
    app.mainloop()


if __name__ == "__main__":
    main()
