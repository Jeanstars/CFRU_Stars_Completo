# Textos da introdução por idioma (guia do Pikachu + Professor Carvalho/Oak)

Cada texto existe em 3 idiomas, escolhidos pelas mesmas flags dos seus scripts:

    0x260 = Português (BR)     0x261 = English (USA)     0x262 = Español (ESP)

Sem nenhuma flag, o jogo usa o idioma guardado no save (o da tela de seleção). Se não houver, usa BR.

## Como editar os textos (editor em HTML)

1. Abra `ferramentas/editores/editor_textos_intro.html` no **Google Chrome** ou no **Microsoft Edge** (duplo clique).
2. Clique em **📂 Abrir pasta do CFRU** e escolha a pasta do projeto (a que tem `src`, `strings` e `charmap.tbl`).
   O navegador vai pedir permissão para editar a pasta: aceite.
3. Escolha o texto na lista, clique na aba do idioma e **cole o seu texto** (Ctrl+V).
   O programa adapta sozinho para o jogo (veja abaixo) e mostra, à direita, **como fica na tela do jogo**.
4. Clique em **💾 Salvar e exportar**. Ele grava na pasta do CFRU:
   - `strings/intro_texts.string`
   - `src/intro_texts_table.h`
   - `intro_texts.json` (os seus textos; ao abrir a pasta de novo eles voltam sozinhos)
5. Compile o projeto normalmente.

Em outros navegadores (Firefox, Safari) não dá para abrir a pasta: o editor funciona, e ao salvar ele **baixa** os
3 arquivos para você copiar para `strings/`, `src/` e a raiz do projeto.

## A adaptação automática
Ao colar (ou ao clicar em **✨ Adaptar texto ao jogo**) o programa:
- **quebra as linhas** pela largura real em pixels da fonte do jogo (medida na ROM), e não por número de letras;
- na caixa do Professor Carvalho, junta **2 linhas por página** e coloca o `\p` entre as páginas;
- no guia do Pikachu, só quebra as linhas (a página inteira aparece de uma vez);
- ajusta caracteres: `POKEMON`/`Pokémon` → `POKéMON` (como o jogo escreve), `...` → `…`, aspas curvas → retas,
  e `ä`/`ö` (que textos de outras hacks usam no lugar de ã/õ) → `ã`/`õ`.

**Enter no texto colado = novo parágrafo** (no Professor, começa uma nova página). Você não precisa digitar `\n` nem `\p`.

Botões e opções:
- **✨ Adaptar texto ao jogo**: refaz as quebras de linha e de página do texto da aba atual.
  Atenção: ele **refaz** as quebras, então quebras que você tenha colocado à mão com `\n` são recalculadas.
- **↶ Desfazer adaptação**: volta ao texto anterior.
- **Adaptar sozinho ao colar**: ligado por padrão.
- **Salvar**: se algum texto não couber (linha larga demais ou linhas demais), o programa avisa e oferece
  **Adaptar e salvar** (adapta só os textos com problema), **Salvar assim mesmo** ou **Cancelar**.
  Se houver caractere que o jogo não tem, ele não deixa salvar e mostra qual.

Limites usados (medidos nos textos originais da ROM):
Professor Carvalho: 196 px por linha e 2 linhas por página; páginas do Pikachu: 216 px; explicação dos botões: 221 px;
textos de cada botão (direcional, A, B, START, SELECT, L/R): 182 px e o mesmo número de linhas do original (as telas azuis têm pouco espaço ao lado do ícone).
Uma linha que passa do limite aparece em **vermelho** na prévia, com a marca do limite.

### A prévia
As quebras de linha, as páginas e as larguras são **exatas** (usam as larguras reais das letras do jogo).
O desenho das letras e o cenário são só aproximados (não usei os gráficos do jogo).

## Cada fala é um texto separado
O jogo mostra a apresentação do Professor em várias falas ("Apresentação", "Este mundo…", "Habitado por POKéMON",
"POKéMON: pets e profissão"), e entre "Este mundo…" e "…é habitado" toca o grito de um POKéMON.
Cole cada trecho na fala correspondente. Se colar tudo numa fala só, as outras continuam aparecendo depois.

## Acentos: ã e õ
A fonte do FireRed **não tem** as letras com til (ã, õ). Por padrão o programa troca por `a` e `o` ao salvar e avisa
em cada caso (`botão` vira `botao`). Para ter o til de verdade é preciso editar a fonte da ROM.
Para o programa deixar de trocar, desmarque a opção "Trocar ã/õ por a/o" (aí ele acusa erro nesses caracteres).

## O que foi adicionado ao projeto
| Arquivo | Para que serve |
|---|---|
| `src/intro_text_translation.c` | `TranslateIntroText()`: recebe o texto original e devolve o do idioma atual |
| `src/intro_texts_table.h` e `strings/intro_texts.string` | GERADOS pelo editor (não edite à mão) |
| `intro_texts.json` | seus textos em PT/EN/ES (o editor lê e grava) |
| `assembly/hooks/intro_text_hooks.s` | os dois ganchos |
| `hooks` (2 linhas no fim) | `IntroText_StringExpandWrapper 081C5D08 3` e `IntroText_PrintTextHook 0812E5A6 4` |
| `bytereplacement` | **cópia do arquivo do CFRU** com um bloco no fim: os 14 `bl StringExpandPlaceholders` do Carvalho chamam o desvio |
| `src/scripting.c` | a versão em C do CFRU de uma fala do Carvalho também traduz (e o gancho do título da seleção de idioma) |

Atenção: `bytereplacement` substitui o arquivo de mesmo nome do CFRU (idêntico, mais o bloco novo no fim).
O desvio ocupa 8 bytes do antigo texto "This world…" (0x081C5D08); o conteúdo desse texto vem da tabela, não da ROM.

## Textos cobertos (23)
Guia do Pikachu: 3 páginas de introdução, explicação dos botões, direcional, A, B, START, SELECT, L/R.
Professor Carvalho: apresentação, "Este mundo…", habitado por POKéMON, pets/profissão, fale de você,
pergunta do nome, confirmação do nome, neto/rival, pergunta e confirmações do nome do rival,
"Sua lenda vai começar", menino ou menina (a pergunta).

## Fora do escopo (precisam de outros ganchos)
Rótulos MENINO/MENINA, lista de nomes (NEW NAME, GREEN, RED…), títulos da tela de digitar nome.

## Verificação feita
- Build completo do CFRU com a ROM BPRE 1.0: sem erros.
- Emulação ARM da ROM gerada: 0 divergências nos dois ganchos, registradores/pilha preservados.
- Editor HTML testado num Chromium real: colar texto, adaptar, desfazer, prévia, avisos, salvar numa pasta,
  reabrir; os arquivos exportados são idênticos aos do gerador de referência e a ferramenta de strings do CFRU os aceita.
- NÃO foi testado num emulador de GBA. Comece um save novo e confira a introdução nos 3 idiomas.
