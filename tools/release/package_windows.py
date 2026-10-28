"""Create a clean portable beta ZIP, without copying the user's profile."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--app', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--commit', required=True)
parser.add_argument('--runtime-licenses', type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
name = 'citron-mods-by-yngvifreyr1810-Beta-v0.1-windows-x64'
destination = args.out / name
if destination.exists():
    raise SystemExit('Output folder already exists; use a fresh output location.')
destination.mkdir(parents=True)
files = ['citron.exe', 'libc++.dll', 'libunwind.dll', 'libusb-1.0.dll', 'qt.conf',
         'Qt6Concurrent.dll', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Network.dll',
         'Qt6Svg.dll', 'Qt6Widgets.dll']
for filename in files:
    target = 'citron-mods-by-yngvifreyr1810.exe' if filename == 'citron.exe' else filename
    shutil.copy2(args.app / filename, destination / target)
for directory in ['iconengines', 'imageformats', 'platforms', 'styles', 'tls']:
    shutil.copytree(args.app / directory, destination / directory)
shutil.copy2(source / 'LICENSE', destination / 'LICENSE')
shutil.copytree(source / 'LICENSES', destination / 'LICENSES')
for library in ['libc++', 'libunwind', 'libusb']:
    shutil.copytree(args.runtime_licenses / library, destination / 'LICENSES' / library)
shutil.copy2(source / 'docs/THIRD-PARTY-RUNTIME.md', destination / 'THIRD-PARTY-RUNTIME.md')
shutil.copy2(source / 'README.md', destination / 'README.md')
shutil.copy2(source / 'docs/releases/Beta-v0.1.md', destination / 'RELEASE-NOTES.md')
(destination / 'user').mkdir()
(destination / 'SOURCE.txt').write_text(
    'Corresponding source: https://github.com/yngvifreyr1810/citron-w-graphic-mods\n'
    'Tag: v0.1-beta\nCommit: ' + args.commit + '\n'
    'Build instructions: docs/BETA-BUILD.md in that source tree.\n', encoding='utf-8')
manifest = {p.relative_to(destination).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in destination.rglob('*') if p.is_file()}
(destination / 'SHA256-MANIFEST.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
archive = args.out / (name + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    z.writestr(name + '/user/', b'')
    for p in destination.rglob('*'):
        if p.is_file(): z.write(p, name + '/' + p.relative_to(destination).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert [p for p in z.namelist() if '/user/' in p] == [name + '/user/']
print(archive)
print(hashlib.sha256(archive.read_bytes()).hexdigest())
