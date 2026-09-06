'''Loads settings from json file
   !!! Currently defaults to converting str to bytes
   Sets up logging to file.
'''

import os
import json
import sys
from datetime import date

import logging
from logging import info

_default = {
    "waveserverHost": "127.0.0.1",
    "waveserverPort": 16029,

    "plotColor": "black",

    "width": 800,
    "height": 250,
    "dpi": 100
}

settings = _default.copy()


# Setup logging
def setup(program_name):
    global settings
    filename, _ = os.path.splitext(program_name)
    filename = date.today().strftime(filename + "_%Y%m%d.log")
    ew_log = os.getenv('EW_LOG')
    if ew_log and os.path.isdir(ew_log):
        print('Using EW_LOG env var.')
        filename = os.path.join(ew_log, filename)
    level = logging.WARNING
    if '--debug' in sys.argv:
        level = logging.DEBUG
    if '--info' in sys.argv:
        level = logging.INFO
    logging.basicConfig(
        level=level,
        style='{',
        format='{asctime} [{module}:{funcName}] {message}',
        datefmt='%H:%M:%S',
        handlers=[
            logging.FileHandler(filename, mode='w'),
            logging.StreamHandler()
        ])

    if os.path.isfile('settings.json'):
        info('Reading settings.json...')
        with open('settings.json', 'r') as file:
            settings = _default.copy()
            loaded = json.load(file)
            settings.update(loaded)
            # Convert all strings to bytes with encoding ascii
            for k in settings:
                if isinstance(settings[k], str):
                    settings[k] = bytes(settings[k], encoding='ascii')
                elif isinstance(settings[k], list):
                    for setting in settings[k]:
                        if isinstance(setting, str):
                            setting = bytes(setting, encoding='ascii')


def get_default():
    return _default


def get(item):
    return settings[item]


def remap(d):
    return {k: settings[v] for (k, v) in d.items()}
