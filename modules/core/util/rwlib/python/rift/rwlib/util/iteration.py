# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 11/9/15
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

def iterate_with_lookahead(iterable):
    '''Provides a way of knowing if you are the first or last iteration.'''
    iterator = iter(iterable)
    last = next(iterator)
    for val in iterator:
        yield last, False
        last = val
    yield last, True
