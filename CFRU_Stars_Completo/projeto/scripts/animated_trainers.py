#!/usr/bin/env python3
"""
animated_trainers.py - registra automaticamente os treinadores animados (2 quadros)

Uso: coloque os PNGs em graphics/animated_trainers/ e rode (o a_makepy.bat ja faz isso):
    python scripts//animated_trainers.py

Cada PNG:
  - 64x128 (quadro 1 em cima, quadro 2 embaixo)  ou  128x64 (lado a lado - o script converte)
  - indexado com ate 16 cores (a cor 0 e a transparente)
  - nome livre. Opcional no nome:
        _106 ou _0x6A  -> numero do sprite do treinador (se faltar, o script reconhece pela imagem)
        _v16           -> velocidade: frames que cada quadro fica na tela (padrao 16; 60 = 1 segundo)
        _r2            -> toca 2 vezes e para (padrao: repete sem parar)
    Ex.: Brock.png   Brock_106.png   Brock_v12.png   Brock_0x6A_v20_r3.png

O script gera src/animated_trainer_pics_table.h (nao edite esse arquivo a mao).
So usa a biblioteca padrao do Python.
"""

import os
import re
import shutil
import struct
import sys
import zlib

SPRITE_FOLDER = os.path.join('graphics', 'animated_trainers')
BACKUP_FOLDER = 'animated_trainers_originais'  # fora de graphics/, para o build nao compilar
ERROR_FOLDER = 'animated_trainers_com_erro'    # PNGs com problema saem de graphics/ para nao quebrar o build
TABLE_HEADER = os.path.join('src', 'animated_trainer_pics_table.h')
GRITFLAGS = '-gzl -gB4 -fts -fh -pe16 -pzlz77 -gu8\n'
DEFAULT_SPEED = 16
DEFAULT_ROM = 'BPRE0.gba'
PTR_TRAINER_PIC_TABLE = 0x3473C  # literal usado por DecompressTrainerFrontPic
PTR_TRAINER_PAL_TABLE = 0x3474C
PTR_TRAINER_TABLE = 0x0FC00      # gTrainers (usado para mostrar quem usa cada sprite)
PTR_TRAINER_CLASS_NAMES = 0xD80A0 # nomes das classes de treinador
MATCH_THRESHOLD = 0.80  # parte dos pixels que precisa bater para reconhecer o treinador

try:
    sys.stdout.reconfigure(errors='replace')
except Exception:
    pass


def log(msg=''):
    print(msg)


# ---------------------------------------------------------------- PNG ----------

class PngError(Exception):
    pass


