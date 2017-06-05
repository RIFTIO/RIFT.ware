
# STANDARD_RIFT_IO_COPYRIGHT

#
# Rift Exceptions:
#   These exceptions each coorespond with a rift status as they are defined
# in rwtypes.vala.  Adding them here so that errors from C transistioning
# back to python can be handled in a pythonic manner rather than having to
# inspect return values.

class RWErrorFailure(Exception):
  pass

class RWErrorDuplicate(Exception):
  pass

class RWErrorNotFound(Exception):
  pass

class RWErrorOutOfBounds(Exception):
  pass

class RWErrorBackpressure(Exception):
  pass

class RWErrorTimeout(Exception):
  pass

class RWErrorExists(Exception):
  pass

class RWErrorNotEmpty(Exception):
  pass

class RWErrorNotConnected(Exception):
  pass

