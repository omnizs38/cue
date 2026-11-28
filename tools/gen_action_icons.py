"""Embed the supplied SVG icons on their exact two-unit pixel grid.

Authoring-only dependencies: Inkscape CLI and Pillow. The application reads
compiled bitmaps and never parses SVGs or loads an extra library at runtime.
Run: python3 tools/gen_action_icons.py
"""
from pathlib import Path
import subprocess, tempfile
from PIL import Image

root = Path(__file__).resolve().parents[1]
names = ['save-frame', 'return', 'info', 'open-file', 'close-video', 'about-cue']
lines = ['// Generated from assets/icons/*.svg by tools/gen_action_icons.py.',
         '// Each cell is half an art pixel. Preserve SVGs as the source of truth.',
         'static const BitmapDef kActionIcons[] = {']
with tempfile.TemporaryDirectory() as temp:
    for name in names:
        png = Path(temp) / (name + '.png')
        subprocess.run(['inkscape', str(root / 'assets/icons' / (name + '.svg')),
                        '--export-type=png', '--export-filename=' + str(png),
                        '--export-width=32', '--export-height=32'],
                       check=True, capture_output=True)
        alpha = Image.open(png).convert('RGBA').getchannel('A')
        assert set(alpha.tobytes()) <= {0,255}, (name, 'SVG left the two-unit grid')
        alpha = alpha.crop(alpha.getbbox())
        cells = ''.join('#' if a else '.' for a in alpha.tobytes())
        lines.append('  {"cue:%s", %d, %d, "%s"},' % (name, *alpha.size, cells))
lines.append('};')
(root / 'src/ui/action_icons.inc').write_text('\n'.join(lines) + '\n')
print('Embedded six SVG icons without smoothing or runtime dependencies.')
