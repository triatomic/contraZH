"""Build-time readability fixes for the docs site; the .md files stay GitHub Markdown."""

import os
import re

from docutils import nodes
from pygments.formatters import HtmlFormatter
from pygments.styles import get_style_by_name
from pygments.token import Token
from sphinx.transforms import SphinxTransform

FENCE = re.compile(r'^\s*(```|~~~)')
LIST_ITEM = re.compile(r'^(\s*)([*+-]|\d+\.)\s+')
NOTE_LINE = re.compile(r'^(Note|NOTE|Notes?):\s*(.*)$')
UPSTREAM_WIKI = re.compile(r'https://github\.com/Andreas-W/GeneralsGameCode_Modding/wiki/([^)#\s]+)(#[^)\s]*)?')

# The upstream pages stay verbatim, so their contraZH corrections are linked in here
CORRECTIONS = {
    'FXList-&-ParticleSystems': 'Undocumented-Upstream-Features.md#fxlist-decal-scale',
    'ChatCommands': 'Undocumented-Upstream-Features.md#chatcommands',
}


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
        out.extend(['```{admonition} %s' % title, ':class: note', ''] + body + ['```', ''])
    return out


def local_wiki_links(markdown, srcdir):
    """Point upstream wiki links at this site's copy of the page."""
    def local(match):
        if os.path.isfile(os.path.join(srcdir, match.group(1) + '.md')):
            return match.group(1) + '.md' + (match.group(2) or '')
        return match.group(0)
    return UPSTREAM_WIKI.sub(local, markdown)


def correction_note(lines, docname):
    target = CORRECTIONS.get(docname)
    if target is None:
        return lines
    for i, line in enumerate(lines):
        if line.startswith('# '):
            note = 'Note: contraZH corrects part of this page, see [Corrections to upstream pages](%s).' % target
            return lines[:i + 1] + ['', note] + lines[i + 1:]
    return lines


def nav_title(lines, docname, nav):
    """Give pages without an h1 their nav title, so they get a sidebar link and a heading."""
    in_fence = False
    for line in lines:
        if FENCE.match(line):
            in_fence = not in_fence
        elif not in_fence and line.startswith('# '):
            return lines
    for caption, pages in nav:
        for title, doc in pages:
            if doc == docname:
                return ['# ' + title, ''] + lines
    return lines


def nav_toctrees(nav):
    """Hidden toctrees that build the sidebar from the contra_nav config."""
    out = []
    for caption, pages in nav:
        out += ['', '```{toctree}', ':hidden:', ':caption: ' + caption, '']
        out += ['%s <%s>' % (title, doc) for title, doc in pages]
        out += ['```']
    return out


def on_source_read(app, docname, source):
    markdown = local_wiki_links(source[0], app.srcdir)
    lines = nav_title(markdown.split('\n'), docname, app.config.contra_nav)
    lines = note_admonitions(correction_note(lines, docname))
    if docname == app.config.root_doc:
        lines += nav_toctrees(app.config.contra_nav)
    source[0] = '\n'.join(lines)


class NestLaterTitles(SphinxTransform):
    """Pages with several h1s keep the first as the title and the later h1s as parts inside it."""

    default_priority = 500

    def apply(self, **kwargs):
        sections = [n for n in self.document.children if isinstance(n, nodes.section)]
        for section in sections[1:]:
            self.document.remove(section)
            sections[0].append(section)


def scheme_pygments(app):
    """Code colours for the Slate scheme; Sphinx writes only pygments_style's sheet."""
    if app.builder.format != 'html':
        return
    style = app.config.contra_slate_pygments
    prefix = ':root[data-scheme="slate"] .highlight'
    text = get_style_by_name(style).style_for_token(Token)['color'] or 'e6edf3'
    rules = ['%s { color: #%s }' % (prefix, text),
             '%s span { color: inherit; font-weight: inherit; font-style: inherit; background-color: transparent; border: none }' % prefix]
    formatter = HtmlFormatter(style=style, nobackground=True)
    rules += formatter.get_background_style_defs(prefix) + formatter.get_token_style_defs(prefix)
    static = os.path.join(app.outdir, '_static')
    os.makedirs(static, exist_ok=True)
    with open(os.path.join(static, 'pygments-slate.css'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(rules) + '\n')
    app.add_css_file('pygments-slate.css')


def setup(app):
    app.add_config_value('contra_nav', [], 'env')
    app.add_config_value('contra_slate_pygments', 'github-dark', 'html')
    app.connect('builder-inited', scheme_pygments)
    app.connect('source-read', on_source_read)
    app.add_transform(NestLaterTitles)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
