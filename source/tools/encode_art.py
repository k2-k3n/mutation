#!/usr/bin/env python3
"""Compile image-generated artwork and OFL Geist glyphs to immutable SDK assets."""
from PIL import Image,ImageFont,ImageDraw
from pathlib import Path
p=Path(__file__).resolve().parents[1]
im=Image.open(p/'assets/mutation-reference.png').convert('RGB').resize((320,172),Image.Resampling.LANCZOS)
q=im.quantize(colors=256,method=Image.Quantize.FASTOCTREE)
pal=q.getpalette()
def c565(r,g,b):return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
def arr(name,typ,data):return 'static const '+typ+' '+name+'[]={\n'+','.join(str(v) for v in data)+'\n};\n'
s='// Generated from approved image-generation concept and OFL Geist. MIT artwork; see assets/.\n'
s+=arr('art_palette','uint16_t',[c565(*pal[i:i+3]) for i in range(0,768,3)])
s+=arr('art_pixels','uint8_t',list(q.getdata()))
for name,chars,size,w,h in [('digit','0123456789',40,28,46),('small',' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/-',11,9,15)]:
 data=[]
 font=ImageFont.truetype(str(p/'assets/Geist-Medium.ttf'),size)
 for ch in chars:
  m=Image.new('L',(w,h));d=ImageDraw.Draw(m);d.text((0,-2),ch,font=font,fill=255);data.extend(m.getdata())
 s+='static const char '+name+'_chars[]="'+chars+'";\n'
 s+=arr(name+'_alpha','uint8_t',data)
(p/'src/art.h').write_text(s)
print('art generated',len(s))
