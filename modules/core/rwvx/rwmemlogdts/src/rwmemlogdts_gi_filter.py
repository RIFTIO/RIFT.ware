#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab

import sys

renames = {
    0: {'rwmemlog': 'RwMemlog'},
}

def gobjectify(ident):
    if not ident.startswith('rw_'):
        if not ident.startswith('rw'):
            return ident

    # Remove trailing '_[a-z]' from ident
    if ident[-2] == '_':
        ident = ident[:-2]

    return ''.join(renames.get(depth, {}).get(name, name.title())
                   for depth, name in enumerate(ident.split('_')))


if __name__ == '__main__':
    text = gobjectify(sys.stdin.read().strip())
    sys.stdout.write(text)

