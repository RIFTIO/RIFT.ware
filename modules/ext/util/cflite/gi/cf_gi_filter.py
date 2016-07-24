#!/usr/bin/python
# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab

import sys

def gobjectify(ident):
    if not ident.startswith('CF'):
        return ident

    # Remove trailing '_[a-z]' from ident
    if ident.endswith('Ref'):
        ident = ident[:-3]

    return ident

if __name__ == '__main__':
    text = gobjectify(sys.stdin.read().strip())
    sys.stdout.write(text)

