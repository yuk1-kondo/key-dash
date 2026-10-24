"""Build a genuine ARM64 Debian package on macOS or Linux using Python stdlib.

macOS CPack distributions omit the DEB generator. This produces the same
launcher/binary layout without installing a second packaging toolchain.
"""
from pathlib import Path, PurePosixPath
import argparse, gzip, io, json, struct, tarfile
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, default=root/'build/cp0-cross/Release/key-dash')
parser.add_argument('--output', type=Path, default=root/'dist')
args = parser.parse_args()
manifest = json.loads((root/'app-builder.json').read_text())
binary = args.binary.read_bytes()
if binary[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', binary, 18)[0] != 183:
 raise SystemExit('Refusing to package: binary must be a little-endian ELF64 AArch64 executable.')
name = manifest['package_name']; version = manifest['version']+'-rev1'
control = f'''Package: {name}
Version: {version}
Section: APPLaunch
Priority: optional
Architecture: arm64
Maintainer: yuk1-kondo
Depends: libc6 (>= 2.34), libstdc++6 (>= 11), libgcc-s1, libfreetype6, libpng16-16t64 | libpng16-16, libjpeg62-turbo, zlib1g, libcurl4t64 | libcurl4, ca-certificates
Description: A typing game with optional online rankings for CardputerZero
 Type short words in timed or untimed rounds and track personal bests.
'''
desktop = '''[Desktop Entry]
Type=Application
Name=taM5CZTyping
Comment=tataM5CZTypingyping
Exec=/usr/bin/key-dash
Icon=share/images/key-dash.png
Terminal=false
Categories=Game;
'''
def archive(files):
 buffer=io.BytesIO()
 with tarfile.open(fileobj=buffer,mode='w',format=tarfile.USTAR_FORMAT) as tar:
  directories = {str(parent) for path, _, _ in files for parent in PurePosixPath(path).parents}
  for directory in sorted(directories, key=lambda value: (len(PurePosixPath(value).parts), value)):
   info = tarfile.TarInfo('./' if directory == '.' else './' + directory)
   info.type = tarfile.DIRTYPE
   info.mode = 0o755
   info.uid = info.gid = 0
   info.uname = info.gname = 'root'
   info.mtime = 0
   tar.addfile(info)
  for path,data,mode in files:
   info=tarfile.TarInfo('./'+path);info.size=len(data);info.mode=mode
   info.uid=info.gid=0;info.uname=info.gname='root';info.mtime=0
   tar.addfile(info,io.BytesIO(data))
 return gzip.compress(buffer.getvalue(),mtime=0)
files=[('usr/bin/key-dash',binary,0o755),
 ('usr/share/APPLaunch/applications/key-dash.desktop',desktop.encode(),0o644),
 ('usr/share/APPLaunch/share/images/key-dash.png',(root/'assets/images/key-dash.png').read_bytes(),0o644),
 ('usr/share/doc/key-dash/README.md',(root/'README.md').read_bytes(),0o644),
 ('usr/share/doc/key-dash/LICENSE',(root/'LICENSE').read_bytes(),0o644)]
for image in ['stackchan-neutral.png','stackchan-happy.png']:
 files.append((f'usr/share/key-dash/images/{image}',(root/'assets/images'/image).read_bytes(),0o644))
files.append(('usr/share/doc/key-dash/STACKCHAN-CREDITS.md',(root/'assets/images/STACKCHAN-CREDITS.md').read_bytes(),0o644))
for label,path in [
 ('lvgl',root/'.cache/cp0-deps/lvgl-src/LICENCE.txt'),
 ('nlohmann-json',root/'third_party/nlohmann/LICENSE.MIT'),
 ('qrcodegen',root/'third_party/qrcodegen/LICENSE.txt'),
 ('fmt',root/'.cache/cp0-deps/fmt-src/LICENSE.rst'),
 ('thorvg',root/'.cache/cp0-deps/lvgl-src/src/libs/thorvg/LICENSE.txt')]:
 if path.exists(): files.append((f'usr/share/doc/key-dash/licenses/{label}.txt',path.read_bytes(),0o644))
control_data=archive([('control',control.encode(),0o644)])
data=archive(files)
def member(name,data):
 header=f'{name+"/":<16}{0:<12}{0:<6}{0:<6}{"100644":<8}{len(data):<10}`\n'.encode('ascii')
 assert len(header)==60
 return header+data+(b'\n' if len(data)%2 else b'')
args.output.mkdir(parents=True,exist_ok=True)
output=args.output/f'{name}_{version}_arm64.deb'
output.write_bytes(b'!<arch>\n'+member('debian-binary',b'2.0\n')+member('control.tar.gz',control_data)+member('data.tar.gz',data))
print(output)
