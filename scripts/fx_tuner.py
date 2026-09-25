# Tunes the electric, laser and laser ground glow shaders with live previews, and renders the pictures
# for docs/Electric-&-Laser-Shading.md. The math follows softparticle.hlsl and laserglow.hlsl, and the
# noise matches W3DSoftParticles::createNoise.
#
#   python scripts/fx_tuner.py            opens the tuner
#   python scripts/fx_tuner.py --docs     writes the doc pictures into docs/images
#
# Needs numpy and Pillow, and PyQt5 for the tuner. The tuner reads and writes the keys in
# GameData.ini, which cheat builds reload while a map runs.

import argparse
import os
import re
import shutil
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

DOCS_IMAGES = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'images')
DEFAULT_GAMEDATA = 'C:/Games/contra/contraprerelease/Data/INI/GameData.ini'

def load_font(name, size):
    path = os.path.join('C:/Windows/Fonts', name)
    return ImageFont.truetype(path, size) if os.path.exists(path) else ImageFont.load_default()

FONT = load_font('segoeui.ttf', 15)
FONT_BOLD = load_font('segoeuib.ttf', 15)
BG = np.array([0.04, 0.05, 0.08])
PAGE = (255, 255, 255)
INK = (40, 40, 40)
DIM = (120, 120, 120)

# ---------------------------------------------------------------- noise

NOISE_SIZE = 64

def hash_lattice(x, y, seed):
    x = np.asarray(x, dtype=np.uint32)
    y = np.asarray(y, dtype=np.uint32)
    with np.errstate(over='ignore'):
        h = x * np.uint32(374761393) + y * np.uint32(668265263) + np.uint32((seed * 2246822519) & 0xffffffff)
        h = (h ^ (h >> np.uint32(13))) * np.uint32(1274126177)
        return h ^ (h >> np.uint32(16))

def value_noise(cells, seed):
    idx = np.arange(NOISE_SIZE)
    x, y = np.meshgrid(idx, idx)
    fx = (x * cells).astype(np.float64) / NOISE_SIZE
    fy = (y * cells).astype(np.float64) / NOISE_SIZE
    x0 = fx.astype(np.int64)
    y0 = fy.astype(np.int64)
    x1 = (x0 + 1) % cells
    y1 = (y0 + 1) % cells
    tx = fx - x0
    ty = fy - y0
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    v = lambda a, b: (hash_lattice(a, b, seed) & 0xffff) / 65535.0
    top = v(x0, y0) + (v(x1, y0) - v(x0, y0)) * tx
    bottom = v(x0, y1) + (v(x1, y1) - v(x0, y1)) * tx
    return top + (bottom - top) * ty

NOISE = np.stack([np.floor((0.65 * value_noise(8, c) + 0.35 * value_noise(16, c + 3)) * 255 + 0.5) / 255 for c in range(3)], axis=-1)

def tex_noise(u, v):
    # Bilinear, wrapped, with D3D texel centres.
    px = u * NOISE_SIZE - 0.5
    py = v * NOISE_SIZE - 0.5
    x0 = np.floor(px).astype(np.int64)
    y0 = np.floor(py).astype(np.int64)
    fx = (px - x0)[..., None]
    fy = (py - y0)[..., None]
    x0 %= NOISE_SIZE
    y0 %= NOISE_SIZE
    x1 = (x0 + 1) % NOISE_SIZE
    y1 = (y0 + 1) % NOISE_SIZE
    top = NOISE[y0, x0] * (1 - fx) + NOISE[y0, x1] * fx
    bottom = NOISE[y1, x0] * (1 - fx) + NOISE[y1, x1] * fx
    return top * (1 - fy) + bottom * fy

def jump_offset(jump):
    return ((hash_lattice(jump, 0, 7) & 0xffff) / 65536.0, (hash_lattice(jump, 1, 7) & 0xffff) / 65536.0)

# ---------------------------------------------------------------- textures

class Texture:
    """An image file sampled bilinearly, clamped, or wrapped along u."""

    def __init__(self, path):
        self.name = os.path.basename(path)
        self.a = np.asarray(Image.open(path).convert('RGBA'), dtype=np.float64) / 255.0

    def sample(self, u, v, wrap_u=False):
        h, w = self.a.shape[:2]
        if wrap_u:
            u = np.mod(u, 1.0)
        px = np.clip(u * w - 0.5, 0, w - 1)
        py = np.clip(v * h - 0.5, 0, h - 1)
        x0 = np.floor(px).astype(np.int64)
        y0 = np.floor(py).astype(np.int64)
        x1 = np.minimum(x0 + 1, w - 1)
        y1 = np.minimum(y0 + 1, h - 1)
        fx = (px - x0)[..., None]
        fy = (py - y0)[..., None]
        top = self.a[y0, x0] * (1 - fx) + self.a[y0, x1] * fx
        bottom = self.a[y1, x0] * (1 - fx) + self.a[y1, x1] * fx
        return top * (1 - fy) + bottom * fy

def flare_texture(u, v):
    # A white lightning flare: soft glow, hot core and six thin rays.
    dx = u - 0.5
    dy = v - 0.5
    r = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)
    rays = np.zeros_like(r)
    for k in range(6):
        a = k * np.pi / 3 + 0.3
        d = np.abs(np.angle(np.exp(1j * (theta - a))))
        rays += np.exp(-(d * r * 60) ** 2) * np.clip(1 - r / 0.5, 0, 1) ** 1.5
    glow = np.exp(-(r / 0.2) ** 2) * 0.8 + np.exp(-(r / 0.05) ** 2) * 0.6
    edge = np.clip((0.5 - r) / 0.08, 0, 1)
    i = np.clip(glow + 0.7 * rays, 0, 1) * edge
    return np.stack([i, i, i, i], axis=-1)

