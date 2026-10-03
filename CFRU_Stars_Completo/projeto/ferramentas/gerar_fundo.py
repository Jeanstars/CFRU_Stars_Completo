from PIL import Image, ImageDraw
import random, math, sys
W,H=240,160
STATIC_MAX=192            # indices 0..191 static, 192..239 dynamic ramps (3 x 16)
RAMP_BASE=192
CX=(50,120,190)

def c5(v): return (v*8)|(v>>2)
pal=[]               # list of (r,g,b) 8-bit
idx={}
def col(r,g,b):
    r,g,b=int(max(0,min(255,r)))&0xF8,int(max(0,min(255,g)))&0xF8,int(max(0,min(255,b)))&0xF8  # snap to GBA 5-bit
    k=(r,g,b)
    if k not in idx:
        if len(pal)>=STATIC_MAX: raise SystemExit('static palette overflow')
        idx[k]=len(pal); pal.append(k)
    return idx[k]
def lerp(a,b,t): return tuple(a[i]+(b[i]-a[i])*t for i in range(3))
def grad(a,b,t): return col(*lerp(a,b,t))

im=Image.new('P',(W,H),0)
d=ImageDraw.Draw(im)
def px(x,y,i):
    if 0<=x<W and 0<=y<H: im.putpixel((x,y),i)
def rect(x0,y0,x1,y1,i): d.rectangle([x0,y0,x1,y1],fill=i)
def vgrad(x0,y0,x1,y1,a,b):
    for y in range(y0,y1+1):
        t=(y-y0)/max(1,(y1-y0)); rect(x0,y,x1,y,grad(a,b,t))

# index 0 must exist first (backdrop colour)
BG0=(10,14,24); col(*BG0)

# ---------- outer frame / bezel ----------
vgrad(0,0,W-1,H-1,(30,42,62),(18,26,42))
# bezel edge lines
d.rectangle([0,0,W-1,H-1],outline=col(70,95,125))
d.rectangle([1,1,W-2,H-2],outline=col(12,18,30))
# panel seams + rivets
for x in (30,210):
    d.line([(x,98),(x,112)],fill=col(10,16,28)); d.line([(x+1,98),(x+1,112)],fill=col(60,82,110))
for (x,y) in ((4,155),(235,155),(4,112),(235,112)):
    px(x,y,col(120,150,180)); px(x+1,y,col(60,80,105)); px(x,y+1,col(60,80,105))

# ---------- screen area with circuit pattern ----------
SX0,SY0,SX1,SY1=6,18,233,96
vgrad(SX0,SY0,SX1,SY1,(14,22,38),(20,32,52))
d.rectangle([SX0-1,SY0-1,SX1+1,SY1+1],outline=col(8,12,22))
d.rectangle([SX0-2,SY0-2,SX1+2,SY1+2],outline=col(64,88,118))
random.seed(11)
trace=col(34,56,86); node=col(58,92,132); trace2=col(26,42,66)
def circuit(x,y,dx,dy,n,color):
    for _ in range(n):
        L=random.randrange(6,18)
        for s in range(L):
            x+=dx; y+=dy
            if SX0<x<SX1 and SY0<y<SY1: px(x,y,color)
        if random.random()<.7:
            dx,dy=(dy,dx) if random.random()<.5 else (-dy,-dx)
        if dx==0 and dy==0: dx=1
    if SX0<x<SX1 and SY0<y<SY1:
        d.rectangle([x-1,y-1,x+1,y+1],outline=node)
for _ in range(34):
    x=random.randrange(SX0+2,SX1-2); y=random.randrange(SY0+2,SY1-2)
    dx,dy=random.choice(((1,0),(-1,0),(0,1),(0,-1)))
    circuit(x,y,dx,dy,random.randrange(2,5),trace if random.random()<.6 else trace2)
# floor strip under platforms
vgrad(SX0,92,SX1,SY1,(16,26,44),(10,16,28))
for x in range(SX0,SX1+1,8): px(x,94,col(30,48,74))

