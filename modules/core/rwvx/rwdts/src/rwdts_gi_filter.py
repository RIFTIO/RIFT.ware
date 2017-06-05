#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab

import sys

# rwsched_instance_ptr_t -> RwschedInstance
renames = {
    0: {'rwdts': 'RwDts'},
}

def gobjectify(ident):
    if not ident.startswith('rwdts_'):
        if not ident.startswith('rwdts'):
            return ident

    # Remove trailing '_[a-z]' from ident
    if ident.endswith('ptr_t'):
        ident = ident[:-5]
    if ident.endswith('_t'):
        ident = ident[:-2]
    elif ident.endswith('Ref'):
        ident = ident[:-3]


    s =  ''.join(renames.get(depth, {}).get(name, name.title())
                   for depth, name in enumerate(ident.split('_')))
    return s


if __name__ == '__main__':
    text = gobjectify(sys.stdin.read().strip())
    sys.stdout.write(text)

