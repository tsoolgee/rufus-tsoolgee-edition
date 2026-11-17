#!/usr/bin/env python3
"""Rufus tsoolgee - release automation helpers (run from the repository root).

  plan              decide which upstream releases still need a tsoolgee release
                    (writes 'tags', 'builds', 'count', 'newest' to $GITHUB_OUTPUT)
  ver  TAG OUTDIR   write the signed-to-be update file(s) for TAG into OUTDIR
  newer A B         exit 0 if tag A is newer than tag B

Environment: GH_TOKEN (gh CLI), GITHUB_REPOSITORY, FORCE_TAG (optional), UPSTREAM (default pbatard/rufus)
"""
import json
import os
import re
import subprocess
import sys

UPSTREAM = os.environ.get('UPSTREAM', 'pbatard/rufus')
REPO = os.environ.get('GITHUB_REPOSITORY', 'tsoolgee/rufus-tsoolgee-edition')
HERE = os.path.dirname(os.path.abspath(__file__))


def tag_key(tag):
    """v4.15 < v4.16_BETA < v4.16_BETA2 < v4.16 < v4.16.1"""
    m = re.match(r'^v(\d+)\.(\d+)(?:\.(\d+))?(?:_(ALPHA|BETA|RC)(\d*))?$', tag)
    if not m:
        return None
    major, minor, patch, kind, num = m.groups()
    rank = {'ALPHA': 0, 'BETA': 1, 'RC': 2, None: 3}[kind]
    return (int(major), int(minor), int(patch or 0), rank, int(num or 1))


def gh_json(path):
    # one compact JSON object per line, whatever the number of pages
    out = subprocess.run(['gh', 'api', '--paginate', path, '--jq', '.[]'], check=True, capture_output=True,
                         text=True, encoding='utf-8').stdout
    return [json.loads(line) for line in out.splitlines() if line.strip()]


def read(name):
    with open(os.path.join(HERE, name), encoding='utf-8') as f:
        return f.read().strip()


def upstream_releases():
    rel = [r for r in gh_json(f'repos/{UPSTREAM}/releases?per_page=100') if not r['draft']]
    return [r for r in rel if tag_key(r['tag_name']) is not None]


def output(**kw):
    path = os.environ.get('GITHUB_OUTPUT')
    lines = [f'{k}={v}' for k, v in kw.items()]
    if path:
        with open(path, 'a', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


def plan():
    min_key = tag_key('v' + read('MIN_VERSION'))
    force = os.environ.get('FORCE_TAG', '').strip()
    ups = upstream_releases()
    mine = {r['tag_name'] for r in gh_json(f'repos/{REPO}/releases?per_page=100')}
    pending = []
    for r in ups:
        t = r['tag_name']
        if t == force or (tag_key(t) >= min_key and t not in mine):
            pending.append(r)
    pending.sort(key=lambda r: tag_key(r['tag_name']))
    tags = [r['tag_name'] for r in pending]
    builds = []
    for r in pending:
        # Mirror upstream: stable releases ship x64 + x86 + arm64, BETAs only x64
        archs = ['x64'] if r['prerelease'] else ['x64', 'x86', 'arm64']
        builds += [{'tag': r['tag_name'], 'arch': a, 'beta': bool(r['prerelease'])} for a in archs]
    newest = max((r['tag_name'] for r in ups), key=tag_key)
    output(tags=json.dumps(tags), builds=json.dumps({'include': builds}), count=len(tags), newest=newest)


def file_version(rc_path=None):
    rc_path = rc_path or os.environ.get('RC_PATH') or 'src/rufus.rc'
    with open(rc_path, encoding='latin-1') as f:
        m = re.search(r'FILEVERSION\s+(\d+),(\d+),(\d+)', f.read())
    return '.'.join(m.groups())


def rtf_escape(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch in '\\{}':
            out.append('\\' + ch)
        elif o < 128:
            out.append(ch)
        else:
            out.append('\\u%d?' % (o if o < 32768 else o - 65536))
    return ''.join(out)


def make_ver(tag, outdir):
    rel = next((r for r in upstream_releases() if r['tag_name'] == tag), None)
    beta = bool(rel and rel['prerelease'])
    version = file_version()
    short = tag[1:]
    base = f'https://github.com/{REPO}/releases/download/{tag}/rufus-{short}'
    platform_min = '6.2'
    try:
        up = subprocess.run(['curl', '-fsSL', 'https://rufus.ie/Rufus_win.ver'], capture_output=True, text=True, encoding='utf-8', timeout=60).stdout
        m = re.search(r'^platform_min\s*=\s*([\d.]+)', up, re.M)
        if m:
            platform_min = m.group(1)
    except Exception:
        pass
    he = (f'Rufus tsoolgee {short} - \u05de\u05d1\u05d5\u05e1\u05e1 \u05e2\u05dc Rufus {short} \u05d4\u05de\u05e7\u05d5\u05e8\u05d9, '
          '\u05e2\u05dd \u05d0\u05e4\u05e9\u05e8\u05d5\u05ea \u05dc\u05d4\u05e6\u05d9\u05d2 \u05db\u05d5\u05e0\u05e0\u05d9\u05dd \u05e4\u05e0\u05d9\u05de\u05d9\u05d9\u05dd '
          '\u05d5\u05d0\u05d6\u05d4\u05e8\u05d5\u05ea \u05e0\u05d5\u05e1\u05e4\u05d5\u05ea \u05dc\u05e4\u05e0\u05d9 \u05d3\u05d9\u05e1\u05e7 \u05d4\u05de\u05e2\u05e8\u05db\u05ea.')
    en = f'Rufus tsoolgee {short} - an edited version (not the original Rufus) based on Rufus {short}, with optional listing of internal drives and extra warnings for the system disk.'
    he += ' נערך על ידי צול גאה (tsoolgee.uk).'
    en += ' Edited by tsoolgee (tsoolgee.uk).'
    notes = (r'{\rtf1\fbidis\ansi\ansicpg1252\deff0\deflang1024{\fonttbl{\f0\fnil\fcharset0 Arial Unicode MS;}}'
             '\n\\pard\\ltrpar\\sl276\\slmult1\\par\\b\\f0\\fs22 ' + rtf_escape(f'Rufus tsoolgee {short}') + '\\b0\\par\\par\n'
             '\\pard\\rtlpar\\fs18 ' + rtf_escape(he) + '\\par\\par\n'
             '\\pard\\ltrpar\\fs18 ' + rtf_escape(en) + '\\par\\par\n'
             '\\fs16 ' + rtf_escape(f'Upstream changes: https://github.com/{UPSTREAM}/releases/tag/{tag}') + '\\par\n'
             + rtf_escape(f'This edition: https://github.com/{REPO}') + '\\par\n}')
    lines = [f'version = {version}', f'platform_min = {platform_min}', f'download_url = {base}.exe']
    if not beta:
        lines += [f'download_url_x86 = {base}_x86.exe', f'download_url_arm64 = {base}_arm64.exe']
    lines.append('release_notes = ' + notes)
    name = 'Rufus_beta_win.ver' if beta else 'Rufus_win.ver'
    os.makedirs(outdir, exist_ok=True)
    # Rufus' parser expects plain text, no BOM; keep LF line endings
    with open(os.path.join(outdir, name), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(name)


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'plan':
        plan()
    elif cmd == 'ver':
        make_ver(sys.argv[2], sys.argv[3])
    elif cmd == 'newer':
        a, b = tag_key(sys.argv[2]), tag_key(sys.argv[3])
        sys.exit(0 if (a and b and a > b) else 1)
    else:
        print(__doc__)
        sys.exit(2)
