import asyncio

import gi

gi.require_version('RwKeyspec', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwDts', '1.0')

from gi.repository import (
    RwKeyspec,
    RwDts,
    RwYang,
)

from rift.rwpb.memberdata import (
    MemberData
)

from .dts import (
    DTS,
    Group,
)

from .tasklet import (
    Tasklet,
)

class PathAnnotation(object):
    '''
    Base class for the annotations on the on_prepare callbacks.
    '''
    def __init__(self, subscription_point):
        self.subscription_point = subscription_point
        
    def __call__(self, function):
        def wrapped_function(*args):
            return function(*args)

        setattr(wrapped_function, '_keys', self.subscription_point._keys)
        setattr(wrapped_function, '_path', self.subscription_point._path)
        setattr(wrapped_function, '_schema_descriptor', self.subscription_point._current_descriptor)
        return wrapped_function

def pre_commit(function):
    '''
    Annotation for pre_commit functions.
    '''

    def wrapped_function(*args):
        return function(*args)

    setattr(wrapped_function, '_group_wide', True)
    setattr(wrapped_function, '_pre_commit', True)
    return wrapped_function

def abort(function):
    '''
    Annotation for abort functions.
    '''

    def wrapped_function(*args):
        return function(*args)

    setattr(wrapped_function, '_group_wide', True)
    setattr(wrapped_function, '_abort', True)
    return wrapped_function

def commit(function):
    '''
    Annotation for commit functions.
    '''

    def wrapped_function(*args):
        return function(*args)

    setattr(wrapped_function, '_group_wide', True)
    setattr(wrapped_function, '_commit', True)
    return wrapped_function

def cached_member_data(function):
    '''
    Annotation for the function that returns the list of cached member data.
    '''

    def wrapped_function(*args):
        return function(*args)

    setattr(wrapped_function, '_cached_member_data', True)
    return wrapped_function

class subscribe(PathAnnotation):
    '''
    Annotation for a subscription pre_commit.
    '''

    def __init__(self, subscription_point):
        super(__class__, self).__init__(subscription_point)

    def __call__(self, function):
        wrapped_function = super(__class__, self).__call__(function)

        setattr(wrapped_function, '_group_wide', True)
        setattr(wrapped_function, '_subscribe', True)
        return wrapped_function

class publish(PathAnnotation):
    '''
    Annotation for a publication pre_commit.
    '''

    def __init__(self, subscription_point):
        super(__class__, self).__init__(subscription_point)

    def __call__(self, function):
        wrapped_function = super(__class__, self).__call__(function)

        setattr(wrapped_function, '_group_wide', True)
        setattr(wrapped_function, '_publish', True)
        return wrapped_function

class Registry(Tasklet):
    '''
    Base class for DTS tasklets wanting to use the annotation scheme and member data wrappers.
    '''

    def __init__(self, *args, **kwargs):
        self.schema = kwargs["schema"]
        del kwargs["schema"]
        super(Registry, self).__init__(*args, **kwargs)

        self.member_data_paths = list()
        self.cached_publications = list()
        self.keys = dict()
        self.registrations = dict()

        self.member_data = MemberData(self.schema, self.registrations, self.member_data_paths)

    @property
    def data(self):
        '''
        Returns the reference to the member data wrapper
        '''
        self.member_data.reset()
        return self.member_data

    def register(self):
        '''
        This method registers the anotated callbacks with DTS, it must be called during the init phase.
        '''

        subscriptions = list()
        publications = list()
        group_pre_commit = None
        group_commit = None
        group_abort = None

        self.callbacks = dict()

        for attribute in dir(self):

            handle = getattr(self, attribute)

            if hasattr(handle, "_group_wide"):
                if hasattr(handle, "_publish"):
                    publications.append(handle)
                    self.callbacks[handle._path] = handle
                elif hasattr(handle, "_subscribe"):
                    subscriptions.append(handle)
                    self.callbacks[handle._path] = handle
                elif hasattr(handle, "_pre_commit"):
                    group_pre_commit = handle
                elif hasattr(handle, "_abort"):
                    group_abort = handle
                elif hasattr(handle, "_commit"):
                    group_commit = handle
                else:
                    assert False, "expected annotation on %s" % attribute
            elif hasattr(handle, "_cached_member_data"):
                self.cached_publications += handle()

        handlers = Group.Handler()
        
        with self._dts.group_create(handler=handlers) as group:

            # subscriptions
            for sub in subscriptions:
                handler = DTS.RegistrationHandler(
                    on_prepare=sub,
                    on_precommit=group_pre_commit,
                    on_abort=group_abort,
                    on_commit=group_commit)
                reg_handle = group.register(
                    xpath=sub._path,
                    flags=RwDts.Flag.SUBSCRIBER,
                    handler=handler)
                self.registrations[sub._path] = reg_handle

            # non-member data
            for pub in publications:
                handler = DTS.RegistrationHandler(
                    on_prepare=pub,
                    on_precommit=group_pre_commit,
                    on_abort=group_abort,
                    on_commit=group_commit)
                reg_handle = group.register(
                    xpath=sub._path,
                    flags=RwDts.Flag.PUBLISHER|RwDts.Flag.CACHE,
                    handler=handler)
                self.registrations[pub._path] = reg_handle

            # member data
            for reg_key in self.cached_publications:
                handler = DTS.RegistrationHandler()
                reg_handle = group.register(
                    xpath=reg_key._path,
                    flags=RwDts.Flag.PUBLISHER|RwDts.Flag.CACHE|RwDts.Flag.NO_PREP_READ,
                    handler=handler)
                self.registrations[reg_key._path] = reg_handle
                self.member_data_paths.append(reg_key._path)
