"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file monkey.py
@author Austin Cormier (austin.cormier@riftio.com)
@date 10/16/2015
@description Monkey patch all asyncio functions to ensure that the loop is always provided

When using asyncio within a tasklet, any usage of asyncio without providing a
CFRunLoop compatible event loop (StackedEventLoop) will almost certainly cause
very random and difficult to debug hangs.

Patch all asyncio methods that take a loop so that any usage of asyncio
without a loop from within a process that uses the StackedEventLoop
will throw an exception.
"""

import asyncio
import functools


class LoopNotProvidedError(Exception):
    pass


_patched = False

# maps module name -> attribute name -> original item
# e.g. "time" -> "sleep" -> built-in function sleep
saved = {}

_asyncio_patch_functions = [
    "as_completed",
    "async",
    "ensure_future",
    "gather",
    "run_coroutine_threadsafe"
    "shield",
    "sleep",
    "wait",
    "wait_for",
]

_asyncio_patch_classes = [
    "Future",
    "Lock",
    "Event",
    "Condition",
    "Semaphore",
]


def _patch_item(module, attr, newitem):
    NONE = object()
    olditem = getattr(module, attr, NONE)
    if olditem is not NONE:
        saved.setdefault(module.__name__, {}).setdefault(attr, olditem)

    setattr(module, attr, newitem)


def _force_loop_param_wrapper(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        if "loop" not in kwargs:
            raise LoopNotProvidedError(
                "loop keyword parameter must be provided in %s call" % f.__name__
            )

        return f(*args, **kwargs)

    @functools.wraps(f)
    @asyncio.coroutine
    def coroutine_wrapper(*args, **kwargs):
        if "loop" not in kwargs:
            raise LoopNotProvidedError(
                "loop keyword parameter must be provided in asyncio.%s call" % f.__name__
            )

        return (yield from f(*args, **kwargs))

    if asyncio.iscoroutinefunction(f):
        return coroutine_wrapper
    else:
        return wrapper


def is_module_patched():
    return _patched


def patch():
    """ Patch all asyncio functions to force loop kwarg to be passed in"""
    if is_module_patched():
        return

    for fn_name in _asyncio_patch_functions:
        if hasattr(asyncio, fn_name):
            orig_fn = getattr(asyncio, fn_name)
            wrap_fn = _force_loop_param_wrapper(orig_fn)

            _patch_item(asyncio, fn_name, wrap_fn)

    for class_name in _asyncio_patch_classes:
        if hasattr(asyncio, class_name):
            cls = getattr(asyncio, class_name)
            orig_fn = getattr(cls, "__init__")
            wrap_fn = _force_loop_param_wrapper(orig_fn)

            _patch_item(cls, "__init__", wrap_fn)

    global _patched
    _patched = True
