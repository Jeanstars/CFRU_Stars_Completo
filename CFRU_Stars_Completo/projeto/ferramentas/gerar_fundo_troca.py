# Gera graphics/CharSwapScreen/bgCharSwapScreen.png (240x160, 8bpp)
# Indices 0..191 = arte fixa | 192..207 rampa painel ESQ | 208..223 rampa painel DIR | 224..227 aneis de selecao
from PIL import Image, ImageDraw, ImageFilter
import random, sys
W,H=240,160
STATIC_MAX=192
RAMP_L,RAMP_R,RING=192,208,224

pal=[]; idx={}
def col(r,g,b):
    k=(int(max(0,min(255,r)))&0xF8,int(max(0,min(255,g)))&0xF8,int(max(0,min(255,b)))&0xF8)
    if k not in idx:
        if len(pal)>=STATIC_MAX: raise SystemExit('paleta fixa estourou')
        idx[k]=len(pal); pal.append(k)
    return idx[k]
def lerp(a,b,t): return tuple(a[i]+(b[i]-a[i])*t for i in range(3))
def grad(a,b,t): return col(*lerp(a,b,t))

# ---------------------------------------------------------------- fundo "campo desfocado"
random.seed(5)
bg=Image.new('RGB',(W,H))
d=ImageDraw.Draw(bg)
for y in range(H):
    t=y/H; d.line([(0,y),(W,y)],fill=tuple(int(v) for v in lerp((74,150,74),(58,124,62),t)))
for _ in range(70):                                  # manchas de grama
    x=random.randrange(-10,W); y=random.randrange(-10,H); r=random.randrange(5,16)
    c=random.choice(((90,168,84),(64,136,66),(80,158,78),(52,118,58)))
    d.ellipse([x-r,y-r,x+r,y+r],fill=c)
# caminho de terra (faixa horizontal)
for y in range(50,86):
    e=min(y-50,85-y)
    base=lerp((206,184,116),(196,172,104),(y-50)/35)
    d.line([(0,y),(W,y)],fill=tuple(int(v) for v in lerp((120,150,80),base,min(1,e/4))))
for _ in range(40):
    x=random.randrange(0,W); y=random.randrange(54,82); r=random.randrange(2,6)
    d.ellipse([x-r,y-r,x+r,y+r],fill=random.choice(((220,198,130),(190,166,98))))
# casa (canto superior esquerdo): telhado vermelho + parede clara
d.rectangle([0,0,34,16],fill=(196,64,52)); d.rectangle([0,0,34,5],fill=(222,96,76)); d.rectangle([0,12,34,16],fill=(150,44,40))
d.rectangle([0,17,34,46],fill=(232,228,222))
for wx in (4,20): d.rectangle([wx,24,wx+9,34],fill=(120,170,222))
# arvores
def tree(cx,cy,r):
    d.ellipse([cx-r,cy-r+3,cx+r,cy+r+3],fill=(24,78,46))
    d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=(34,100,58))
    d.ellipse([cx-r+3,cy-r+1,cx+r-4,cy+2],fill=(58,136,76))
for (cx,cy,r) in ((214,8,13),(232,22,12),(222,36,11),(236,4,10),(206,30,9)): tree(cx,cy,r)
for x in range(2,240,17): tree(x+random.randrange(-3,4),152+random.randrange(-2,4),11)
for (cx,cy,r) in ((6,132,10),(18,146,12),(232,128,10),(222,140,11)): tree(cx,cy,r)
bg=bg.filter(ImageFilter.GaussianBlur(2.0))
q=bg.quantize(colors=40,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.FLOYDSTEINBERG)
qp=q.getpalette()[:40*3]
remap={i:col(qp[i*3],qp[i*3+1],qp[i*3+2]) for i in range(40)}

im=Image.new('P',(W,H),0)
px=im.load(); qx=q.load()
for y in range(H):
    for x in range(W): px[x,y]=remap[qx[x,y]]
dr=ImageDraw.Draw(im)
def rect(x0,y0,x1,y1,i): dr.rectangle([x0,y0,x1,y1],fill=i)
def vgrad(x0,y0,x1,y1,a,b):
    for y in range(y0,y1+1): rect(x0,y,x1,y,grad(a,b,(y-y0)/max(1,y1-y0)))
def putp(x,y,i):
    if 0<=x<W and 0<=y<H: im.putpixel((x,y),i)

# ---------------------------------------------------------------- moldura da janela
FX0,FY0,FX1,FY1=30,4,209,140
dr.rounded_rectangle([FX0-1,FY0-1,FX1+1,FY1+1],radius=6,fill=col(20,44,84))
for y in range(FY0,FY1+1):                                    # anel claro (gradiente vertical)
    c=grad((226,244,255),(112,170,220),(y-FY0)/(FY1-FY0))
    dr.line([(FX0,y),(FX1,y)],fill=c)
dr.rounded_rectangle([FX0,FY0,FX1,FY1],radius=5,outline=col(20,44,84))
# interior (margens entre os blocos)
vgrad(FX0+3,FY0+3,FX1-3,FY1-3,(190,218,238),(150,190,222))
dr.rectangle([FX0+2,FY0+2,FX1-2,FY1-2],outline=col(70,118,168))
dr.line([(FX0+3,FY0+1),(FX1-3,FY0+1)],fill=col(255,255,255))   # brilho no topo
for (x,y) in ((FX0+2,FY0+2),(FX1-3,FY0+2),(FX0+2,FY1-3),(FX1-3,FY1-3)): putp(x,y,col(255,255,255))

