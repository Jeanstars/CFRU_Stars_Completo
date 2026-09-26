#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Desinstalador do Character Swap System (troca de personagem) para CFRU-expansion.

Coloque este arquivo na RAIZ do projeto (a mesma pasta de "src", "include", "hooks")
e rode:
    python desinstalar_character_swap.py                 -> remove tudo e volta o PC para 25 boxes
    python desinstalar_character_swap.py --manter-boxes  -> remove tudo, mas mantem o PC com 10 boxes
    python desinstalar_character_swap.py --simular       -> so mostra o que seria feito, sem alterar nada
    python desinstalar_character_swap.py --restaurar PASTA_DE_BACKUP -> desfaz a desinstalacao

Antes de alterar qualquer coisa, uma copia de todos os arquivos envolvidos e guardada
numa pasta "backup_character_swap_DATA_HORA".
"""

import os
import re
import shutil
import sys
from datetime import datetime

# Arquivos criados pela mecanica: sao apagados
NEW_FILES = [
    "src/character_swap.c",
    "assembly/overworld_scripts/character_swap.s",
    "strings/character_swap.string",
]

# Arquivos do CFRU que a mecanica modificou: so as alteracoes sao desfeitas
MODIFIED_FILES = [
    "src/config.h",
    "hooks",
    "src/Tables/item_tables.c",
    "include/new/item_tables.h",
    "src/start_menu.c",
    "include/pokemon_storage_system.h",
    "src/pokemon_storage_system.c",
]

BOX_FILES = ["include/pokemon_storage_system.h", "src/pokemon_storage_system.c"]


# ----------------------------------------------------------------------------------------
# Leitura/escrita preservando a codificacao e as quebras de linha originais (CRLF/LF)
# ----------------------------------------------------------------------------------------

def read(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


NL = r"\r?\n"  # Quebra de linha LF ou CRLF


# ----------------------------------------------------------------------------------------
# Regras de remocao. Cada regra: (descricao, regex, substituicao)
# Se a regex nao encontrar nada, a regra e apenas ignorada (ja removida ou editada a mao).
# ----------------------------------------------------------------------------------------

RULES = {
    "src/config.h": [
        ("Bloco NEW_GAME_START_LOCATION",
         r"(?:" + NL + r")?/\* New Game Start Location \*/" + NL + r"#define NEW_GAME_START_LOCATION.*?" + NL + r"#endif" + NL,
         ""),
        ("Bloco CHARACTER_SWAP_SYSTEM",
         r"(?:" + NL + r")?/\* Character Swap System \(Blue Flute\) \*/" + NL + r".*?#ifdef CHARACTER_SWAP_SYSTEM" + NL + r".*?" + NL + r"#endif" + NL + r"?",
         ""),
    ],

    "hooks": [
        ("Hook do local de inicio do jogo",
         r"#ifdef NEW_GAME_START_LOCATION" + NL + r"WarpToPlayersRoom_Custom [0-9A-Fa-f]+ \d" + NL + r"#endif" + NL,
         ""),
        ("Hooks da descricao do item e do IsOtherTrainer",
         r"#ifdef CHARACTER_SWAP_SYSTEM" + NL + r"(?:(?:ItemId_GetDescription_CharSwap|IsOtherTrainer_CharSwap) [0-9A-Fa-f]+ \d" + NL + r")+#endif" + NL,
         ""),
        ("Hook da opcao do Start Menu",
         NL + r"##Character Swap System - Start Menu option[^\r\n]*" + NL + r"#ifdef CHAR_SWAP_MENU_FLAG" + NL + r"DrawHelpMessageWindowWithText_CharSwap [0-9A-Fa-f]+ \d" + NL + r"#endif" + NL,
         ""),
    ],

    "src/Tables/item_tables.c": [
        ("Blue Flute volta ao original",
         r"#ifdef CHARACTER_SWAP_SYSTEM" + NL + r"\t\{[^{}]*?(?:\{[^{}]*\}[^{}]*?)*FieldUseFunc_CharacterSwap.*?" + NL + r"#else" + NL + r"(.*?)" + NL + r"#endif",
         r"\1"),
    ],

    "include/new/item_tables.h": [
        ("Prototipo FieldUseFunc_CharacterSwap", r"void FieldUseFunc_CharacterSwap\(u8 taskId\);" + NL, ""),
        ("Declaracao DESC_CHAR_SWAP_FLUTE", r"extern const u8 DESC_CHAR_SWAP_FLUTE\[\];" + NL, ""),
    ],

    "src/start_menu.c": [
        ("Opcoes do enum",
         r"\tSTARTMENU_CHAR_SWAP,[^\r\n]*" + NL + r"\tSTARTMENU_CHAR_SWAP_EN,[^\r\n]*" + NL + r"\tSTARTMENU_CHAR_SWAP_ES,[^\r\n]*" + NL,
         ""),
        ("Declaracoes externas",
         NL + r"//Character Swap System" + NL + r"(?:(?:extern const u8 gText_(?:MenuCharSwap|CharSwapDescription)\w*\[\];|bool8 StartMenuCharSwapCallback\(void\);|bool8 IsStartMenuCharSwapOption\(u8 option\);|u8 GetStartMenuCharSwapOption\(void\);|bool8 HelpMessageWindowExists\(void\);)" + NL + r")+",
         ""),
        ("Entradas da tabela de acoes",
         r"\t#if defined\(CHARACTER_SWAP_SYSTEM\) && defined\(CHAR_SWAP_MENU_FLAG\)" + NL + r"(?:\t\[STARTMENU_CHAR_SWAP\w*\][^\r\n]*" + NL + r")+\t#endif" + NL,
         ""),
        ("Entradas das descricoes",
         r"\t#if defined\(CHARACTER_SWAP_SYSTEM\) && defined\(CHAR_SWAP_MENU_FLAG\)" + NL + r"(?:\tgText_CharSwapDescription\w*,?" + NL + r")+\t#endif" + NL,
         ""),
        ("Opcao abaixo de BAG",
         NL + r"#if defined\(CHARACTER_SWAP_SYSTEM\) && defined\(CHAR_SWAP_MENU_FLAG\)" + NL + r"\tif \(FlagGet\(CHAR_SWAP_MENU_FLAG\)\)" + NL + r"\t\tAppendToStartMenuItems\(GetStartMenuCharSwapOption\(\)\);[^\r\n]*" + NL + r"#endif" + NL,
         ""),
        ("Checagem da caixa de descricao (1)",
         r" && HelpMessageWindowExists\(\)\)",
         ")"),
        ("Botao A sem fade para a troca",
         r"\t\tif \(IsStartMenuCharSwapOption\(sStartMenuOrder\[sStartMenuCursorPos\]\)\)" + NL + r"\t\t\{" + NL + r"\t\t\tRemoveTimeBox\(\);[^\r\n]*" + NL + r"\t\t\treturn FALSE;" + NL + r"\t\t\}" + NL,
         ""),
        ("Remocao da caixa de descricao ao trocar de pagina",
         r"\tDestroyHelpMessageWindow_\(\); //Recreated by ReloadStartMenuItems[^\r\n]*" + NL,
         ""),
        ("Funcoes da opcao do menu (final do arquivo)",
         NL + r"/\* -+ \*/" + NL + r"/\*\s+Character Swap System - Start Menu option\s+\*/" + NL + r"/\* -+ \*/" + NL + r".*\Z",
         ""),
    ],

    "include/pokemon_storage_system.h": [
        ("Remove PHYSICAL_TOTAL_BOXES_COUNT", r"#define PHYSICAL_TOTAL_BOXES_COUNT[^\r\n]*" + NL, ""),
        ("TOTAL_BOXES_COUNT volta para 25", r"#define TOTAL_BOXES_COUNT       \t\d+[^\r\n]*", "#define TOTAL_BOXES_COUNT       \t25"),
        ("TOTAL_BOXES_COUNT_1_LESS volta para 24", r"#define TOTAL_BOXES_COUNT_1_LESS\t\d+[^\r\n]*", "#define TOTAL_BOXES_COUNT_1_LESS\t24"),
    ],

    "src/pokemon_storage_system.c": [
        ("Tabelas das boxes voltam para TOTAL_BOXES_COUNT", r"\[PHYSICAL_TOTAL_BOXES_COUNT\]", "[TOTAL_BOXES_COUNT]"),
    ],
}


# ----------------------------------------------------------------------------------------

def check_root():
    if not (os.path.isdir("src") and os.path.isdir("include") and os.path.isfile("hooks")):
        print("ERRO: rode este programa na raiz do projeto CFRU (a pasta que tem 'src', 'include' e 'hooks').")
        sys.exit(1)


def make_backup(files):
    folder = "backup_character_swap_" + datetime.now().strftime("%Y%m%d_%H%M%S")
    for f in files:
        if os.path.isfile(f):
            dest = os.path.join(folder, f)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(f, dest)
    return folder


def restore(folder):
    check_root()
    if not os.path.isdir(folder):
        print("ERRO: pasta de backup nao encontrada:", folder)
        sys.exit(1)
    count = 0
    for root, _, names in os.walk(folder):
        for name in names:
            src = os.path.join(root, name)
            rel = os.path.relpath(src, folder)
            os.makedirs(os.path.dirname(rel) or ".", exist_ok=True)
            shutil.copy2(src, rel)
            print("  restaurado:", rel)
            count += 1
    print("\n%d arquivo(s) restaurado(s). A mecanica voltou ao estado de antes da desinstalacao." % count)


def uninstall(simulate, keep_boxes):
    check_root()
    modified = [f for f in MODIFIED_FILES if not (keep_boxes and f in BOX_FILES)]

    print("=" * 70)
    print(" Desinstalador do Character Swap System")
    print("=" * 70)
    print("\nArquivos que serao APAGADOS (criados pela mecanica):")
    for f in NEW_FILES:
        print("  %s %s" % ("-" if os.path.isfile(f) else " (nao existe)", f))
    print("\nArquivos do CFRU que serao CORRIGIDOS (so as alteracoes da mecanica saem):")
    for f in modified:
        print("  %s %s" % ("*" if os.path.isfile(f) else " (nao existe)", f))
    if keep_boxes:
        print("\nO PC vai CONTINUAR com 10 boxes (--manter-boxes).")
    else:
        print("\nO PC vai VOLTAR para 25 boxes.")

    print("\nATENCAO: saves feitos com a mecanica NAO devem ser usados depois de desinstalar,")
    print("a menos que voce use --manter-boxes. As boxes 11 a 25 guardam os dados do")
    print("outro personagem e apareceriam como Pokemon corrompidos se o PC voltar a ter 25 boxes.")

    if not simulate:
        answer = input("\nContinuar? Digite S para sim: ").strip().lower()
        if answer not in ("s", "sim", "y", "yes"):
            print("Cancelado. Nada foi alterado.")
            return
        backup = make_backup(NEW_FILES + modified)
        print("\nBackup criado em:", backup)

    print()
    problems = 0
    for f in modified:
        if not os.path.isfile(f):
            continue
        text = read(f)
        original = text
        print("[%s]" % f)
        for desc, pattern, repl in RULES[f]:
            new_text, n = re.subn(pattern, repl, text, flags=re.DOTALL)
            if n:
                print("   ok   ", desc)
                text = new_text
            else:
                print("   --   ", desc, "(nao encontrado: ja removido ou editado a mao)")
        # Sobras que precisam ser vistas a mao
        leftovers = [w for w in ("CharSwap", "CHAR_SWAP", "CHARACTER_SWAP", "PHYSICAL_TOTAL_BOXES_COUNT", "NEW_GAME_START")
                     if w in text and not (keep_boxes and w == "PHYSICAL_TOTAL_BOXES_COUNT")]
        if leftovers:
            problems += 1
            print("   AVISO: ainda contem:", ", ".join(leftovers), "- confira este arquivo a mao.")
        if not simulate and text != original:
            write(f, text)

    for f in NEW_FILES:
        if os.path.isfile(f):
            if not simulate:
                os.remove(f)
            print("apagado:" if not simulate else "seria apagado:", f)

    print()
    if simulate:
        print("Simulacao concluida. Nada foi alterado.")
    elif problems:
        print("Desinstalacao concluida com %d aviso(s). Confira os arquivos indicados acima." % problems)
    else:
        print("Desinstalacao concluida sem avisos.")
    if not simulate:
        print("Apague a pasta 'build' e compile o projeto de novo.")
        print("Para desfazer: python %s --restaurar %s" % (os.path.basename(sys.argv[0]), backup))


def main():
    args = sys.argv[1:]
    if "--restaurar" in args:
        i = args.index("--restaurar")
        if i + 1 >= len(args):
            print("Use: --restaurar NOME_DA_PASTA_DE_BACKUP")
            sys.exit(1)
        restore(args[i + 1])
        return
    uninstall(simulate="--simular" in args, keep_boxes="--manter-boxes" in args)


if __name__ == "__main__":
    main()
