#!/usr/bin/env python

from os.path import abspath, dirname, exists, join
import argparse
import json
import os
import re
import subprocess
import sys

DATABASE = 'update_v8.json'

options = None
dry_run = False

try:
  unicode
except NameError:
  unicode = str  # Fix-up for python 3.

PY3 = (unicode == str)


def git(*args, **kwargs):
  cmd = [options.git] + list(args)
  print(' '.join(cmd))

  try:
    if kwargs.pop('dry_run'): return
  except KeyError:
    pass

  if kwargs.get('check_output'):
    del kwargs['check_output']
    if PY3: kwargs['encoding'] = 'utf-8'
    return subprocess.check_output(cmd, **kwargs).strip()

  subprocess.check_call(cmd, **kwargs)


def isv8(dep):
  return '' == dep['path']


def repodir(dep):
  return abspath(join(options.tmpdir, 'v8', dep['path'].replace('/', '_')))


def repodir_exists(dep):
  return exists(join(repodir(dep), 'config'))


def update_one(dep):
  cwd = abspath('.')

  url = dep['url']
  path = dep['path']
  branch = dep['branch']
  commit = dep['commit']
  clonedir = repodir(dep)

  if not repodir_exists(dep):
    git('clone', '--bare', '--config', 'core.autocrlf=false', url, clonedir,
        cwd=options.tmpdir, dry_run=dry_run)

  what = commit
  if isv8(dep):
    #what = '+refs/{}:refs/{}'.format(branch, branch)
    what = '+refs/branch-heads/*:refs/branch-heads/*'

  git('fetch', url, what, cwd=clonedir, dry_run=dry_run)


def update_all():
  with open(DATABASE) as fp:
    deps = json.load(fp)

  assert len(deps) > 0
  assert isinstance(deps, list)

  for dep in deps:
    assert isinstance(dep, dict)
    assert isinstance(dep.get('branch'), unicode)
    assert isinstance(dep.get('commit'), unicode)
    assert isinstance(dep.get('path'), unicode)
    assert isinstance(dep.get('url'), unicode)

  v8 = deps[0] # must be first
  assert isv8(v8)

  update_one(v8)
  v8['commit'] = (
      git('rev-parse', v8['branch'], check_output=True, cwd=repodir(v8)))

  # Now for some arbitrary code execution...
  what = '{}:DEPS'.format(v8['commit'])
  source = git('show', what, check_output=True, cwd=repodir(v8))
  code = compile('def Var(k): return vars[k]\ndef Str(k): return str(k)\n' + source, 'DEPS', 'exec')
  globls = {}
  eval(code, globls)
  v8_deps = globls['deps']
  assert isinstance(v8_deps, dict)

  for dep in deps[1:]: # skip v8 itself
    changed = options.force or not repodir_exists(dep)

    path = dep['path']
    url_and_commit = v8_deps.get(path)
    if not url_and_commit:
      raise Exception('{} missing from DEPS'.format(path))
    if isinstance(url_and_commit, dict):
      url_and_commit = url_and_commit.get('url')
    if not isinstance(url_and_commit, str):
      raise Exception('{} is not a string or dict in DEPS'.format(path))
    url, commit = url_and_commit.split('@', 2)

    if url != dep['url']:
      print('url changed: {} -> {}'.format(dep['url'], url))
      dep['url'] = url
      changed = True

    if commit != dep['commit']:
      print('commit changed: {} -> {}'.format(dep['commit'], commit))
      dep['commit'] = commit
      changed = True

    if changed:
      update_one(dep)

  arg = '-n' if dry_run else '-q'
  try:
    git('rm', arg, '-r', 'v8')
  except Exception as e:
    print('Failed to remove v8: {}'.format(e)) # Not critical, continue.  Maybe a previous run removed it but then failed and we are re-running now.

  for dep in deps:
    # debug: print dep contents
    prefix = 'v8/{}'.format(dep['path'])
    if len(dep['path']):
      prefix += '/'

    archive_cmd = [
        options.git, 'archive', '--format=tar',
        '--prefix={}'.format(prefix), dep['commit']
    ]
    tar_cmd = [options.tar, 'x']
    print('{} | {}'.format(archive_cmd, tar_cmd))
    if not dry_run:
      archive = subprocess.Popen(
          archive_cmd, cwd=repodir(dep), stdout=subprocess.PIPE)
      try:
        subprocess.check_call(tar_cmd, stdin=archive.stdout)
      finally:
        archive.stdout.close()
        archive_status = archive.wait()
      if archive_status:
        raise subprocess.CalledProcessError(archive_status, archive_cmd)

  for filename in sorted(os.listdir('patches')):
    if filename.endswith('.patch'):
      git('apply', '--reject', join('patches', filename), dry_run=dry_run)

  for path, _, files in os.walk('v8'):
    for filename in files:
      if filename.endswith('.pyc'):
        os.remove(join(path, filename))

  git('add', '-f', 'v8', dry_run=dry_run)
  git('log', '-1', '--oneline', v8['commit'], cwd=repodir(v8))

  newdeps = json.dumps(deps, indent=2)
  newdeps = re.sub(r'\s+$', '\n', newdeps)
  if dry_run:
    print(newdeps)
  else:
    with open(DATABASE, 'w') as fp:
      fp.write(newdeps)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Sync with upstream V8')
  parser.add_argument('--dry-run', default=False, action='store_true')
  parser.add_argument('--force', default=False, action='store_true')
  parser.add_argument('--git', default='git')
  parser.add_argument('--tar', default='tar')
  parser.add_argument('--tmpdir', default=os.environ.get('TMPDIR', '/tmp'))
  parser.add_argument('--workspace', default=abspath(dirname(__file__)))
  options = parser.parse_args()
  dry_run = options.dry_run

  os.chdir(options.workspace)

  lockfile_name = '{}.lock'.format(DATABASE)
  try:
    lockfile = open(lockfile_name, 'x') # python 3
  except ValueError:
    lockfile = open(lockfile_name, 'wx') # python 2

  try:
    update_all()
  finally:
    lockfile.close()
    try:
      os.unlink(lockfile_name)
    except OSError:
      pass