# ---------- title bar (chamfered hex pill) ----------
def poly_pill(x0,y0,x1,y1,c,fill,border,ci):
    pts=[(x0+c,y0),(x1-c,y0),(x1,y0+ (y1-y0)//2),(x1-c,y1),(x0+c,y1),(x0,y0+(y1-y0)//2)]
    d.polygon(pts,fill=border)
    pts2=[(x0+c+1,y0+1),(x1-c-1,y0+1),(x1-1,y0+(y1-y0)//2),(x1-c-1,y1-1),(x0+c+1,y1-1),(x0+1,y0+(y1-y0)//2)]
    d.polygon(pts2,fill=fill)
poly_pill(3,2,236,16,8,col(10,16,30),col(120,185,215),0)
# inner gradient on title
for y in range(4,15):
    t=(y-4)/10; i=grad((8,14,28),(16,30,52),t)
    for x in range(14,226): px(x,y,i)
# bright inner line
d.line([(12,3),(227,3)],fill=col(150,210,235))

# ---------- bottom bezel ----------
vgrad(2,98,W-3,113,(26,38,58),(20,30,48))
d.line([(2,97),(W-3,97)],fill=col(64,88,118))
d.line([(2,113),(W-3,113)],fill=col(10,16,28))

# ---------- pillars ----------
def ramp(p,k): return RAMP_BASE+p*16+k
for p,cx in enumerate(CX):
    x0,x1=cx-25,cx+25
    for y in range(20,92):
        t=(y-20)/71
        k=int(2+t*6.2)               # 2..8 glow gradient (stronger at bottom)
        for x in range(x0,x1+1):
            e=min(x-x0,x1-x)
            kk=k-(2 if e<2 else 1 if e<5 else 0)
            if (y%4)==0: kk-=1
            px(x,y,ramp(p,max(0,kk)))
    # top cap line
    d.line([(x0,20),(x1,20)],fill=ramp(p,8)); d.line([(x0,21),(x1,21)],fill=ramp(p,3))
    # platform
    d.ellipse([cx-30,77,cx+30,95],fill=ramp(p,14))                  # shadow outline
    d.ellipse([cx-29,77,cx+29,93],fill=ramp(p,12))                  # rim light
    d.ellipse([cx-27,79,cx+27,92],fill=ramp(p,9))                   # top surface
    d.ellipse([cx-24,81,cx+24,90],fill=ramp(p,8))
    d.ellipse([cx-21,82,cx+21,89],fill=ramp(p,10))
    d.arc([cx-29,77,cx+29,93],200,340,fill=ramp(p,13))
    # brackets (selection corners)
    bx0,bx1,by0,by1=cx-29,cx+29,18,96
    L=8
    for t in range(2):
        for (ax,ay,sx,sy) in ((bx0,by0,1,1),(bx1,by0,-1,1),(bx0,by1,1,-1),(bx1,by1,-1,-1)):
            for s in range(L):
                px(ax+sx*s,ay+sy*t,ramp(p,15)); px(ax+sx*t,ay+sy*s,ramp(p,15))

# ---------- name tags ----------
tagbg=col(8,10,16); tagbg2=col(14,18,26)
for p,cx in enumerate(CX):
    x0,x1,y0,y1=cx-34,cx+34,98,111
    d.rounded_rectangle([x0,y0,x1,y1],radius=4,fill=ramp(p,11))
    d.rounded_rectangle([x0+1,y0+1,x1-1,y1-1],radius=3,fill=tagbg)
    for y in range(y0+2,y1-1): 
        for x in range(x0+9,x1-1): px(x,y,tagbg if (y-y0)<8 else tagbg2)
    # emblem circle (type colour) with mini poke ball
    ex,ey=x0+7,(y0+y1)//2
    d.ellipse([ex-7,ey-7,ex+7,ey+7],fill=ramp(p,11))
    d.ellipse([ex-6,ey-6,ex+6,ey+6],fill=ramp(p,14))
    d.ellipse([ex-5,ey-5,ex+5,ey+5],fill=col(250,250,250))
    d.pieslice([ex-5,ey-5,ex+5,ey+5],180,360,fill=col(232,48,40))
    d.line([(ex-5,ey),(ex+5,ey)],fill=col(24,24,32))
    d.ellipse([ex-1,ey-1,ex+1,ey+1],fill=col(250,250,250),outline=col(24,24,32))

# ---------- bottom dialog box (left) ----------
bx0,by0,bx1,by1=3,114,113,158
d.rounded_rectangle([bx0,by0,bx1,by1],radius=3,fill=col(30,34,42))
d.rounded_rectangle([bx0+1,by0+1,bx1-1,by1-1],radius=3,fill=col(150,155,165))
d.rounded_rectangle([bx0+2,by0+2,bx1-2,by1-2],radius=2,fill=col(44,48,58))
for y in range(by0+3,by1-2):
    t=(y-by0-3)/(by1-by0-6)
    i=grad((248,248,250),(214,216,224),t)
    for x in range(bx0+3,bx1-2): px(x,y,i)

# ---------- info panel (right) ----------
ix0,iy0,ix1,iy1=118,114,236,158
d.rounded_rectangle([ix0,iy0,ix1,iy1],radius=2,fill=col(8,10,14))
d.rounded_rectangle([ix0+1,iy0+1,ix1-1,iy1-1],radius=2,fill=col(190,200,215))
d.rounded_rectangle([ix0+2,iy0+2,ix1-2,iy1-2],radius=1,fill=col(70,80,96))
for y in range(iy0+3,iy1-2):
    t=(y-iy0-3)/(iy1-iy0-6)
    i=grad((8,10,14),(18,22,30),t)
    for x in range(ix0+3,ix1-2): px(x,y,i)

# ---------- palette out ----------
# idle-looking placeholder ramps (runtime overwrites them with the real type colours)
base=[(60,170,70),(230,90,40),(60,120,230)]
while len(pal)<RAMP_BASE: pal.append((0,0,0))
for p in range(3):
    for k in range(16):
        b=base[p]
        if k<=11: r=lerp((10,14,24),b,k/11)
        elif k==12: r=lerp(b,(255,255,255),.5)
        elif k==13: r=lerp(b,(255,255,255),.8)
        elif k==14: r=lerp(b,(0,0,0),.6)
        else: r=(250,210,50)
        pal.append(tuple(int(v) for v in r))
flat=[]
for c in pal: flat+=list(c)
flat+=[0]*(768-len(flat))
im.putpalette(flat)
out=sys.argv[1]
im.save(out)
tiles=set()
for ty in range(20):
    for tx in range(30):
        tiles.add(im.crop((tx*8,ty*8,tx*8+8,ty*8+8)).tobytes())
print('static colours used:',len(idx),'/',STATIC_MAX,' unique 8x8 tiles:',len(tiles),'(max 600)')
