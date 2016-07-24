# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 11/9/15
# 

def iterate_with_lookahead(iterable):
    '''Provides a way of knowing if you are the first or last iteration.'''
    iterator = iter(iterable)
    last = next(iterator)
    for val in iterator:
        yield last, False
        last = val
    yield last, True
