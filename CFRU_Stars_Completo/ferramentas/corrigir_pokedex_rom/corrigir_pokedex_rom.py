#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Corrige o texto quebrado "Seen/Owned" da Pokedex direto na ROM base.

O patch de Pokedex que ja vem instalado na sua ROM (regiao 0x09600000) busca
esses textos nos enderecos 0x087207A4 e 0x087207AC, onde nao ha texto. Este
programa troca so esses dois ponteiros pelos textos originais do FireRed:
    0x087207A4 -> 0x08415DC4  ("Seen:")
    0x087207AC -> 0x08415DCA  ("Owned:")

Uso: coloque na mesma pasta da ROM e rode
    python corrigir_pokedex_rom.py BPRE0.gba
(sem argumento, ele usa BPRE0.gba). Uma copia .bak da ROM e criada antes.
"""
import os, shutil, struct, sys

PATCHES = [  # (offset na ROM, valor antigo, valor novo)
    (0x160D968, 0x087207A4, 0x08415DC4),
    (0x160D974, 0x087207AC, 0x08415DCA),
]

def main():
    rom = sys.argv[1] if len(sys.argv) > 1 else "BPRE0.gba"
    if not os.path.isfile(rom):
        print("ERRO: ROM nao encontrada:", rom)
        return 1
    data = bytearray(open(rom, "rb").read())
    todo = []
    for off, old, new in PATCHES:
        cur = struct.unpack_from("<I", data, off)[0]
        if cur == new:
            continue
        if cur != old:
            print("ERRO: em 0x%X esperava 0x%08X e achei 0x%08X." % (off, old, cur))
            print("Esta ROM e diferente da analisada. Nada foi alterado.")
            return 1
        todo.append((off, new))
    if not todo:
        print("A ROM ja esta corrigida. Nada foi alterado.")
        return 0
    shutil.copy2(rom, rom + ".bak")
    for off, new in todo:
        struct.pack_into("<I", data, off, new)
    open(rom, "wb").write(data)
    print("Pokedex corrigida em", rom, "(copia em %s.bak)" % rom)
    print("Compile o projeto de novo para a correcao entrar no jogo.")
    return 0

if __name__ == "__main__":
    code = main()
    if os.name == "nt":
        input("\nPressione Enter para fechar...")
    sys.exit(code)
