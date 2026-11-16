#!/usr/bin/env python3
"""Compile supported repository examples, excluding private smoke/test firmware."""
import configparser
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
config = configparser.ConfigParser(interpolation=None)
config.read(root / 'platformio.ini')
environments = [section[4:] for section in config.sections()
                if section.startswith('env:') and
                ('../examples/' in config[section].get('build_src_filter', ''))]
# Recovery environments inherit their profiles but keep their own source filters.
subprocess.run(['pio', 'run', *[item for env in environments for item in ['-e', env]]], cwd=root, check=True)
for folder in ['moisture-controller', 'moisture-controller-esp32']:
    subprocess.run(['pio', 'run', '--project-dir', str(root / 'examples' / folder)], check=True)
