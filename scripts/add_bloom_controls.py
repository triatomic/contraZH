"""Add bloom controls to a Contra game folder, or stage them with --output-dir."""

import argparse
from datetime import datetime
from pathlib import Path
import re
import shutil


def window_block(text, name):
    lines = text.splitlines(keepends=True)
    hit = next(i for i, line in enumerate(lines)
               if line.strip() == 'NAME = "OptionsMenu.wnd:%s";' % name)
    start = hit
    while lines[start].strip() != "WINDOW":
        start -= 1
    indent = lines[start][:len(lines[start]) - len(lines[start].lstrip())]
    end = next(i for i in range(hit + 1, len(lines))
               if lines[i].rstrip("\r\n") == indent + "END")
    return "".join(lines[start - 1:end + 1])


def place(block, rect):
    return re.sub(r"UPPERLEFT: \d+ \d+, BOTTOMRIGHT: \d+ \d+",
                  "UPPERLEFT: %d %d, BOTTOMRIGHT: %d %d" % rect, block)


def patch_window(text):
    if 'NAME = "OptionsMenu.wnd:TextEntryBloomThreshold";' in text:
        return text
    entry = window_block(text, "TextEntryBloomStrength")
    label = window_block(text, "ShadowsLabel")
    additions = ""
    for name, y in (("Threshold", 203), ("Size", 226)):
        new_label = label.replace("ShadowsLabel", "Bloom%sLabel" % name)
        new_label = re.sub(r'TEXT = "[^"]*";', 'TEXT = "GUI:Bloom%s";' % name, new_label)
        additions += place(new_label, (160, y, 335, y + 20))
        new_entry = entry.replace("BloomStrength", "Bloom" + name)
        additions += place(new_entry, (339, y, 388, y + 20))
    for name, y in (("CheckShadowMap", 249), ("CheckSoftParticles", 272),
                    ("CheckAmbientOcclusion", 295), ("SuperSamplingLabel", 318),
                    ("ComboBoxSuperSampling", 318)):
        old = window_block(text, name)
        rect = re.search(r"UPPERLEFT: (\d+) \d+, BOTTOMRIGHT: (\d+) \d+", old)
        text = text.replace(old, place(old, (int(rect[1]), y, int(rect[2]), y + 20)), 1)
    return text.replace(entry, entry + additions, 1)


def patch_strings(text):
    newline = "\r\n" if "\r\n" in text else "\n"
    entries = {
        "GUI:BloomThreshold": "Threshold %",
        "GUI:BloomSize": "Size %",
        "TOOLTIP:BloomThreshold": "0 to 100. Only brightness above this value feeds the glow. Requires Direct3D 9.",
        "TOOLTIP:BloomSize": "25 to 400. Glow radius as a percentage of its original size. 100 keeps the original size.",
    }
    for key, value in entries.items():
        if not re.search(r"^" + re.escape(key) + r"\s*$", text, re.MULTILINE):
            text += newline + newline.join((key, '"%s"' % value, "END", ""))
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    sources = (args.game_dir / "Window/Menus/OptionsMenu.wnd", args.game_dir / "Data/Generals.str")
    updated = [patch_window(sources[0].read_bytes().decode("latin-1")),
               patch_strings(sources[1].read_bytes().decode("latin-1"))]
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
    backup_dir = args.game_dir.parent / "_bak_to_delete" / ("bloom-controls-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    for source, content in zip(sources, updated):
        target = args.output_dir / source.name if args.output_dir else source
        data = content.encode("latin-1")
        if target.exists() and target.read_bytes() == data:
            continue
        if not args.output_dir:
            backup_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, backup_dir / source.name)
        target.write_bytes(data)
        print(target)


if __name__ == "__main__":
    main()