# ---------------------------------------------------------------- barra de titulo
TX0,TY0,TX1,TY1=36,9,203,23
dr.rectangle([TX0-1,TY0-1,TX1+1,TY1+1],fill=col(36,60,100))
vgrad(TX0,TY0,TX1,TY1,(252,254,255),(208,228,246))
dr.line([(TX0,TY0),(TX1,TY0)],fill=col(255,255,255))
dr.line([(TX0,TY1),(TX1,TY1)],fill=col(150,184,214))

# ---------------------------------------------------------------- paineis
PY0,PY1=26,92
PANELS=((36,116,RAMP_L,RING),(123,203,RAMP_R,RING+2))
for (px0,px1,rb,rg) in PANELS:
    cx=(px0+px1)//2
    rect(px0,PY0,px1,PY0+12,rb+0)                              # faixa do nome
    dr.line([(px0,PY0+1),(px1,PY0+1)],fill=rb+1)
    for y in range(PY0+13,PY0+13+43):                          # corpo (gradiente por rampa)
        k=2+int((y-(PY0+13))/43*7.99)
        dr.line([(px0,y),(px1,y)],fill=rb+k)
    for y in range(PY0+13,PY0+13+43,4):                        # textura leve (linhas finas)
        dr.line([(px0,y),(px1,y)],fill=rb+min(9,2+int((y-(PY0+13))/43*7.99)+1))
    rect(px0,PY0+56,px1,PY1,rb+10)                             # faixa de status
    dr.line([(px0,PY0+56),(px1,PY0+56)],fill=rb+11)
    # plataforma sob o sprite
    dr.ellipse([cx-17,PY0+50,cx+17,PY0+58],fill=rb+12)
    dr.ellipse([cx-15,PY0+51,cx+15,PY0+57],fill=rb+9)
    # aneis de selecao (3px): fora, meio, dentro
    ro,ri=rg,rg+1
    dr.rectangle([px0,PY0,px1,PY1],outline=ro)
    dr.rectangle([px0+1,PY0+1,px1-1,PY1-1],outline=ri)
    dr.rectangle([px0+2,PY0+2,px1-2,PY1-2],outline=ro)
# divisor central
vgrad(117,PY0,122,PY1,(246,252,255),(176,208,234))
dr.line([(117,PY0),(117,PY1)],fill=col(120,160,200)); dr.line([(122,PY0),(122,PY1)],fill=col(120,160,200))

# ---------------------------------------------------------------- caixa de dialogo (estilo da imagem)
BX0,BY0,BX1,BY1=36,96,203,134
dr.rounded_rectangle([BX0,BY0,BX1,BY1],radius=3,fill=col(16,16,24))
dr.rounded_rectangle([BX0+1,BY0+1,BX1-1,BY1-1],radius=3,fill=col(110,118,136))
dr.rounded_rectangle([BX0+2,BY0+2,BX1-2,BY1-2],radius=2,fill=col(212,218,230))
for y in range(BY0+3,BY1-2):
    c=grad((255,255,255),(226,230,240),(y-BY0-3)/(BY1-BY0-5))
    dr.line([(BX0+3,y),(BX1-3,y)],fill=c)

# ---------------------------------------------------------------- barra de teclas
vgrad(0,145,W-1,H-1,(30,38,62),(14,18,34))
dr.line([(0,145),(W-1,145)],fill=col(86,112,166))
dr.line([(0,146),(W-1,146)],fill=col(10,14,26))

# ---------------------------------------------------------------- paleta
while len(pal)<STATIC_MAX: pal.append((0,0,0))
def ramp(base,sel):
    out=[]
    dark=(8,12,24)
    bb=base if sel else tuple(dark[i]+(base[i]-dark[i])*0.875 for i in range(3))
    for k in range(16):
        if k==0: c=lerp(dark,bb,22/32)
        elif k==1: c=lerp(dark,bb,28/32)
        elif 2<=k<=9: c=lerp(lerp(dark,bb,28/32),lerp(dark,bb,1.0),(k-2)/7)
        elif k==10: c=lerp(dark,bb,18/32)
        elif k==11: c=lerp(dark,bb,12/32)
        elif k==12: c=lerp(bb,(255,255,255),12/32)
        else: c=bb
        out.append(tuple(int(v) for v in c))
    return out
pal+=ramp((72,128,200),False)+ramp((48,128,112),True)
pal+=[(190,255,60),(120,230,40),(60,80,120),(44,60,96)]
while len(pal)<256: pal.append((0,0,0))
flat=[]
for c in pal[:256]: flat+=list(c)
im.putpalette(flat)
im.save(sys.argv[1])
tiles=set()
for ty in range(20):
    for tx in range(30): tiles.add(im.crop((tx*8,ty*8,tx*8+8,ty*8+8)).tobytes())
print('cores fixas:',len(idx),'/',STATIC_MAX,'| tiles unicos:',len(tiles),'/600')