def read_png(path):
    """Le um PNG indexado (tipo de cor 3, 1/2/4/8 bits, sem entrelacamento)."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise PngError('nao e um arquivo PNG')
    pos = 8
    width = height = bitdepth = colortype = interlace = None
    palette = []
    idat = b''
    while pos < len(data):
        length, ctype = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b'IHDR':
            width, height, bitdepth, colortype, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
        elif ctype == b'PLTE':
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif ctype == b'IDAT':
            idat += chunk
        elif ctype == b'IEND':
            break
    if colortype != 3:
        raise PngError('a imagem nao esta indexada (precisa ser PNG com paleta de ate 16 cores)')
    if interlace:
        raise PngError('PNG entrelacado nao e suportado; salve sem entrelacamento')
    raw = zlib.decompress(idat)
    stride = (width * bitdepth + 7) // 8
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        ftype = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        bpp = 1
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        prev = line
        pixels = []
        for x in range(width):
            bit = x * bitdepth
            byte = line[bit // 8]
            shift = 8 - bitdepth - (bit % 8)
            pixels.append((byte >> shift) & ((1 << bitdepth) - 1))
        rows.append(pixels)
    return width, height, rows, palette


def write_png(path, rows, palette):
    """Grava PNG indexado de 4 bits (16 cores)."""
    height, width = len(rows), len(rows[0])
    raw = bytearray()
    for r in rows:
        raw.append(0)
        for x in range(0, width, 2):
            hi = r[x] & 0xF
            lo = (r[x + 1] & 0xF) if x + 1 < width else 0
            raw.append((hi << 4) | lo)
    pal = list(palette[:16]) + [(0, 0, 0)] * (16 - min(16, len(palette)))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 4, 3, 0, 0, 0))
    out += chunk(b'PLTE', b''.join(bytes(c) for c in pal))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(out)


# ---------------------------------------------------------------- ROM ----------

def lz77(rom, off):
    if off < 0 or off + 4 > len(rom) or rom[off] != 0x10:
        return None
    size = rom[off + 1] | (rom[off + 2] << 8) | (rom[off + 3] << 16)
    out = bytearray()
    src = off + 4
    while len(out) < size:
        if src >= len(rom):
            return None
        flags = rom[src]
        src += 1
        for bit in range(7, -1, -1):
            if len(out) >= size:
                break
            if flags & (1 << bit):
                b1, b2 = rom[src], rom[src + 1]
                src += 2
                length = (b1 >> 4) + 3
                disp = (((b1 & 0xF) << 8) | b2) + 1
                if disp > len(out):
                    return None
                for _ in range(length):
                    out.append(out[-disp])
            else:
                out.append(rom[src])
                src += 1
    return bytes(out[:size])


def gba_to_rgb(c):
    return ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)


def rgb5(c):
    return (c[0] >> 3, c[1] >> 3, c[2] >> 3)


def tiles_to_rows(tiles, width_tiles=8, height_tiles=8):
    rows = [[0] * (width_tiles * 8) for _ in range(height_tiles * 8)]
    for t in range(width_tiles * height_tiles):
        tx, ty = t % width_tiles, t // width_tiles
        for y in range(8):
            for x in range(8):
                b = tiles[t * 32 + y * 4 + x // 2]
                rows[ty * 8 + y][tx * 8 + x] = (b >> 4) if (x & 1) else (b & 0xF)
    return rows


class RomTrainers:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.rom = f.read()
        self.pic_table = self.ptr(PTR_TRAINER_PIC_TABLE)
        self.pal_table = self.ptr(PTR_TRAINER_PAL_TABLE)
        self._cache = {}

    def ptr(self, off):
        v = struct.unpack_from('<I', self.rom, off)[0]
        return v - 0x08000000 if 0x08000000 <= v < 0x0A000000 else -1

    def count(self):
        n = 0
        while True:
            e = self.pic_table + n * 8
            if e + 8 > len(self.rom):
                break
            p, size, tag = struct.unpack_from('<IHH', self.rom, e)
            if not (0x08000000 <= p < 0x0A000000) or size == 0 or n > 2000:
                break
            n += 1
        return n

    def text(self, off, maxlen):
        chars = {0: ' ', 0xAD: '.', 0xB8: ',', 0xAB: '!', 0xAC: '?', 0xAE: '-', 0xB4: "'", 0x53: 'PK', 0x54: 'MN', 0x1B: 'e'}
        out = ''
        for i in range(maxlen):
            c = self.rom[off + i]
            if c == 0xFF:
                break
            if 0xBB <= c <= 0xD4:
                out += chr(ord('A') + c - 0xBB)
            elif 0xD5 <= c <= 0xEE:
                out += chr(ord('a') + c - 0xD5)
            elif 0xA1 <= c <= 0xAA:
                out += chr(ord('0') + c - 0xA1)
            else:
                out += chars.get(c, '')
        return out.strip()

    def trainer_users(self):
        """{sprite: ['CLASSE NOME', ...]} a partir da tabela de treinadores da ROM."""
        users = {}
        table, classes = self.ptr(PTR_TRAINER_TABLE), self.ptr(PTR_TRAINER_CLASS_NAMES)
        if table < 0 or classes < 0:
            return users
        for i in range(1, 0x400):
            e = table + i * 0x28
            if e + 0x28 > len(self.rom):
                break
            party = struct.unpack_from('<I', self.rom, e + 0x24)[0]
            if not (0x08000000 <= party < 0x0A000000):
                break
            name = self.text(e + 4, 12)
            if not name:
                continue
            label = (self.text(classes + self.rom[e + 1] * 13, 13) + ' ' + name).strip()
            lst = users.setdefault(self.rom[e + 3], [])
            if label not in lst:
                lst.append(label)
        return users

    def pic(self, pic_id):
        if pic_id in self._cache:
            return self._cache[pic_id]
        res = None
        e = self.pic_table + pic_id * 8
        tiles = lz77(self.rom, self.ptr(e))
        pal_off = self.ptr(self.pal_table + pic_id * 8)
        pal_raw = lz77(self.rom, pal_off) if pal_off >= 0 else None
        if tiles and len(tiles) >= 0x800 and pal_raw and len(pal_raw) >= 32:
            pal = [gba_to_rgb(struct.unpack_from('<H', pal_raw, i * 2)[0]) for i in range(16)]
            res = (tiles_to_rows(tiles[:0x800]), pal)
        self._cache[pic_id] = res
        return res


def frame_similarity(rows_a, pal_a, rows_b, pal_b):
    same = total = 0
    for y in range(64):
        for x in range(64):
            ia, ib = rows_a[y][x], rows_b[y][x]
            if ia == 0 and ib == 0:
                continue
            total += 1
            if ia != 0 and ib != 0 and rgb5(pal_a[ia]) == rgb5(pal_b[ib]):
                same += 1
    return same / total if total else 0.0


# ---------------------------------------------------------------- main ---------

NAME_OPTS = re.compile(r'_(?:(0x[0-9A-Fa-f]+|\d+)|v(\d+)|r(\d+))(?=_|$)')


def parse_name(stem):
    pic_id = speed = plays = None
    for m in NAME_OPTS.finditer(stem):
        if m.group(1):
            pic_id = int(m.group(1), 16) if m.group(1).lower().startswith('0x') else int(m.group(1))
        elif m.group(2):
            speed = int(m.group(2))
        elif m.group(3):
            plays = int(m.group(3))
    return pic_id, speed, plays


def safe_stem(stem):
    import unicodedata
    s = unicodedata.normalize('NFD', stem)
    s = ''.join(ch for ch in s if unicodedata.category(ch) != 'Mn')
    s = re.sub(r'[^A-Za-z0-9_]', '_', s)
    if not s or not s[0].isalpha():
        s = 'Treinador_' + s
    return s


def find_rom_name():
    try:
        with open(os.path.join('scripts', 'make.py'), encoding='utf-8', errors='ignore') as f:
            m = re.search(r'^ROM_NAME\s*=\s*["\']([^"\']+)["\']', f.read(), re.M)
            if m:
                return m.group(1)
    except OSError:
        pass
    return DEFAULT_ROM


def backup(path):
    os.makedirs(BACKUP_FOLDER, exist_ok=True)
    dst = os.path.join(BACKUP_FOLDER, os.path.basename(path))
    if not os.path.exists(dst):
        shutil.copy2(path, dst)


def move_bad(path, fname):
    os.makedirs(ERROR_FOLDER, exist_ok=True)
    dst = os.path.join(ERROR_FOLDER, fname)
    if os.path.exists(dst):
        os.remove(dst)
    shutil.move(path, dst)
    log('       -> movido para %s/ (arrume e coloque de volta na pasta)' % ERROR_FOLDER)


def main():
    if not os.path.isdir('src') or not os.path.isdir('scripts'):
        log('ERRO: rode este script na raiz do projeto CFRU (onde ficam as pastas src e scripts).')
        return 1

    os.makedirs(SPRITE_FOLDER, exist_ok=True)
    flag_file = os.path.join(SPRITE_FOLDER, 'gritflags.txt')
    if not os.path.isfile(flag_file):
        with open(flag_file, 'w') as f:
            f.write(GRITFLAGS)
        log('Criado ' + flag_file)

    rom = None
    rom_name = find_rom_name()
    if os.path.isfile(rom_name):
        try:
            rom = RomTrainers(rom_name)
        except Exception as e:
            log('Aviso: nao consegui ler %s (%s). Coloque o numero do treinador no nome dos PNGs.' % (rom_name, e))
    else:
        log('Aviso: ROM %s nao encontrada. Coloque o numero do treinador no nome dos PNGs (ex.: Brock_106.png).' % rom_name)

    pngs = sorted(f for f in os.listdir(SPRITE_FOLDER) if f.lower().endswith('.png'))
    entries = []
    used_ids = {}
    errors = 0

    log('== Treinadores animados (%s) ==' % SPRITE_FOLDER)
    if not pngs:
        log('Nenhum PNG na pasta. Nenhum treinador animado.')

    for fname in pngs:
        path = os.path.join(SPRITE_FOLDER, fname)
        stem = fname[:-4]

        # 1) nome valido para o simbolo do grit
        new_stem = safe_stem(stem)
        if new_stem != stem:
            new_path = os.path.join(SPRITE_FOLDER, new_stem + '.png')
            if os.path.exists(new_path):
                log('ERRO %s: queria renomear para %s.png, mas ja existe. Renomeie a mao.' % (fname, new_stem))
                move_bad(path, fname)
                errors += 1
                continue
            os.rename(path, new_path)
            log('  %s renomeado para %s.png (nome precisa ser sem espacos/acentos e comecar com letra)' % (fname, new_stem))
            fname, stem, path = new_stem + '.png', new_stem, new_path

        # 2) ler e validar a imagem
        try:
            w, h, rows, pal = read_png(path)
        except (PngError, zlib.error, struct.error) as e:
            log('ERRO %s: %s' % (fname, e))
            move_bad(path, fname)
            errors += 1
            continue

        if (w, h) == (128, 64):  # lado a lado -> um embaixo do outro
            backup(path)
            rows = [r[:64] for r in rows] + [r[64:] for r in rows]
            write_png(path, rows, pal)
            w, h = 64, 128
            log('  %s: estava lado a lado (128x64); convertido para 64x128 (original em %s/)' % (fname, BACKUP_FOLDER))
        if (w, h) != (64, 128):
            log('ERRO %s: tamanho %dx%d. Precisa ser 64x128 (2 quadros de 64x64, um embaixo do outro).' % (fname, w, h))
            move_bad(path, fname)
            errors += 1
            continue
        max_index = max(max(r) for r in rows)
        if max_index > 15:
            log('ERRO %s: usa a cor numero %d; o GBA aceita no maximo 16 cores (0 a 15).' % (fname, max_index))
            move_bad(path, fname)
            errors += 1
            continue

        # 3) numero do treinador: do nome ou reconhecido pela imagem
        pic_id, speed, plays = parse_name(stem)
        how = 'pelo nome'
        if pic_id is None:
            if rom is None:
                log('ERRO %s: sem numero no nome e sem ROM para reconhecer. Use ex.: %s_106.png' % (fname, stem))
                move_bad(path, fname)
                errors += 1
                continue
            best, best_id = 0.0, None
            for i in range(rom.count()):
                p = rom.pic(i)
                if p is None:
                    continue
                s = frame_similarity(rows[:64], pal, p[0], p[1])
                if s > best:
                    best, best_id = s, i
            if best_id is None or best < MATCH_THRESHOLD:
                log('ERRO %s: nao reconheci o treinador (maior semelhanca %d%%). Coloque o numero no nome, ex.: %s_106.png'
                    % (fname, int(best * 100), stem))
                move_bad(path, fname)
                errors += 1
                continue
            pic_id = best_id
            how = 'reconhecido pela imagem (%d%% igual ao sprite %d)' % (int(best * 100), best_id)
            new_stem = '%s_%d' % (stem, pic_id)
            new_path = os.path.join(SPRITE_FOLDER, new_stem + '.png')
            if not os.path.exists(new_path):  # deixa o numero explicito no nome daqui em diante
                os.rename(path, new_path)
                log('  %s renomeado para %s.png (numero do treinador no nome)' % (fname, new_stem))
                fname, stem, path = new_stem + '.png', new_stem, new_path

        if pic_id in used_ids:
            log('ERRO %s: o treinador %d ja esta animado por %s. Deixe so um arquivo por treinador.'
                % (fname, pic_id, used_ids[pic_id]))
            move_bad(path, fname)
            errors += 1
            continue

        # 4) paleta: o jogo usa a paleta do treinador; arruma a ordem das cores se precisar
        if rom is not None and rom.pic(pic_id) is not None:
            rom_pal = rom.pic(pic_id)[1]
            rom5 = [rgb5(c) for c in rom_pal]
            used = sorted({v for r in rows for v in r})
            mapping = {0: 0}
            missing = []
            for idx in used:
                if idx == 0:
                    continue
                c = rgb5(pal[idx]) if idx < len(pal) else None
                if c is not None and c in rom5[1:]:
                    mapping[idx] = rom5.index(c, 1)
                else:
                    missing.append(idx)
            if missing:
                log('  Aviso %s: %d cor(es) nao existem na paleta do treinador %d; o jogo vai mostrar'
                    ' as cores da paleta dele nessas posicoes.' % (fname, len(missing), pic_id))
            elif any(mapping.get(i, i) != i for i in used):
                backup(path)
                rows = [[mapping.get(v, v) for v in r] for r in rows]
                write_png(path, rows, rom_pal)
                log('  %s: cores reorganizadas na ordem da paleta do treinador %d (original em %s/)'
                    % (fname, pic_id, BACKUP_FOLDER))

        speed = DEFAULT_SPEED if not speed else min(speed, 255)
        plays = 0 if plays is None else min(plays, 255)
        used_ids[pic_id] = fname
        entries.append((pic_id, stem, speed, plays))
        log('  OK  %-28s treinador %d (0x%X) %s | troca a cada %d frames | %s'
            % (fname, pic_id, pic_id, how, speed, 'repete sempre' if plays == 0 else 'toca %d vez(es)' % plays))

    # 5) gerar a tabela (ordenada pelo numero do treinador: a saida nao muda se nada mudou)
    entries.sort(key=lambda e: e[0])
    lines = [
        '// GERADO AUTOMATICAMENTE por scripts/animated_trainers.py - nao edite a mao.',
        '// Para mudar, coloque/remova PNGs em graphics/animated_trainers/ e compile de novo.',
        '#pragma once',
        '',
    ]
    for pic_id, stem, speed, plays in entries:
        lines.append('extern const u32 %sTiles[];' % stem)
    lines.append('')
    lines.append('#define ANIMATED_TRAINER_PICS_AUTO \\')
    for pic_id, stem, speed, plays in entries:
        lines.append('\t{%d, %sTiles, %d, %d}, \\' % (pic_id, stem, speed, plays))
    lines.append('')
    content = '\n'.join(lines) + '\n'
    old = None
    if os.path.isfile(TABLE_HEADER):
        with open(TABLE_HEADER, encoding='utf-8', errors='ignore') as f:
            old = f.read()
    if old != content:  # so reescreve se mudou (evita recompilar a toa)
        with open(TABLE_HEADER, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        c_file = os.path.join('src', 'animated_trainer_pics.c')
        if os.path.isfile(c_file):
            os.utime(c_file, None)  # o make.py so recompila .c alterados
    log('== %d treinador(es) animado(s), %d erro(s). Tabela: %s ==' % (len(entries), errors, TABLE_HEADER))
    return 0


if __name__ == '__main__':
    sys.exit(main())
