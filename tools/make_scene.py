# Erzeugt src/scene.h: Pixel-Szene 384x64 in 4 Animationsbildern (Palette + Indizes)
import random, math, sys
W,H=384,64
PAL=[  # Palette (RGB)
 (0x0b,0x08,0x1e),(0x16,0x10,0x3a),(0x24,0x16,0x52),(0x3a,0x1a,0x66),(0x5a,0x1e,0x74),(0x7e,0x24,0x7a),  # 0-5 Himmel oben
 (0xa8,0x30,0x78),(0xd0,0x45,0x6e),(0xec,0x66,0x62),(0xf8,0x8c,0x5a),(0xff,0xb0,0x5a),                   # 6-10 Himmel unten
 (0xff,0xd8,0x78),(0xff,0xf0,0xb0),(0xff,0xff,0xe8),                                                   # 11-13 Sonne
 (0x10,0x2a,0x5a),(0x16,0x3a,0x72),(0x1e,0x4e,0x8a),(0x0a,0x1a,0x40),                                  # 14-17 Meer
 (0x2a,0x10,0x34),(0x14,0x08,0x1c),                                                                   # 18-19 Insel/Palme
 (0xff,0xff,0xff),(0xb8,0xc8,0xff),(0x6a,0x9a,0xd8),                                                  # 20-22 Sterne/Glitzer
 (0xc0,0x3a,0x70),(0xe0,0x5a,0x6a),                                                                   # 23-24 Wolken
]
SKY=[0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,9,9,9,10,10,10,10,11,11,11,11,11]
HOR=44
def frame(k):
    P=[[0]*W for _ in range(H)]
    for y in range(HOR):
        for x in range(W):
            i=min(y*len(SKY)//HOR,len(SKY)-1)
            # leichtes Dithering an Farbgrenzen
            j=min((y*len(SKY)+((x+y)%2)*len(SKY)//2)//HOR,len(SKY)-1)
            P[y][x]=SKY[j]
    rnd=random.Random(3)
    for n in range(60):
        x,y=rnd.randrange(W),rnd.randrange(0,20)
        if (n+k)%4: P[y][x]=20 if (n+k)%3 else 21
    # Sonne
    cx,cy,r=296,HOR,16
    for y in range(HOR-r,HOR):
        for x in range(cx-r,cx+r+1):
            d=math.hypot(x-cx,(y-cy))
            if d<=r: P[y][x]=13 if d<r-7 else (12 if d<r-3 else 11)
    # Wolkenbaender vor der Sonne
    for (x0,x1,y,c) in [(230,330,30,23),(250,372,34,24),(200,300,38,24),(20,120,24,23),(60,170,28,23)]:
        for x in range(x0,x1):
            if (x+y)%7: P[y][x]=c
    # Meer
    for y in range(HOR,H):
        for x in range(W):
            P[y][x]=14 if y<HOR+5 else (15 if y<HOR+12 else 17)
    rr=random.Random(50+k)
    for y in range(HOR,H):
        w=max(2,int(20-(y-HOR)*0.8))
        for x in range(cx-w,cx+w+1):
            if rr.random()<0.55: P[y][x]=12 if abs(x-cx)<w*0.4 else 11
    for _ in range(90):
        x,y=rr.randrange(W),rr.randrange(HOR+1,H)
        if abs(x-cx)>22:
            P[y][x]=22 if rr.random()<0.7 else 21
            if x+1<W: P[y][x+1]=P[y][x]
    # Insel links
    for x in range(10,140):
        t=(x-75)/65
        top=HOR-1-int(9*(1-t*t))
        for y in range(top,HOR+1): P[y][x]=18
    # zweite, kleine Insel
    for x in range(150,190):
        t=(x-170)/20
        top=HOR-1-int(3*(1-t*t))
        for y in range(top,HOR+1): P[y][x]=18
    # Palmen (Stamm gebogen, Kronen)
    def palm(bx,by,h,lean):
        pts=[]
        for i in range(h):
            x=bx+int(lean*(i/h)**1.6*h/3); y=by-i
            P[y][x]=19; P[y][x+1]=19; pts.append((x,y))
        tx,ty=pts[-1]
        leaves=[(1,0.2),(-1,0.2),(1,-0.5),(-1,-0.5),(0.6,0.9),(-0.6,0.9),(1,0.6),(-1,0.6)]
        for dx,dy in leaves:
            for s in range(1,15):
                x=tx+int(dx*s); y=ty+int(dy*s*0.5+ (s*s)/40)
                if 0<=x<W and 0<=y<H:
                    P[y][x]=19
                    if y+1<H and s<12: P[y+1][x]=19
    palm(60,HOR-6,26,1.0); palm(90,HOR-5,18,-0.8)
    return P
frames=[frame(k) for k in range(4)]
with open(sys.argv[1],'w') as f:
    f.write("// automatisch erzeugt von tools/make_scene.py\n#define SCENE_W %d\n#define SCENE_H %d\n#define SCENE_FRAMES 4\n"%(W,H))
    f.write("static const unsigned int scene_pal[%d]={%s};\n"%(len(PAL),",".join("0x%02x%02x%02x"%c for c in PAL)))
    f.write("static const unsigned char scene_px[4][%d]={\n"%(W*H))
    for P in frames:
        flat=[v for row in P for v in row]
        f.write("{"+",".join(map(str,flat))+"},\n")
    f.write("};\n")
# Vorschau
from PIL import Image
im=Image.new('RGB',(W,H))
im.putdata([PAL[v] for row in frames[0] for v in row])
im.resize((W*3,H*3),Image.NEAREST).save('/home/claude/ags-starter/tools/scene_preview.png')