def beam_texture(side, colour):
    prof = np.clip(1 - np.abs(side), 0, 1) ** 1.5
    return np.stack([prof * colour[0], prof * colour[1], prof * colour[2], prof], -1)

def to_image(colour, bg=BG, ss=1):
    img = np.clip(bg + colour, 0, 1)
    if ss > 1:
        h, w = img.shape[0] // ss, img.shape[1] // ss
        img = img.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))
    return Image.fromarray((img * 255 + 0.5).astype(np.uint8))

# ---------------------------------------------------------------- electric

ELECTRIC_DEFAULTS = dict(arcs=1.5, sharpness=10.0, noise=40.0, jitter=0.03, flicker=0.6, rate=15.0)
CAMERA_DISTANCE = 250.0 / np.sin(np.radians(37.5))
PX_PER_UNIT = 5.3

def render_electric(p=None, electric=True, size=30.0, colour=(0.35, 0.55, 1.0), jump=3, depth=None,
                    tile=160, ss=2, centre=(37.0, 11.0), widen=True, texture=None, camera_distance=CAMERA_DISTANCE):
    q = dict(ELECTRIC_DEFAULTS)
    q.update(p or {})
    depth = camera_distance if depth is None else depth
    res = tile * ss
    scale = PX_PER_UNIT * camera_distance / depth
    pix = (np.arange(res) + 0.5) / ss - tile / 2
    X, Y = np.meshgrid(pix / scale, -pix / scale)
    u = X / size + 0.5
    v = 0.5 - Y / size
    inside = (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
    diffuse = np.array(colour)
    sample = texture.sample if texture is not None else flare_texture

    if not electric:
        colour_out = sample(np.clip(u, 0, 1), np.clip(v, 0, 1))[..., :3] * diffuse
    else:
        ox, oy = jump_offset(jump)
        fx = (centre[0] + X) / max(q['noise'], 0.01) + ox
        fy = (centre[1] + Y) / max(q['noise'], 0.01) + oy
        a = tex_noise(fx, fy)
        b = tex_noise(fx * 2 + oy, fy * 2 + ox)
        uu = np.clip(u + (a[..., 0] - 0.5) * q['jitter'], 0, 1)
        vv = np.clip(v + (a[..., 1] - 0.5) * q['jitter'], 0, 1)
        colour_out = sample(uu, vv)[..., :3] * diffuse
        sharp = q['sharpness'] * (min(1.0, camera_distance / depth) if widen else 1.0)
        arcs = np.clip(1 - np.abs(np.stack([a[..., 2], b[..., 1]], -1) - 0.5) * sharp, 0, 1) ** 2
        arc = arcs.max(-1)
        peak = colour_out.max(-1)
        hue = colour_out / np.maximum(peak, 0.001)[..., None]
        reach = np.sqrt(peak)
        strobe = 1 + q['flicker'] * (b[..., 2] - 0.5)
        colour_out = colour_out * strobe[..., None] + (hue + 1) * (0.5 * arc * reach * q['arcs'])[..., None]

    return to_image(colour_out * inside[..., None], ss=ss)

# ---------------------------------------------------------------- laser

LASER_DEFAULTS = dict(core=1.2, width=0.25, shimmer=0.3, pulse=0.4, pulse_size=120.0, speed=400.0)
LASER_PX_PER_UNIT = 2.0

def render_laser(p=None, shaded=True, width_px=720, beam_units=20.0, t=0.0, ss=2, pad=14, colour=(1.0, 0.22, 0.15), texture=None):
    q = dict(LASER_DEFAULTS)
    q.update(p or {})
    height_px = int(beam_units * LASER_PX_PER_UNIT) + 2 * pad
    W, H = width_px * ss, height_px * ss
    xs = (np.arange(W) + 0.5) / ss
    ys = (np.arange(H) + 0.5) / ss - height_px / 2
    X, Y = np.meshgrid(xs / LASER_PX_PER_UNIT, ys / LASER_PX_PER_UNIT)
    side = Y / (beam_units / 2)
    inside = np.abs(side) <= 1
    if texture is not None:
        # Along the beam is u, repeating every four beam widths, and across is v.
        texel = texture.sample(X / (beam_units * 4), np.clip(side * 0.5 + 0.5, 0, 1), wrap_u=True)
        colour_out = texel[..., :3] * np.array(colour)
    else:
        colour_out = beam_texture(side, colour)[..., :3]
    if shaded:
        scale = 1.0 / max(q['pulse_size'], 0.01)
        travel = np.fmod(t * q['speed'] * scale, 10.0)
        along = X * scale
        a = tex_noise(along - travel, np.full_like(along, 0.25))
        b = tex_noise(along * 2.3 - travel * 1.5, np.full_like(along, 0.75))
        pulse = 1 + q['pulse'] * (a[..., 0] + b[..., 1] - 1)
        waver = min(max(q['shimmer'], 0), 1)
        core = np.exp2(side * side * (-3.0 / max(q['width'], 0.01) ** 2) * (1 + waver * (b[..., 2] - 0.5)))
        peak = colour_out.max(-1)
        hot = colour_out * 0.35 + peak[..., None] * 0.65
        edge = np.clip(4 - 4 * np.abs(side), 0, 1)
        colour_out = (colour_out * edge[..., None] + hot * (core * q['core'])[..., None]) * pulse[..., None]
    return to_image(colour_out * inside[..., None], ss=ss)

# ---------------------------------------------------------------- laser ground glow

GLOW_DEFAULTS = dict(falloff=2.0, wrap=0.5, reach=40.0, pulse=0.4, pulse_size=120.0, speed=400.0)
GLOW_TILE_UNITS = 140.0

def ground(X, Y, hills):
    z = np.zeros_like(X)
    if hills:
        for (cx, cy, h, r) in [(-25, 22, 14, 16), (28, -20, 12, 14), (30, 30, 9, 12), (-35, -25, 8, 11)]:
            z += h * np.exp(-((X - cx) ** 2 + (Y - cy) ** 2) / (2 * r * r))
    return z

def render_glow(p=None, beam=((-70, 0, 6), (70, 0, 6)), hills=False, ss=2, glow_rgb=(1.5, 0.4, 0.25), draw_beam=True, t=0.0, px=220):
    q = dict(GLOW_DEFAULTS)
    q.update(p or {})
    res = px * ss
    coords = ((np.arange(res) + 0.5) / res - 0.5) * GLOW_TILE_UNITS
    X, Y = np.meshgrid(coords, -coords)
    Z = ground(X, Y, hills)
    step = coords[1] - coords[0]
    gy, gx = np.gradient(Z, -step, step)
    n = np.stack([-gx, -gy, np.ones_like(Z)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)

    sun = np.array([-0.4, 0.5, 0.77])
    sun /= np.linalg.norm(sun)
    grain = tex_noise(X / 30.0, Y / 30.0)[..., 0] * 0.25 + 0.85
    base = np.array([0.52, 0.47, 0.36]) * (np.clip((n * sun).sum(-1), 0, 1) * 0.65 + 0.35)[..., None] * grain[..., None]

    start = np.array(beam[0], dtype=float)
    span = np.array(beam[1], dtype=float) - start
    world = np.stack([X, Y, Z], -1)
    tb = np.clip(((world - start) * span).sum(-1) / (span @ span), 0, 1)
    to_beam = start + span * tb[..., None] - world
    dist = np.linalg.norm(to_beam, axis=-1)
    falloff = np.clip(1 - dist / max(q['reach'], 0.01), 0, 1) ** max(q['falloff'], 0.1)
    facing = np.clip((n * to_beam).sum(-1) / np.maximum(dist, 0.001), 0, 1)
    wrap = min(max(q['wrap'], 0), 1)
    facing = facing + (1 - facing) * wrap
    scale = 1.0 / max(q['pulse_size'], 0.01)
    travel = np.fmod(t * q['speed'] * scale, 10.0)
    along = tb * np.linalg.norm(span) * scale
    a = tex_noise(along - travel, np.full_like(along, 0.25))
    b = tex_noise(along * 2.3 - travel * 1.5, np.full_like(along, 0.75))
    pulse = 1 + q['pulse'] * (a[..., 0] + b[..., 1] - 1)
    light = np.array(glow_rgb) * (falloff * facing * pulse)[..., None]
    img = np.clip(base * (1 + light), 0, 1)
    img = img.reshape(px, ss, px, ss, 3).mean(axis=(1, 3))
    out = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    if draw_beam:
        d = ImageDraw.Draw(out)
        to_px = lambda wx, wy: ((wx / GLOW_TILE_UNITS + 0.5) * px, (0.5 - wy / GLOW_TILE_UNITS) * px)
        d.line([to_px(*beam[0][:2]), to_px(*beam[1][:2])], fill=(255, 235, 225), width=2)
    return out

# ---------------------------------------------------------------- doc layout

def label_value(key, value, default):
    text = '%s = %g' % (key, value)
    return text + ('  (default)' if value == default else '')

def grid(rows, tile_w, tile_h, title_w=190, gap=12, caption_h=24, top=10):
    """rows: list of (row title, [(image, caption), ...])"""
    cols = max(len(r[1]) for r in rows)
    width = title_w + cols * (tile_w + gap) + gap
    height = top + len(rows) * (tile_h + caption_h + gap)
    page = Image.new('RGB', (width, height), PAGE)
    d = ImageDraw.Draw(page)
    y = top
    for title, tiles in rows:
        d.text((gap, y + tile_h / 2 - 10), title, font=FONT_BOLD, fill=INK)
        x = title_w
        for img, caption in tiles:
            page.paste(img, (x, y))
            tw = d.textlength(caption, font=FONT)
            d.text((x + (tile_w - tw) / 2, y + tile_h + 2), caption, font=FONT, fill=INK if 'default' in caption else DIM)
            x += tile_w + gap
        y += tile_h + caption_h + gap
    return page

def stack(rows, gap=10, caption_h=22, top=8):
    """rows: list of (image, caption) stacked vertically, caption above each."""
    width = max(img.width for img, _ in rows) + 2 * gap
    height = top + sum(img.height + caption_h + gap for img, _ in rows)
    page = Image.new('RGB', (width, height), PAGE)
    d = ImageDraw.Draw(page)
    y = top
    for img, caption in rows:
        d.text((gap, y), caption, font=FONT, fill=INK if 'default' in caption else DIM)
        y += caption_h
        page.paste(img, (gap, y))
        y += img.height + gap
    return page

def save(img, name):
    path = os.path.join(DOCS_IMAGES, name)
    img.save(path, optimize=True)
    print(name, os.path.getsize(path) // 1024, 'KB')

def save_gif(frames, name, ms):
    path = os.path.join(DOCS_IMAGES, name)
    pal = [f.convert('P', palette=Image.ADAPTIVE, colors=64) for f in frames]
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=ms, loop=0, optimize=True)
    print(name, os.path.getsize(path) // 1024, 'KB')

def electric_images():
    T = 160
    colours = [('blue', (0.35, 0.55, 1.0)), ('green', (0.35, 1.0, 0.45)), ('red', (1.0, 0.3, 0.3))]
    rows = [('Plain sprite', [(render_electric(electric=False, colour=c), name) for name, c in colours]),
            ('Electric shading', [(render_electric(colour=c), name) for name, c in colours])]
    save(grid(rows, T, T), 'electric-overview.png')

    D = ELECTRIC_DEFAULTS
    rows = [
        ('ElectricArcs', [(render_electric({'arcs': v}), label_value('', v, D['arcs'])[3:]) for v in (0.0, 0.5, 1.5, 4.0)]),
        ('ElectricArcSharpness', [(render_electric({'sharpness': v}), label_value('', v, D['sharpness'])[3:]) for v in (3.0, 10.0, 20.0, 30.0)]),
        ('ElectricNoiseSize', [(render_electric({'noise': v}), label_value('', v, D['noise'])[3:]) for v in (15.0, 40.0, 80.0, 150.0)]),
    ]
    save(grid(rows, T, T), 'electric-arcs.png')

    rows = [
        ('ElectricJitter', [(render_electric({'jitter': v, 'arcs': 0.0}), label_value('', v, D['jitter'])[3:]) for v in (0.0, 0.03, 0.06, 0.1)]),
        ('ElectricFlicker', [(render_electric({'flicker': v, 'arcs': 0.0}), label_value('', v, D['flicker'])[3:]) for v in (0.0, 0.6, 1.2, 2.0)]),
    ]
    save(grid(rows, T, T), 'electric-jitter-flicker.png')

    frames_row = [(render_electric(jump=j), 'jump %d' % (k + 1)) for k, j in enumerate((10, 11, 12, 13))]
    save(grid([('Consecutive crackles', frames_row)], T, T), 'electric-crackles.png')

    frames = []
    for f in range(30):
        tiles = []
        for rate in (5, 15, 30):
            jump = int(f / 30.0 * rate)
            tiles.append((render_electric(jump=jump, tile=120), 'ElectricRate = %d%s' % (rate, ' (default)' if rate == 15 else '')))
        frames.append(grid([('', tiles)], 120, 120, title_w=12, gap=30))
    save_gif(frames, 'electric-rate.gif', 33)

    # The same flare further from the camera, pixels enlarged so both tiles fill the same space.
    def far(mult, widen):
        img = render_electric(depth=CAMERA_DISTANCE * mult, ss=1, tile=160 // mult, widen=widen)
        return img.resize((160, 160), Image.NEAREST)
    rows = [(title, [(far(m, widen), 'distance x%d, pixels x%d' % (m, m) if m > 1 else 'distance x1') for m in (1, 2, 4)])
            for widen, title in ((True, 'Arcs widen'), (False, 'Fixed width'))]
    save(grid(rows, 160, 160), 'electric-distance.png')

def laser_images():
    D = LASER_DEFAULTS
    save(stack([(render_laser(shaded=False), 'Plain beam'), (render_laser(), 'Laser shading, defaults')]), 'laser-overview.png')

    rows = [(render_laser({'core': v}), label_value('LaserCore', v, D['core'])) for v in (0.0, 1.2, 3.0)]
    rows += [(render_laser({'width': v}), label_value('LaserCoreWidth', v, D['width'])) for v in (0.1, 0.25, 0.6)]
    save(stack(rows), 'laser-core.png')

    rows = [(render_laser({'shimmer': v, 'pulse': 0.0}), label_value('LaserShimmer', v, D['shimmer']) + ', LaserPulse = 0') for v in (0.0, 0.3, 1.0)]
    save(stack(rows), 'laser-shimmer.png')

    rows = [(render_laser({'pulse': v}), label_value('LaserPulse', v, D['pulse'])) for v in (0.0, 0.4, 1.0)]
    rows += [(render_laser({'pulse_size': v, 'pulse': 0.8}), label_value('LaserPulseSize', v, D['pulse_size']) + ', LaserPulse = 0.8') for v in (40.0, 120.0, 400.0)]
    save(stack(rows), 'laser-pulse.png')

    frames = []
    for f in range(30):
        t = f / 30.0
        rows = [(render_laser({'speed': v, 'pulse': 0.8}, t=t, width_px=480), label_value('LaserPulseSpeed', v, D['speed']) + ', LaserPulse = 0.8') for v in (0.0, 400.0, 1200.0)]
        frames.append(stack(rows))
    save_gif(frames, 'laser-speed.gif', 33)

def glow_images():
    D = GLOW_DEFAULTS
    T = 220
    skim = ((-70, 0, 6), (70, 0, 6))
    climb = ((-70, 0, 6), (70, 0, 150))
    rows = [
        ('Beam height', [(render_glow(beam=skim), 'skimming, 6 up'), (render_glow(beam=((-70, 0, 30), (70, 0, 30))), 'level, 30 up'), (render_glow(beam=climb), 'climbing to 150')]),
        ('LaserGroundGlowFalloff', [(render_glow({'falloff': v}), label_value('', v, D['falloff'])[3:]) for v in (1.0, 2.0, 4.0)]),
        ('LaserGroundGlowWrap', [(render_glow({'wrap': v}, hills=True), label_value('', v, D['wrap'])[3:]) for v in (0.0, 0.5, 1.0)]),
        ('LaserGroundGlowRadius', [(render_glow({'reach': v}), '%d' % v) for v in (20.0, 40.0, 70.0)]),
    ]
    save(grid(rows, T, T, title_w=215), 'laser-glow.png')

# ---------------------------------------------------------------- GameData.ini

# Tuner control -> GameData.ini key. The tuner edits only these.
GAMEDATA_KEYS = [
    ('electric', 'arcs', 'ElectricArcs'),
    ('electric', 'sharpness', 'ElectricArcSharpness'),
    ('electric', 'noise', 'ElectricNoiseSize'),
    ('electric', 'jitter', 'ElectricJitter'),
    ('electric', 'flicker', 'ElectricFlicker'),
    ('electric', 'rate', 'ElectricRate'),
    ('laser', 'core', 'LaserCore'),
    ('laser', 'width', 'LaserCoreWidth'),
    ('laser', 'shimmer', 'LaserShimmer'),
    ('laser', 'pulse', 'LaserPulse'),
    ('laser', 'pulse_size', 'LaserPulseSize'),
    ('laser', 'speed', 'LaserPulseSpeed'),
    ('glow', 'falloff', 'LaserGroundGlowFalloff'),
    ('glow', 'wrap', 'LaserGroundGlowWrap'),
    ('glow', 'radius', 'LaserGroundGlowRadius'),
]

def key_pattern(key):
    return re.compile(r'^([ \t]*' + key + r'[ \t]*=[ \t]*)([^\s;]+)', re.M | re.I)

def read_gamedata(path):
    text = open(path, encoding='latin-1').read()
    values = {}
    for key in [k for _, _, k in GAMEDATA_KEYS] + ['CameraHeight', 'CameraPitch']:
        m = key_pattern(key).search(text)
        if m:
            try:
                values[key] = float(m.group(2).rstrip('%'))
            except ValueError:
                pass
    return values

def write_gamedata(path, values):
    """Replaces each key's value in place, keeping its comment, or adds it before the GameData block's End."""
    raw = open(path, 'rb').read().decode('latin-1')
    nl = '\r\n' if '\r\n' in raw else '\n'
    text = raw
    for key, value in values.items():
        formatted = '%g' % value
        pattern = key_pattern(key)
        if pattern.search(text):
            text = pattern.sub(lambda m: m.group(1) + formatted, text, count=1)
            continue
        block = re.search(r'^GameData\b', text, re.M)
        end = re.search(r'^End\b', text[block.end():], re.M) if block else None
        if end is None:
            raise ValueError('no GameData block to add %s to' % key)
        at = block.end() + end.start()
        text = text[:at] + '  %s = %s%s' % (key, formatted, nl) + text[at:]
    open(path, 'wb').write(text.encode('latin-1'))

# ---------------------------------------------------------------- tuner

# Hover help for each GameData.ini key.
KEY_HELP = {
    'ElectricArcs': 'Arc brightness. 0 turns the arcs off. Past 4 the arcs wash the sprite white.',
    'ElectricArcSharpness': 'Arc thickness. Low values give broad glowing bands, high values thin threads.',
    'ElectricNoiseSize': 'World units across one tile of arc noise. Smaller gives more, closer arcs.',
    'ElectricJitter': 'How far the texture jumps each crackle, in texture widths.',
    'ElectricFlicker': 'How far brightness swings, as a fraction. 0 is steady. Past 2 dim patches go black.',
    'ElectricRate': 'Crackles per second. 0 freezes the arcs.',
    'LaserCore': 'Core brightness. 0 turns the core off.',
    'LaserCoreWidth': "Core width, as a fraction of the beam's half width.",
    'LaserShimmer': "How far the core's width wavers along the beam. 0 is steady, 1 at most.",
    'LaserPulse': 'How far brightness swings along the beam, as a fraction. 0 is steady.',
    'LaserPulseSize': 'World units across one tile of pulse noise. Smaller gives more, shorter pulses.',
    'LaserPulseSpeed': 'World units a second the pulses travel towards the target. 0 freezes them.',
    'LaserGroundGlowFalloff': 'How fast the light fades with distance from the beam, as a power.',
    'LaserGroundGlowWrap': 'Light on ground facing away from the beam, from 0 to 1.',
    'LaserGroundGlowRadius': "How far the light reaches from the beam. 0 takes 1.25 times the laser's OuterBeamWidth, at least 15.",
}

DARK_STYLE = """
QToolTip { color: #e0e0e0; background-color: #2d2d30; border: 1px solid #555; padding: 4px; }
QGroupBox { border: 1px solid #3f3f46; border-radius: 4px; margin-top: 14px; padding: 8px 6px 6px 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #9cdcfe; }
QTabWidget::pane { border: 1px solid #3f3f46; }
QTabBar::tab { background: #2d2d30; color: #c8c8c8; padding: 6px 16px; border: 1px solid #3f3f46; border-bottom: none; }
QTabBar::tab:selected { background: #3e3e42; color: #ffffff; }
QLabel#preview { background-color: #0a0d14; border: 1px solid #3f3f46; }
QLabel#key { font-weight: bold; }
QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #707070; border-radius: 2px; background: #1e1e1e; }
QCheckBox::indicator:checked { background: #007acc; border-color: #3ea0e8; }
"""

def dark_palette(QtGui, QtCore):
    palette = QtGui.QPalette()
    colours = {
        QtGui.QPalette.Window: (45, 45, 48),
        QtGui.QPalette.WindowText: (220, 220, 220),
        QtGui.QPalette.Base: (30, 30, 30),
        QtGui.QPalette.AlternateBase: (45, 45, 48),
        QtGui.QPalette.ToolTipBase: (45, 45, 48),
        QtGui.QPalette.ToolTipText: (220, 220, 220),
        QtGui.QPalette.Text: (220, 220, 220),
        QtGui.QPalette.Button: (55, 55, 58),
        QtGui.QPalette.ButtonText: (220, 220, 220),
        QtGui.QPalette.BrightText: (255, 80, 80),
        QtGui.QPalette.Link: (86, 156, 214),
        QtGui.QPalette.Highlight: (0, 122, 204),
        QtGui.QPalette.HighlightedText: (255, 255, 255),
    }
    for role, rgb in colours.items():
        palette.setColor(role, QtGui.QColor(*rgb))
    for role in (QtGui.QPalette.Text, QtGui.QPalette.ButtonText, QtGui.QPalette.WindowText):
        palette.setColor(QtGui.QPalette.Disabled, role, QtGui.QColor(120, 120, 120))
    return palette

def run_tuner(gamedata_path):
    import sys
    from PyQt5 import QtCore, QtGui, QtWidgets

    def decimals(step):
        text = '%g' % step
        return len(text.split('.')[1]) if '.' in text else 0

    class FloatControl(QtWidgets.QWidget):
        """A labelled slider and spin box kept in step, with a reset to the default."""

        changed = QtCore.pyqtSignal()

        def __init__(self, label, lo, hi, step, default, key=None):
            super().__init__()
            self.lo, self.step, self.default = lo, step, default
            layout = QtWidgets.QHBoxLayout(self)
            layout.setContentsMargins(0, 1, 0, 1)

            name = QtWidgets.QLabel(label)
            name.setMinimumWidth(190)
            if key:
                name.setObjectName('key')
                name.setToolTip(KEY_HELP.get(key, ''))
            layout.addWidget(name)

            self.slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
            self.slider.setRange(0, int(round((hi - lo) / step)))
            self.slider.setMinimumWidth(220)
            layout.addWidget(self.slider)

            # The box takes values past the slider's range, so a loaded value is never clamped.
            self.spin = QtWidgets.QDoubleSpinBox()
            self.spin.setDecimals(decimals(step))
            self.spin.setSingleStep(step)
            self.spin.setRange(min(lo, 0.0), hi * 10 + 10)
            self.spin.setMinimumWidth(80)
            layout.addWidget(self.spin)

            reset = QtWidgets.QToolButton()
            reset.setText('↺')
            reset.setToolTip('Default: %g' % default)
            reset.clicked.connect(lambda: self.setValue(default))
            layout.addWidget(reset)

            self.slider.valueChanged.connect(self._from_slider)
            self.spin.valueChanged.connect(self._from_spin)
            self.setValue(default)

        def _from_slider(self, position):
            self.spin.blockSignals(True)
            self.spin.setValue(self.lo + position * self.step)
            self.spin.blockSignals(False)
            self.changed.emit()

        def _from_spin(self, value):
            self.slider.blockSignals(True)
            self.slider.setValue(int(round((value - self.lo) / self.step)))
            self.slider.blockSignals(False)
            self.changed.emit()

        def value(self):
            return self.spin.value()

        def setValue(self, value):
            self.spin.setValue(value)

    class Tuner(QtWidgets.QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle('Electric & Laser Shading Tuner')
            self.controls = {}
            self.colours = {'electric': (0.35, 0.55, 1.0), 'laser': (1.0, 0.22, 0.15), 'glow': (1.0, 0.27, 0.17)}
            self.textures = {'electric': None, 'laser': None}
            self.camera_distance = CAMERA_DISTANCE
            self.backed_up = False
            self.start = time.perf_counter()
            self.dirty = True

            central = QtWidgets.QWidget()
            self.setCentralWidget(central)
            outer = QtWidgets.QVBoxLayout(central)

            path_row = QtWidgets.QHBoxLayout()
            path_row.addWidget(QtWidgets.QLabel('GameData.ini'))
            self.path = QtWidgets.QLineEdit(gamedata_path)
            path_row.addWidget(self.path, 1)
            browse = QtWidgets.QPushButton('Browse...')
            browse.clicked.connect(self.browse)
            path_row.addWidget(browse)
            outer.addLayout(path_row)

            self.tabs = QtWidgets.QTabWidget()
            self.tabs.currentChanged.connect(self.mark_dirty)
            outer.addWidget(self.tabs, 1)
            self.previews = []
            self.animate = []
            self.plain = {}
            self.build_electric()
            self.build_laser()
            self.build_glow()

            buttons = QtWidgets.QHBoxLayout()
            for text, slot, tip in (('Load from GameData.ini', self.load_values, 'Read the keys from the file'),
                                    ('Save to GameData.ini', self.save_values, 'Write the keys into the file (Ctrl+S)'),
                                    ('Copy lines', self.copy_lines, 'Put the keys on the clipboard')):
                button = QtWidgets.QPushButton(text)
                button.setToolTip(tip)
                button.clicked.connect(slot)
                buttons.addWidget(button)
            buttons.addStretch(1)
            outer.addLayout(buttons)
            QtWidgets.QShortcut(QtGui.QKeySequence('Ctrl+S'), self, self.save_values)

            self.timer = QtCore.QTimer(self)
            self.timer.timeout.connect(self.tick)
            self.timer.start(40)

            if os.path.exists(gamedata_path):
                self.load_values()

        # ------------------------------------------------ building

        def mark_dirty(self, *_):
            self.dirty = True

        def add_control(self, layout, group, name, label, lo, hi, step, default, key=None):
            control = FloatControl(label, lo, hi, step, default, key)
            control.changed.connect(self.mark_dirty)
            self.controls[(group, name)] = control
            layout.addWidget(control)

        def new_tab(self, title):
            page = QtWidgets.QWidget()
            row = QtWidgets.QHBoxLayout(page)
            left = QtWidgets.QVBoxLayout()
            row.addLayout(left)
            preview = QtWidgets.QLabel()
            preview.setObjectName('preview')
            preview.setAlignment(QtCore.Qt.AlignCenter)
            preview.setMinimumSize(380, 380)
            row.addWidget(preview, 1, QtCore.Qt.AlignTop)
            self.tabs.addTab(page, title)
            self.previews.append(preview)
            keys = QtWidgets.QGroupBox('GameData.ini')
            keys_layout = QtWidgets.QVBoxLayout(keys)
            left.addWidget(keys)
            view = QtWidgets.QGroupBox('Preview')
            view_layout = QtWidgets.QVBoxLayout(view)
            left.addWidget(view)
            left.addStretch(1)
            return keys_layout, view_layout

        def colour_button(self, layout, group, text):
            button = QtWidgets.QPushButton(text)

            def pick():
                current = QtGui.QColor.fromRgbF(*self.colours[group])
                chosen = QtWidgets.QColorDialog.getColor(current, self, text)
                if chosen.isValid():
                    self.colours[group] = (chosen.redF(), chosen.greenF(), chosen.blueF())
                    self.mark_dirty()

            button.clicked.connect(pick)
            layout.addWidget(button)

        def texture_row(self, layout, group):
            row = QtWidgets.QHBoxLayout()
            name = QtWidgets.QLabel('built-in')

            def load():
                path = QtWidgets.QFileDialog.getOpenFileName(self, 'Texture', '', 'Images (*.tga *.dds *.png *.bmp *.jpg);;All (*.*)')[0]
                if not path:
                    return
                try:
                    self.textures[group] = Texture(path)
                    name.setText(self.textures[group].name)
                except Exception as error:
                    QtWidgets.QMessageBox.critical(self, 'Texture', str(error))
                self.mark_dirty()

            def reset():
                self.textures[group] = None
                name.setText('built-in')
                self.mark_dirty()

            for text, slot in (('Texture...', load), ('Built-in', reset)):
                button = QtWidgets.QPushButton(text)
                button.clicked.connect(slot)
                row.addWidget(button)
            row.addWidget(name, 1)
            layout.addLayout(row)

        def check(self, layout, text, checked):
            box = QtWidgets.QCheckBox(text)
            box.setChecked(checked)
            box.toggled.connect(self.mark_dirty)
            layout.addWidget(box)
            return box

        def build_electric(self):
            keys, view = self.new_tab('Electric')
            self.add_control(keys, 'electric', 'arcs', 'ElectricArcs', 0, 6, 0.05, 1.5, 'ElectricArcs')
            self.add_control(keys, 'electric', 'sharpness', 'ElectricArcSharpness', 1, 60, 0.5, 10, 'ElectricArcSharpness')
            self.add_control(keys, 'electric', 'noise', 'ElectricNoiseSize', 5, 300, 1, 40, 'ElectricNoiseSize')
            self.add_control(keys, 'electric', 'jitter', 'ElectricJitter', 0, 0.5, 0.005, 0.03, 'ElectricJitter')
            self.add_control(keys, 'electric', 'flicker', 'ElectricFlicker', 0, 4, 0.05, 0.6, 'ElectricFlicker')
            self.add_control(keys, 'electric', 'rate', 'ElectricRate', 0, 60, 1, 15, 'ElectricRate')
            self.add_control(view, 'electric', 'size', 'Sprite size (world units)', 3, 120, 1, 30)
            self.add_control(view, 'electric', 'distance', 'Camera distance (x default)', 1, 4, 0.25, 1)
            self.colour_button(view, 'electric', 'Particle colour...')
            self.texture_row(view, 'electric')
            self.plain['electric'] = self.check(view, 'Plain sprite (shading off)', False)
            self.animate.append(self.check(view, 'Animate', True))

        def build_laser(self):
            keys, view = self.new_tab('Laser')
            self.add_control(keys, 'laser', 'core', 'LaserCore', 0, 6, 0.05, 1.2, 'LaserCore')
            self.add_control(keys, 'laser', 'width', 'LaserCoreWidth', 0.02, 1, 0.01, 0.25, 'LaserCoreWidth')
            self.add_control(keys, 'laser', 'shimmer', 'LaserShimmer', 0, 1, 0.05, 0.3, 'LaserShimmer')
            self.add_control(keys, 'laser', 'pulse', 'LaserPulse', 0, 3, 0.05, 0.4, 'LaserPulse')
            self.add_control(keys, 'laser', 'pulse_size', 'LaserPulseSize', 10, 1000, 5, 120, 'LaserPulseSize')
            self.add_control(keys, 'laser', 'speed', 'LaserPulseSpeed', 0, 3000, 10, 400, 'LaserPulseSpeed')
            self.add_control(view, 'laser', 'beam', 'Beam width (world units)', 4, 60, 1, 20)
            self.colour_button(view, 'laser', 'Beam colour...')
            self.texture_row(view, 'laser')
            self.plain['laser'] = self.check(view, 'Plain beam (shading off)', False)
            self.animate.append(self.check(view, 'Animate', True))

        def build_glow(self):
            keys, view = self.new_tab('Ground glow')
            self.add_control(keys, 'glow', 'falloff', 'LaserGroundGlowFalloff', 0.1, 6, 0.1, 2, 'LaserGroundGlowFalloff')
            self.add_control(keys, 'glow', 'wrap', 'LaserGroundGlowWrap', 0, 1, 0.05, 0.5, 'LaserGroundGlowWrap')
            self.add_control(keys, 'glow', 'radius', 'LaserGroundGlowRadius', 0, 300, 1, 0, 'LaserGroundGlowRadius')
            self.add_control(view, 'glow', 'outer', 'OuterBeamWidth (radius 0)', 5, 100, 1, 32)
            self.add_control(view, 'glow', 'strength', 'Light strength', 0, 3, 0.05, 1.5)
            self.add_control(view, 'glow', 'start', 'Beam height at shooter', 0, 60, 1, 6)
            self.add_control(view, 'glow', 'end', 'Beam height at target', 0, 200, 1, 6)
            self.colour_button(view, 'glow', 'Light colour...')
            self.hills = self.check(view, 'Hills', True)
            self.animate.append(self.check(view, 'Animate pulses (Laser tab keys)', True))
            note = QtWidgets.QLabel('Seen from above. The white line is the beam.')
            note.setStyleSheet('color: #909090;')
            view.addWidget(note)

        # ------------------------------------------------ rendering

        def values(self, group):
            return {name: control.value() for (g, name), control in self.controls.items() if g == group}

        def show_preview(self, tab, image):
            image = image.convert('RGBA')
            data = image.tobytes('raw', 'RGBA')
            qimage = QtGui.QImage(data, image.width, image.height, QtGui.QImage.Format_RGBA8888).copy()
            self.previews[tab].setPixmap(QtGui.QPixmap.fromImage(qimage))

        def render(self, tab, now):
            if tab == 0:
                q = self.values('electric')
                jump = int(now * q['rate']) if self.animate[0].isChecked() and q['rate'] > 0 else 0
                mult = q['distance']
                tile = 380
                img = render_electric(q, electric=not self.plain['electric'].isChecked(), size=q['size'],
                                      colour=self.colours['electric'], jump=jump, depth=self.camera_distance * mult,
                                      tile=max(int(tile / mult), 8), ss=1, texture=self.textures['electric'],
                                      camera_distance=self.camera_distance)
                self.show_preview(0, img.resize((tile, tile), Image.NEAREST))
            elif tab == 1:
                q = self.values('laser')
                t = now if self.animate[1].isChecked() else 0.0
                img = render_laser(q, shaded=not self.plain['laser'].isChecked(), width_px=760, beam_units=q['beam'],
                                   t=t, ss=1, colour=self.colours['laser'], texture=self.textures['laser'])
                self.show_preview(1, img)
            else:
                q = self.values('glow')
                laser = self.values('laser')
                reach = q['radius'] if q['radius'] > 0 else max(1.25 * q['outer'], 15.0)
                p = dict(falloff=q['falloff'], wrap=q['wrap'], reach=reach, pulse=laser['pulse'],
                         pulse_size=laser['pulse_size'], speed=laser['speed'])
                rgb = tuple(c * q['strength'] for c in self.colours['glow'])
                t = now if self.animate[2].isChecked() else 0.0
                beam = ((-70, 0, q['start']), (70, 0, q['end']))
                self.show_preview(2, render_glow(p, beam=beam, hills=self.hills.isChecked(), ss=1, glow_rgb=rgb, t=t, px=380))

        def tick(self):
            tab = self.tabs.currentIndex()
            if self.dirty or self.animate[tab].isChecked():
                self.dirty = False
                self.render(tab, time.perf_counter() - self.start)

        # ------------------------------------------------ GameData.ini

        def browse(self):
            path = QtWidgets.QFileDialog.getOpenFileName(self, 'GameData.ini', self.path.text(), 'INI (*.ini);;All (*.*)')[0]
            if path:
                self.path.setText(path)

        def load_values(self):
            try:
                found = read_gamedata(self.path.text())
            except OSError as error:
                QtWidgets.QMessageBox.critical(self, 'GameData.ini', str(error))
                return
            for group, name, key in GAMEDATA_KEYS:
                if key in found:
                    self.controls[(group, name)].setValue(found[key])
            if 'CameraHeight' in found and 'CameraPitch' in found and found['CameraPitch'] > 1:
                self.camera_distance = found['CameraHeight'] / np.sin(np.radians(found['CameraPitch']))
            missing = [k for _, _, k in GAMEDATA_KEYS if k not in found]
            self.statusBar().showMessage('Loaded %d keys%s' % (len(GAMEDATA_KEYS) - len(missing), (', not set: ' + ', '.join(missing)) if missing else ''))
            self.mark_dirty()

        def save_values(self):
            path = self.path.text()
            if not os.path.exists(path):
                QtWidgets.QMessageBox.critical(self, 'GameData.ini', 'Not found: ' + path)
                return
            try:
                if not self.backed_up:
                    shutil.copy2(path, path + time.strftime('.bak-%Y%m%d-%H%M%S'))
                    self.backed_up = True
                write_gamedata(path, {key: self.controls[(group, name)].value() for group, name, key in GAMEDATA_KEYS})
            except (OSError, ValueError) as error:
                QtWidgets.QMessageBox.critical(self, 'GameData.ini', str(error))
                return
            self.statusBar().showMessage('Saved %s at %s. Cheat builds reload it within a second.' % (os.path.basename(path), time.strftime('%H:%M:%S')))

        def copy_lines(self):
            lines = '\n'.join('  %s = %g' % (key, self.controls[(group, name)].value()) for group, name, key in GAMEDATA_KEYS)
            QtWidgets.QApplication.clipboard().setText(lines)
            self.statusBar().showMessage('Copied %d GameData.ini lines' % len(GAMEDATA_KEYS))

    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    app.setStyle('Fusion')
    app.setPalette(dark_palette(QtGui, QtCore))
    app.setStyleSheet(DARK_STYLE)
    window = Tuner()
    window.show()
    return app.exec_()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--docs', action='store_true', help='write the doc pictures into docs/images')
    parser.add_argument('--gamedata', default=DEFAULT_GAMEDATA, help='GameData.ini the tuner loads and saves')
    args = parser.parse_args()
    if args.docs:
        electric_images()
        laser_images()
        glow_images()
    else:
        run_tuner(args.gamedata)
