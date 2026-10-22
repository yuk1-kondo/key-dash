from pathlib import Path
import shutil, plistlib, subprocess, tempfile, os, zipfile, json
project=Path(__file__).resolve().parents[1];root=project
(project/'dist').mkdir(exist_ok=True)
app=Path(tempfile.mkdtemp(prefix='key-dash-preview-',dir='/private/tmp'))/'KEY DASH.app'
mac=app/'Contents/MacOS';resources=app/'Contents/Resources';frameworks=app/'Contents/Frameworks'
for p in (mac,resources,frameworks):p.mkdir(parents=True,exist_ok=True)
main=mac/'key-dash';shutil.copy2(project/'build/darwin-arm64/Debug/key-dash',main)
shutil.copytree(project/'assets',resources/'assets',dirs_exist_ok=True)
plist={'CFBundleExecutable':'key-dash','CFBundleName':'KEY DASH','CFBundleDisplayName':'KEY DASH','CFBundleIdentifier':'local.key-dash.preview','CFBundleVersion':'1','CFBundleShortVersionString':json.loads((project/'app-builder.json').read_text())['version'],'CFBundlePackageType':'APPL','NSHighResolutionCapable':True,'LSMinimumSystemVersion':'13.0'}
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(plist))
queue=[(main,main)];seen=set();dependencies=[]
while queue:
 destination,origin=queue.pop(0)
 lines=subprocess.check_output(['otool','-L',str(origin)],text=True).splitlines()[1:]
 for line in lines:
  original=line.strip().split(' (')[0]
  if not original.startswith('/opt/homebrew/'):continue
  dependency=Path(original);name=dependency.name;copied=frameworks/name
  if name not in seen:
   seen.add(name);shutil.copy2(dependency,copied);queue.append((copied,dependency));dependencies.append(original)
   subprocess.run(['install_name_tool','-id','@rpath/'+name,str(copied)],check=True,capture_output=True)
  replacement=('@executable_path/../Frameworks/' if destination==main else '@loader_path/')+name
  subprocess.run(['install_name_tool','-change',original,replacement,str(destination)],check=True,capture_output=True)

(resources/'DEPENDENCIES.txt').write_text('Local macOS preview. Built from CardputerZero Template (MIT).\nRuntime dependencies bundled from the local Homebrew installation:\n'+'\n'.join(dependencies)+'\n')
licenses=resources/'licenses';licenses.mkdir(exist_ok=True)
for name,path in {
 'Template-MIT.txt':project/'LICENSE',
 'nlohmann-json.txt':project/'third_party/nlohmann/LICENSE.MIT',
 'qrcodegen.txt':project/'third_party/qrcodegen/LICENSE.txt',
 'StackChan-credits.md':project/'assets/images/STACKCHAN-CREDITS.md',
 'SDL2.txt':Path('/opt/homebrew/opt/sdl2/LICENSE.txt'),
 'fmt.txt':Path('/opt/homebrew/opt/fmt/LICENSE'),
 'FreeType.txt':Path('/opt/homebrew/opt/freetype/LICENSE.TXT'),
 'libpng.txt':Path('/opt/homebrew/opt/libpng/LICENSE'),
 'libjpeg-turbo.md':Path('/opt/homebrew/opt/jpeg-turbo/LICENSE.md'),
 'LVGL.txt':project/'.cache/darwin-arm64-deps/lvgl-src/LICENCE.txt',
 'ThorVG.txt':project/'.cache/darwin-arm64-deps/lvgl-src/src/libs/thorvg/LICENSE.txt',
}.items():
 if path.exists():shutil.copyfile(path,licenses/name)
for file in app.rglob('*'):
 if file.is_file():file.chmod(file.stat().st_mode | 0o200)
subprocess.run(['xattr','-cr',str(app)],check=True,capture_output=True)
subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True,capture_output=True)
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True,capture_output=True)
output=project/'dist/KEY-DASH-macOS-preview.zip'
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
 for file in sorted(app.rglob('*')):
  if file.is_file():archive.write(file,file.relative_to(app.parent))

print('Signed and verified:',app)
print(output)
