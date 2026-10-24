"""Validate the locally generated device package; optionally enforce store readiness."""
from pathlib import Path, PurePosixPath
import argparse, io, json, re, struct, tarfile
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--deb',type=Path,default=None)
parser.add_argument('--require-store',action='store_true')
args=parser.parse_args()
manifest=json.loads((root/'app-builder.json').read_text());store=manifest['store']
deb=args.deb or root/f"dist/key-dash_{manifest['version']}-rev1_arm64.deb"
raw=deb.read_bytes();assert raw[:8]==b'!<arch>\n','Not a Debian ar archive'
parts={};offset=8
while offset<len(raw):
 header=raw[offset:offset+60];assert header[58:60]==b'`\n';length=int(header[48:58])
 name=header[:16].decode().strip().rstrip('/');offset+=60;parts[name]=raw[offset:offset+length];offset+=length+length%2
assert parts['debian-binary']==b'2.0\n'
with tarfile.open(fileobj=io.BytesIO(parts['control.tar.gz'])) as tar:
 control=tar.extractfile('./control').read().decode()
for field in ['Package: '+manifest['package_name'],'Version: '+manifest['version']+'-rev1','Architecture: arm64']:
 assert field in control,field
with tarfile.open(fileobj=io.BytesIO(parts['data.tar.gz'])) as tar:
 directories = set()
 for member in tar.getmembers():
  path = PurePosixPath(member.name)
  assert path == PurePosixPath('.') or str(path.parent) in directories, f'Missing parent directory before {member.name}'
  if member.isdir():
   directories.add(str(path))
   assert member.mode == 0o755, f'Incorrect directory permission: {member.name}'
 binary=tar.extractfile('./usr/bin/key-dash').read()
 assert binary[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',binary,18)[0]==183,'Not an ARM64 ELF executable'
 assert tar.getmember('./usr/bin/key-dash').mode==0o755
 desktop=tar.extractfile('./usr/share/APPLaunch/applications/key-dash.desktop').read().decode()
 assert 'Exec=/usr/bin/key-dash' in desktop and 'Terminal=false' in desktop
 assert './usr/share/APPLaunch/share/images/key-dash.png' in tar.getnames()
 assert not any('systemd' in name for name in tar.getnames()),'Unexpected background service'
 for asset in ['stackchan-neutral.png','stackchan-happy.png']:
  resource=tar.extractfile('./usr/share/key-dash/images/'+asset).read()
  assert resource[:8]==b'\x89PNG\r\n\x1a\n',asset
 assert './usr/share/doc/key-dash/STACKCHAN-CREDITS.md' in tar.getnames()
for path in store['screenshots']:
 data=(root/path).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(320,170),path
icon=(root/store['icon']).read_bytes();assert struct.unpack('>II',icon[16:24])==(256,256)
permissions={'camera','microphone','imu','network','additional_hardware','background_service','external_display'}
assert set(store['permissions'])==permissions
assert store['permissions']['network'] is True
assert all(value is False for name,value in store['permissions'].items() if name!='network')
assert 1<=len(store['categories'])<=2
pending=[]
if not re.fullmatch('[A-Z0-9]{4}',store.get('share_code','')):pending.append('Choose and check a globally unique four-character share code.')
if not store.get('source_repo'):pending.append('Add the actual source repository URL after creating the repository.')
print('PASS: package identity, ARM64 ELF, executable permission, APPLaunch entry, icon, screenshots, seven permission declarations, no background service.')
for note in pending:print('Publication pending:',note)
print('Physical-device testing and a real-device demo video are still required.')
if args.require_store and pending:raise SystemExit(1)
