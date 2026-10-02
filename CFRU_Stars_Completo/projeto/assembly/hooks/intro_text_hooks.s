.thumb
.text
.align 2

.global IntroText_StringExpandWrapper
.global IntroText_PrintTextHook

@ ---------------------------------------------------------------------------
@ Gancho 1: wrapper de StringExpandPlaceholders(dest, src)
@ Todas as falas do Professor Carvalho passam por essa funcao. Os 14 pontos de chamada
@ (arquivo 'bytereplacement') chamam o desvio em 0x081C5D08 (linha em 'hooks'), que cai aqui.
@ Entrada: r0 = dest, r1 = src, lr = quem chamou. Troca src pelo texto do idioma e chama a
@ funcao ORIGINAL (que continua intacta na ROM) sem mexer na pilha: o retorno vai direto
@ para quem chamou.
@ ---------------------------------------------------------------------------
.pool
IntroText_StringExpandWrapper:
	push {r0, lr}
	mov r0, r1
	bl TranslateIntroText	@ r0 = texto do idioma (ou o mesmo)
	mov r1, r0
	pop {r0}
	pop {r2}
	mov lr, r2
	ldr r3, =(0x08008FCC | 1)	@ StringExpandPlaceholders original
	bx r3

@ ---------------------------------------------------------------------------
@ Gancho 2: AddTextPrinterParameterized4  [0x812E5A6, via r4]
@ Imprime o guia de controles (as 3 paginas do Pikachu e a explicacao dos botoes).
@ O texto e o 9o argumento (na pilha). Neste ponto o prologo ja salvou r4-r6 e lr.
@ As 5 instrucoes sobrescritas (mov r6,sb; mov r5,r8; push {r5,r6}; sub sp,#0x10;
@ adds r5,r1,#0) sao refeitas aqui; depois volta para 0x812E5B0.
@ ---------------------------------------------------------------------------
.pool
IntroText_PrintTextHook:
	mov r6, sb
	mov r5, r8
	push {r5, r6}
	sub sp, #0x10
	mov r5, r1
	push {r0, r1, r2, r3}
	ldr r0, [sp, #0x48]		@ texto (9o argumento)
	bl TranslateIntroText
	str r0, [sp, #0x48]
	pop {r0, r1, r2, r3}
	ldr r4, =(0x0812E5B0 | 1)
	bx r4

.pool
