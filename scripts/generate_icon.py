"""Export the original KEY DASH keycap monogram, with Python stdlib only."""
from pathlib import Path
import struct,zlib
size=256
pixels=[[(243,238,223)]*size for _ in range(size)]
def box(x,y,w,h,color):
 for row in range(y,y+h):
  for column in range(x,x+w):pixels[row][column]=color
# Two keys echo the native app mark: compact, flat, and readable at store size.
box(36,44,112,168,(37,123,104))
box(164,44,56,72,(37,123,104))
box(164,132,56,80,(225,151,104))
# A cut-out K remains legible after downscaling.
for index,row in enumerate(['10001','10010','10100','11000','10100','10010','10001']):
 for column,letter in enumerate(row):
  if letter=='1':box(56+column*14,70+index*16,12,14,(243,238,223))
def chunk(name,data):
 return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
data=b''.join(b'\0'+bytes(value for pixel in row for value in pixel) for row in pixels)
image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data))+chunk(b'IEND',b'')
(Path(__file__).resolve().parents[1]/'assets/images/key-dash.png').write_bytes(image)
