#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GERADOR DOS GRAFICOS DA TELA DE SELECAO DE IDIOMA

Desenha (pixel a pixel) o fundo, os 3 cartoes com bandeira, o divisor e a setinha, e escreve:
    <projeto>/src/language_select_gfx.c     (tiles, mapas e paletas em C)
    <pasta deste script>/previa_tela_idioma.png   (imagem para voce ver o resultado)

Uso:   python gerar_tela_idioma.py [PASTA_DO_PROJETO]
       (sem argumento grava em ../../projeto, como no pacote)

Para mudar as cores, edite a secao CORES abaixo e rode de novo. Depois compile o projeto.

Limites do GBA que o desenho respeita: cada tile 8x8 usa UMA paleta de 16 cores (indice 0 = transparente);
os fundos compartilham 16 paletas:  0 = textos | 1,2,3 = cartoes | 4 = divisor | 5..14 = degrade do fundo.
"""
import os
import sys

# ----------------------------------------------------------------------------- CORES (R, G, B de 0 a 255)
FUNDO_TOPO = (10, 16, 48)        # degrade do fundo: de cima ...
FUNDO_BASE = (28, 56, 120)       # ... ate embaixo

TEXTO = {1: (255, 255, 255),     # branco
         2: (21, 48, 107),       # sombra azul do titulo / texto selecionado
         3: (169, 182, 224),     # texto do idioma nao selecionado
         4: (5, 10, 26),         # sombra escura
         5: (143, 160, 208),     # dica no rodape
         6: (255, 216, 74)}      # amarelo (reserva)

CARTAO_NORMAL = {1: (43, 63, 122),    # borda
                 2: (22, 38, 79),     # preenchimento (parte de cima)
                 3: (16, 28, 61),     # preenchimento (parte de baixo)
                 4: (34, 53, 107),    # brilho da borda de cima
                 5: (93, 113, 168),   # circulo do indicador
                 6: (16, 28, 61),     # miolo do indicador (igual ao fundo: invisivel)
                 7: (8, 15, 36)}      # sombra embaixo do cartao
CARTAO_SELEC = {1: (127, 178, 255),   # borda (esta cor pulsa no jogo)
                2: (45, 111, 224),
                3: (28, 74, 168),
                4: (150, 195, 255),   # brilho (tambem pulsa)
                5: (255, 255, 255),
                6: (255, 216, 74),    # miolo do indicador: amarelo
                7: (10, 26, 68)}
PULSO_BORDA = (207, 227, 255)         # a borda pulsa entre CARTAO_SELEC[1] e esta cor
PULSO_BRILHO = (230, 242, 255)
PULSO_MIOLO = (255, 255, 255)

DIVISOR = {1: (59, 79, 143), 2: (143, 180, 255), 3: (35, 52, 105)}
SETA = {1: (255, 216, 74), 2: (176, 120, 0), 3: (255, 244, 190)}   # setinha (sprite)

CONTORNO_BANDEIRA = (11, 20, 48)
BANDEIRAS = {   # indices 9.. de cada paleta de cartao
    "BR": {9: (0, 156, 59), 10: (255, 223, 0), 11: (0, 39, 118), 12: (255, 255, 255)},
    "USA": {9: (178, 34, 52), 10: (255, 255, 255), 11: (60, 59, 110)},
    "ESP": {9: (170, 21, 27), 10: (241, 191, 0), 11: (122, 15, 20)},
}
ORDEM = ["BR", "USA", "ESP"]

# ----------------------------------------------------------------------------- geometria (em pixels)
LARG, ALT = 240, 160
CARTAO_TILE_X, CARTAO_LARG_TILES = 2, 26          # 208 px de largura
CARTAO_LINHAS_TILE = [7, 11, 15]                  # linha (em tiles) onde cada cartao comeca; 3 tiles de altura
DIVISOR_Y = 51


def rgb5(c):
    r, g, b = c
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def pal16(mapa):
    p = [0] * 16
    for i, c in mapa.items():
        p[i] = rgb5(c)
    return p


# ----------------------------------------------------------------------------- tela de indices
idx = [[0] * LARG for _ in range(ALT)]          # indice de cor (0 = transparente) de cada pixel de BG1
banco = [[0] * (LARG // 8) for _ in range(ALT // 8)]


def pixel(x, y, i):
    if 0 <= x < LARG and 0 <= y < ALT:
        idx[y][x] = i


def dentro_arredondado(x, y, w, h, r):
    """True se (x,y) esta dentro do retangulo w x h com cantos de raio r."""
    if x < 0 or y < 0 or x >= w or y >= h:
        return False
    cx = min(max(x, r), w - 1 - r)
    cy = min(max(y, r), h - 1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r + r // 2


def desenhar_bandeira(nome, ox, oy):
    """Bandeira de 32x18 px com contorno; indices 8 (contorno) e 9.. (cores)."""
    W, H = 32, 18
    for y in range(H):
        for x in range(W):
            if x in (0, W - 1) or y in (0, H - 1):
                pixel(ox + x, oy + y, 8)
    iw, ih = W - 2, H - 2           # area interna 30x16
    for y in range(ih):
        for x in range(iw):
            c = 9
            if nome == "BR":
                cx, cy = (iw - 1) / 2, (ih - 1) / 2
                dx, dy = x - cx, y - cy
                if abs(dx) / 12.5 + abs(dy) / 6.6 <= 1:
                    c = 10                                   # losango amarelo
                if dx * dx + dy * dy <= 4.7 * 4.7:
                    c = 11                                   # circulo azul
                    if abs(dy + dx * 0.32 - 0.7) < 0.75:
                        c = 12                               # faixa branca
            elif nome == "USA":
                c = 9 if (y * 7 // ih) % 2 == 0 else 10     # 7 faixas
                if x < 13 and y < 9:
                    c = 11                                   # quadro azul
                    if (x in (2, 5, 8, 11) and y in (1, 5)) or (x in (3, 6, 9) and y in (3, 7)):
                        c = 10                               # "estrelas"
            elif nome == "ESP":
                c = 9 if (y < 4 or y >= ih - 4) else 10
                if 7 <= x <= 9 and 5 <= y <= 10:
                    c = 11 if (x + y) % 2 == 0 else 9        # brasao simplificado
            pixel(ox + 1 + x, oy + 1 + y, c)


def desenhar_cartao(k, nome):
    """Cartao de 208x24 px com bandeira, borda arredondada e indicador de selecao."""
    x0 = CARTAO_TILE_X * 8
    y0 = CARTAO_LINHAS_TILE[k] * 8
    W, H = CARTAO_LARG_TILES * 8, 22                  # corpo 208x22 (+ sombra de 1px e 1px livre)
    for y in range(H + 1):
        for x in range(W):
            if y == H:                                 # sombra (so sob a parte reta de baixo)
                if dentro_arredondado(x, H - 1, W, H, 5):
                    pixel(x0 + x, y0 + y, 7)
                continue
            if not dentro_arredondado(x, y, W, H, 5):
                continue
            borda = (not dentro_arredondado(x - 1, y, W, H, 5) or not dentro_arredondado(x + 1, y, W, H, 5) or
                     not dentro_arredondado(x, y - 1, W, H, 5) or not dentro_arredondado(x, y + 1, W, H, 5))
            if borda:
                c = 1
            elif y == 1:
                c = 4                                  # brilho de cima
            elif y < 11:
                c = 2
            elif y == 11:
                c = 2 if (x % 2 == 0) else 3           # mistura suave
            else:
                c = 3
            pixel(x0 + x, y0 + y, c)
    desenhar_bandeira(nome, x0 + 10, y0 + 2)
    # indicador de selecao (circulo) a direita
    cx, cy = x0 + W - 18, y0 + 11
    for y in range(-6, 7):
        for x in range(-6, 7):
            d2 = x * x + y * y
            if 16 <= d2 <= 30:
                pixel(cx + x, cy + y, 5)               # anel
            elif d2 <= 8:
                pixel(cx + x, cy + y, 6)               # miolo (amarelo quando selecionado)
    for ty in range(y0 // 8, y0 // 8 + 3):
        for tx in range(x0 // 8, x0 // 8 + CARTAO_LARG_TILES):
            banco[ty][tx] = 1 + k


def desenhar_divisor():
    ty = DIVISOR_Y // 8
    for x in range(24, 216):
        d = min(x - 24, 215 - x)
        pixel(x, DIVISOR_Y, 1 if d > 14 else 3)
    cx = 120
    for dy in range(-3, 4):
        for dx in range(-(3 - abs(dy)), 3 - abs(dy) + 1):
            pixel(cx + dx, DIVISOR_Y + dy, 2)
    for tx in range(LARG // 8):
        banco[ty][tx] = 4


for k, nome in enumerate(ORDEM):
    desenhar_cartao(k, nome)
desenhar_divisor()

# ----------------------------------------------------------------------------- tiles (sem repetir)
tiles, ids = [bytes(64)], {bytes(64): 0}
mapa = [[0] * (LARG // 8) for _ in range(ALT // 8)]
for ty in range(ALT // 8):
    for tx in range(LARG // 8):
        px = bytes(idx[ty * 8 + y][tx * 8 + x] for y in range(8) for x in range(8))
        if px not in ids:
            ids[px] = len(tiles)
            tiles.append(px)
        t = ids[px]
        mapa[ty][tx] = t | (banco[ty][tx] << 12 if t else 0)


def tile_u32(px):
    out = []
    for y in range(8):
        v = 0
        for x in range(8):
            v |= (px[y * 8 + x] & 0xF) << (4 * x)
        out.append(v)
    return out


# ----------------------------------------------------------------------------- paletas
def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


grad = [lerp(FUNDO_TOPO, FUNDO_BASE, i / 10) for i in range(11)]
pal_bg = [[0] * 16 for _ in range(16)]
pal_bg[0] = pal16({0: FUNDO_TOPO, **TEXTO})
pal_bg[0][0] = rgb5(FUNDO_TOPO)
for k, nome in enumerate(ORDEM):
    pal_bg[1 + k] = pal16({**CARTAO_NORMAL, 8: CONTORNO_BANDEIRA, **BANDEIRAS[nome]})
pal_bg[4] = pal16(DIVISOR)
for g in range(10):
    pal_bg[5 + g] = pal16({1: grad[g], 2: grad[g + 1]})
pal_sel = [pal16({**CARTAO_SELEC, 8: CONTORNO_BANDEIRA, **BANDEIRAS[nome]}) for nome in ORDEM]
pal_obj = pal16(SETA)

# degrade (BG2): tile 0 = cor 1, tile 1 = xadrez cor1/cor2
t_solido = bytes([1] * 64)
t_xadrez = bytes((1 if (x + y) % 2 == 0 else 2) for y in range(8) for x in range(8))
# seta triangular 8x16 apontando para a direita (sprite 8x16 = 2 tiles: cima e baixo)
def _seta():
    H = 15
    px = [[0] * 8 for _ in range(16)]
    for r in range(H):
        w = min(r, H - 1 - r)                      # 0..7: largura-1 desta linha
        for x in range(w + 1):
            c = 2                                  # miolo amarelo
            if x == w or x == 0 or r == 0 or r == H - 1:
                c = 1                              # contorno ambar
            elif x == 1 and r < 7:
                c = 3                              # brilho no lado de cima
            px[r][x] = c
    return bytes(px[y][x] for y in range(8) for x in range(8)) + bytes(px[y + 8][x] for y in range(8) for x in range(8))
t_seta = _seta()

# ----------------------------------------------------------------------------- escreve o C
def fmt_u32(nome, dados, por_linha=8):
    s = "const u32 %s[] =\n{\n" % nome
    for i in range(0, len(dados), por_linha):
        s += "\t" + ", ".join("0x%08X" % v for v in dados[i:i + por_linha]) + ",\n"
    return s + "};\n\n"


def fmt_u16(nome, dados, por_linha=12):
    s = "const u16 %s[] =\n{\n" % nome
    for i in range(0, len(dados), por_linha):
        s += "\t" + ", ".join("0x%04X" % v for v in dados[i:i + por_linha]) + ",\n"
    return s + "};\n\n"


def escrever(projeto):
    caminho = os.path.join(projeto, "src", "language_select_gfx.c")
    os.makedirs(os.path.dirname(caminho), exist_ok=True)
    c = ["// ARQUIVO GERADO por ferramentas/tela_idioma/gerar_tela_idioma.py - NAO EDITE A MAO.",
         "// (edite as cores/desenhos no script e rode de novo)", '#include "defines.h"', "",
         "#define LANG_GFX_TILE_COUNT %d" % len(tiles), ""]
    todos = []
    for t in tiles:
        todos += tile_u32(t)
    c.append(fmt_u32("gLangBg1Tiles", todos))
    c.append("const u32 gLangBg1TilesSize = %d;\n\n" % (len(tiles) * 32))
    flat = [mapa[y][x] for y in range(ALT // 8) for x in range(LARG // 8)]
    c.append(fmt_u16("gLangBg1Map", flat, 15))
    c.append(fmt_u32("gLangBg2Tiles", tile_u32(t_solido) + tile_u32(t_xadrez)))
    flat_pal = [v for bank in pal_bg for v in bank]
    c.append(fmt_u16("gLangPaletteBg", flat_pal, 16))
    c.append(fmt_u16("gLangCardPalSelected", [v for p in pal_sel for v in p], 16))
    c.append(fmt_u32("gLangArrowTiles", tile_u32(t_seta[:64]) + tile_u32(t_seta[64:])))
    c.append(fmt_u16("gLangArrowPalette", pal_obj, 16))
    c.append("// Cores do pulso do cartao selecionado (RGB555)\n")
    c.append("const u16 gLangPulse[3][2] =\n{\n\t{0x%04X, 0x%04X}, //Borda: normal, claro\n\t{0x%04X, 0x%04X}, //Brilho\n\t{0x%04X, 0x%04X}, //Miolo do indicador\n};\n" % (
        rgb5(CARTAO_SELEC[1]), rgb5(PULSO_BORDA), rgb5(CARTAO_SELEC[4]), rgb5(PULSO_BRILHO), rgb5(CARTAO_SELEC[6]), rgb5(PULSO_MIOLO)))
    open(caminho, "w", encoding="utf-8", newline="\n").write("\n".join(c))
    return caminho


def previa(caminho_png):
    from PIL import Image
    def cor(p, i):
        v = p[i]
        return ((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3)
    im = Image.new("RGB", (LARG, ALT))
    # fundo
    for ty in range(ALT // 8):
        b, par = 5 + ty // 2, ty % 2
        for y in range(8):
            for x in range(8):
                i = 1 if (par == 0 or (x + y) % 2 == 0) else 2
                for tx in range(LARG // 8):
                    im.putpixel((tx * 8 + x, ty * 8 + y), cor(pal_bg[b], i))
    # bg1 (o cartao do meio aparece selecionado)
    for y in range(ALT):
        for x in range(LARG):
            i = idx[y][x]
            if i:
                b = banco[y // 8][x // 8]
                p = pal_sel[b - 1] if b in (1, 2, 3) and (b - 1) == 1 else pal_bg[b]
                im.putpixel((x, y), cor(p, i))
    im.save(caminho_png)


if __name__ == "__main__":
    aqui = os.path.dirname(os.path.abspath(__file__))
    proj = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(aqui, "..", "..", "projeto"))
    print("gravado:", escrever(proj))
    try:
        previa(os.path.join(aqui, "previa_tela_idioma.png"))
        print("previa: previa_tela_idioma.png")
    except Exception as e:  # Pillow e opcional
        print("(sem previa:", e, ")")
    print("tiles unicos no fundo dos cartoes: %d de 512 possiveis" % len(tiles))
