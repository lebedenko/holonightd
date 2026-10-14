#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Inspect and smoke-test staged executables without touching a live session."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

prefix = Path(sys.argv[1])
for name in ('holonight-healthd', 'holonight-agentd', 'holonight-agent-event', 'hn-agent-run'):
    binary = prefix / 'bin' / name
    assert binary.is_file(), binary
    resolved = subprocess.check_output(['ldd', str(binary)], text=True)
    assert 'not found' not in resolved, resolved
    assert '/work/source' not in resolved, resolved
assert (prefix / 'etc/holonight/holonightd.conf.example').is_file()
for name in ('holonightd.service', 'holonight-agentd.service', 'holonight-healthd.service', 'holonight-services.target'):
    assert (prefix / 'lib/systemd/system' / name).is_file(), name
with tempfile.TemporaryDirectory() as temporary:
    env = dict(os.environ, HOME=temporary, XDG_STATE_HOME=temporary, XDG_CONFIG_HOME=temporary,
               XDG_RUNTIME_DIR=temporary, DBUS_SESSION_BUS_ADDRESS='unix:path=/nonexistent')
    subprocess.run([prefix / 'bin/holonight-healthd', '--help'], cwd=temporary, env=env, check=True)
    subprocess.run([prefix / 'bin/holonight-agent-event', '--help'], cwd=temporary, env=env, check=True)
    fixture = Path(temporary) / 'fixture.toml'
    fixture.write_text('[general]\ninterval_seconds = 1\nscan_root = "."\ncommands = ["true"]\n')
    subprocess.run([prefix / 'bin/holonight-healthd', '--once', '--config', fixture],
                   cwd=temporary, env=env, check=True, timeout=30)
    # hn-agent-run emits locally with systemd support; invalid arguments exercise parsing only.
    for name in ('holonight-agentd', 'hn-agent-run'):
        result = subprocess.run([prefix / 'bin' / name], cwd=temporary, env=env, capture_output=True, timeout=10)
        assert result.returncode != 0, name
print('Staged payload, dependency resolution and isolated CLI smoke checks passed')
