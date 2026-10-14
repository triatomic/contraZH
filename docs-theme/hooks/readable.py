"""Build-time readability fixes for the docs site; the .md files stay GitHub Markdown."""

import hashlib
import os
import re
import xml.etree.ElementTree as etree

from markdown.extensions import Extension
from markdown.treeprocessors import Treeprocessor

FENCE = re.compile(r'^\s*(```|~~~)')
LIST_ITEM = re.compile(r'^(\s*)([*+-]|\d+\.)\s+')
NOTE_LINE = re.compile(r'^(Note|NOTE|Notes?):\s*(.*)$')
HEADING_TAG = re.compile(r'\s*\((New(?: - EXPERIMENTAL| Command)?|EXPERIMENTAL)\)\s*$')
KEY_SEPARATOR = re.compile(r'^\s*[-–]\s+')


def gfm_lists(lines):
    """Give lists the blank line and 4-space nesting Python-Markdown needs; GitHub needs neither."""
    out = []
    in_fence = False
    in_list = False
    prev = ''
    for line in lines:
        if FENCE.match(line):
            in_fence = not in_fence
        if in_fence or FENCE.match(line):
            out.append(line)
            prev = line
            continue
        item = LIST_ITEM.match(line)
        if item:
            if not in_list and prev.strip():
                out.append('')
            indent = len(item.group(1))
            if 0 < indent < 4:
                line = '    ' + line[indent:]
            in_list = True
        elif not line.strip():
            pass
        elif not line.startswith(' ') and not prev.strip():
            in_list = False
        out.append(line)
        prev = line
    return out


def note_admonitions(lines):
    """Turn 'Note:' paragraphs and 'Notes:' lists into note admonitions."""
    out = []
    in_fence = False
    i = 0
    while i < len(lines):
        line = lines[i]
        if FENCE.match(line):
            in_fence = not in_fence
        note = None if in_fence else NOTE_LINE.match(line)
        if not note:
            out.append(line)
            i += 1
            continue
        title = 'Notes' if note.group(1).lower() == 'notes' else 'Note'
        body = [note.group(2)] if note.group(2) else []
        i += 1
        while i < len(lines):
            nxt = lines[i]
            if not nxt.strip():
                ahead = lines[i + 1] if i + 1 < len(lines) else ''
                if body and (LIST_ITEM.match(ahead) or ahead.startswith('    ')) and LIST_ITEM.match(body[0]):
                    body.append('')
                    i += 1
                    continue
                if body:
                    break
                i += 1
                continue
            if FENCE.match(nxt) or nxt.startswith('#') or nxt.startswith('|'):
                break
            body.append(nxt)
            i += 1
        if not body:
            out.append(line)
            continue
        if out and out[-1].strip():
            out.append('')
        out.append('!!! note "%s"' % title)
        out.append('')
        out.extend(('    ' + b) if b.strip() else '' for b in body)
        out.append('')
    return out


def on_page_markdown(markdown, **kwargs):
    lines = markdown.split('\n')
    lines = note_admonitions(gfm_lists(lines))
    return '\n'.join(lines)


class ReadableTreeprocessor(Treeprocessor):
    """Mark key entries and heading tags after the TOC has taken its ids."""

    def run(self, root):
        for li in root.iter('li'):
            self.key_entry(li)
        for level in range(1, 7):
            for h in root.iter('h%d' % level):
                self.heading_tag(h)

    def key_entry(self, li):
        holder = li
        if len(li) and li[0].tag == 'p' and not (li.text or '').strip():
            holder = li[0]
        if (holder.text or '').strip() or not len(holder) or holder[0].tag != 'code':
            return
        code = holder[0]
        rest = code.tail or ''
        inline = []
        for child in list(holder)[1:]:
            if child.tag in ('ul', 'ol', 'p', 'pre', 'div', 'table'):
                break
            inline.append(child)
        separator = KEY_SEPARATOR.match(rest)
        if not separator and (rest.strip() or inline):
            return
        li.set('class', (li.get('class', '') + ' contra-key').strip())
        if not separator:
            return
        rest = rest[separator.end():]
        if rest.startswith('(') and inline:
            tail = (inline[-1].tail or '').rstrip()
            if tail.endswith(')'):
                rest = rest[1:]
                inline[-1].tail = tail[:-1]
        elif rest.startswith('(') and rest.rstrip().endswith(')'):
            rest = rest.rstrip()[1:-1]
        desc = etree.Element('span', {'class': 'contra-desc'})
        desc.text = rest
        code.tail = None
        index = list(holder).index(code) + 1
        for child in inline:
            holder.remove(child)
            desc.append(child)
        holder.insert(index, desc)

    def heading_tag(self, h):
        link = None
        for child in h:
            if child.tag == 'a' and 'headerlink' in child.get('class', ''):
                link = child
        children = [c for c in h if c is not link]
        if children:
            owner, attr = children[-1], 'tail'
        else:
            owner, attr = h, 'text'
        text = getattr(owner, attr) or ''
        match = HEADING_TAG.search(text)
        if not match:
            return
        setattr(owner, attr, text[:match.start()] + ' ')
        badge = etree.Element('span', {'class': 'contra-badge'})
        badge.text = match.group(1).replace('New - EXPERIMENTAL', 'New · Experimental').replace('EXPERIMENTAL', 'Experimental')
        if 'EXPERIMENTAL' in match.group(1):
            badge.set('class', 'contra-badge contra-badge--exp')
        position = list(h).index(link) if link is not None else len(h)
        h.insert(position, badge)


class ReadableExtension(Extension):
    def extendMarkdown(self, md):
        md.treeprocessors.register(ReadableTreeprocessor(md), 'contra_readable', 4)


def asset_hash(config, path):
    for folder in config.theme.dirs:
        full = os.path.join(folder, path)
        if os.path.isfile(full):
            with open(full, 'rb') as f:
                return hashlib.sha1(f.read()).hexdigest()[:10]
    return None


def on_config(config, **kwargs):
    config.markdown_extensions.append(ReadableExtension())

    # Pages serves CSS and JS with a 10-minute cache, so stamp each link with its content hash
    config.extra_css = ['%s?h=%s' % (p, asset_hash(config, p)) if asset_hash(config, p) else p for p in config.extra_css]
    for i, script in enumerate(config.extra_javascript):
        path = script if isinstance(script, str) else script.path
        digest = asset_hash(config, path)
        if digest and isinstance(script, str):
            config.extra_javascript[i] = '%s?h=%s' % (path, digest)
        elif digest:
            script.path = '%s?h=%s' % (path, digest)
    return config
