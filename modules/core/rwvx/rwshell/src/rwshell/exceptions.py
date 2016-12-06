
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

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

