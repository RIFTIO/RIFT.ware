
# STANDARD_RIFT_IO_COPYRIGHT

"""
The classes in this module are mixins that are used to build tasklets with DTS
capabilities. Mixins are similar to base classes in a multiple inheritance
design. The main difference is that they are not intended to be classes in
their own right, but represent optional functionality that can be used to
compose a particular type of tasklet.
"""

import weakref

import gi
gi.require_version('RwDts', '1.0')

from gi.repository import RwDts


class DtsRegister(object):
    """
    This class is an essential component of a tasklet that intends to
    communicate via DTS. This mixin is used to register the tasklet with DTS.
    """

    def register_with_dts(self, schema):
        """
        Registers the tasklet with DTS and sets the handle to DTS. However,
        this does not mean that DTS is aware of the tasklet. DTS acknowledges
        the tasklet by calling the on_register_with_dts function.

        Arguments:
            schema - the schema that defines the xpaths

        """
        self.dts = RwDts.Api.new(
                self.tasklet_info,
                schema,
                self.on_register_with_dts,
                self,
                )

    def on_register_with_dts(self, dts, state, user_data):
        """Called by DTS to acknowledge the registration of the tasklet

        Arguments:
            dts       - a handle to DTS. This should be identical to the handle
                        originally returned in the register_with_dts function.
            state     - the DTS state
            user_data - any user data that was passed to DTS when the tasklet
                        was registered. Currently this is just a reference to
                        this object.

        Raises:
            This function must be implemented by a class that uses this mixin.
            If it is not implemented, a NotImplementedError is raised.

        """
        msg = '{} has not implemented the on_register_with_dts() method'
        raise NotImplementedError(msg.format(self))


class DtsMemberApi(object):
    """
    This mixin enables a tasklet to register as a publisher or subscriber to an
    xpath.
    """

    def __init__(self, *args, **kwargs):
        super(DtsMemberApi, self).__init__(*args, **kwargs)
        self.xpaths = list()

    def register_xpath(self,
        xpath,
        xpath_register_ready,
        xpath_prepare,
        flags=None):
        """Registers an xpath with DTS

        Arguments:
            xpath                - the xpath to register
            xpath_register_ready - a 'register ready' callback
            xpath_prepare        - a 'prepare' callback
            flags                - registration flags

        Returns:
            the status returned by DTS

        """
        status, regobj = self.dts.member_register_xpath(
                xpath,
                None,
                flags,
                RwDts.ShardFlavor.NULL,
                0,
                0,
                -1,
                0,
                0,
                xpath_register_ready,
                xpath_prepare,
                None,
                None,
                None,
                self)

        self.xpaths.append(regobj)

        return status


class DtsAppConf(object):
    """
    This class provides a convenient way to register appconf groups. It also
    provides the necessary caching of the appconf groups and xpaths that are
    not available in an ordinary tasklet.
    """

    def __init__(self, *args, **kwargs):
        super(DtsAppConf, self).__init__(*args, **kwargs)
        self.appconf_groups = list()
        self.appconf_xpaths = list()

    def create_appconf_group(self):
        """Returns an AppConfGroupContextManager instance

        This function returns a context manager that can be used to create an
        appconf group and registers xpaths with it. It is intended to be used
        with the 'with' keyword to ensure that the registration is completed
        properly. For example,

            with self.create_appconf_group() as group:
                group.config_xact_init = foo.on_config_xact_init
                ...
                group.register_xpath(...)

        Upon exiting from the context manager, the appconf group will be
        created and the xpaths registered.

        """
        return AppConfGroupContextManager(self)


class AppConfGroupContext(object):
    """
    This class is the context that is provided by the
    AppConfGroupContextManager. It provides a convenient interface for setting
    the callbacks and xpaths associated with a particular appconf group. The
    data stored on this object is used by the context manager that created it
    to create the actual appconf group and registrations.
    """

    def __init__(self):
        """Creates the context"""
        self.xpaths = list()
        self.config_xact_init = None
        self.config_xact_deinit = None
        self.config_validate = None
        self.config_apply = None

    def register_xpath(self, xpath, callback, flags):
        """Registers an xpath callback

        Arguments:
            xpath    - a string representing an xpath
            callback - a function object associated with the xpath
            flags    - registration flags

        """
        self.xpaths.append((xpath, callback, flags))


class AppConfGroupContextManager(object):
    """
    This class is a context manager that provides a convenient way to create an
    appconf group for a set of xpaths.
    """

    def __init__(self, tasklet):
        """Creates an AppConfGroupContextManager

        Arguments:
            tasklet - the tasklet that the appconf group will be associated
                      with

        """
        self.tasklet = tasklet
        self.group = None

    def __enter__(self):
        """Returns an AppConfGroupContext object

        The context object return here is used to stage the content that will
        be used to created an appconf group. When the context manager is
        successfully exited, the information that has been given to the context
        given is used to create the appconf group and associated xpaths.

        Returns:
            a context object

        """
        self.group = AppConfGroupContext()
        return self.group

    def __exit__(self, exc_type, exc_value, traceback):
        """Creates the appconf group

        When the context manager is exited, the information that has been added
        to the context object that was provided is used to create an appconf
        group with associated xpaths.

        Arguments:
            exc_type  - the type of exception raised
            exc_value - the instance of the exception
            traceback - the associated traceback

        Returns:
            A boolean indicating whether exceptions should be suppressed

        """
        # If an exception has been raised while the context object is being
        # populated, we do not want to try to create the appconf group.
        if all(arg is None for arg in (exc_type, exc_value, traceback)):
            # First create the appconf group
            status, group = self.tasklet.dts.appconf_group_create(
                    self.group.config_xact_init,
                    self.group.config_xact_deinit,
                    self.group.config_validate,
                    self.group.config_apply,
                    )

            # Add the group to the list of groups associated with this tasklet
            self.tasklet.appconf_groups.append(group)

            # Register all of the xpaths
            for xpath, callback, flags in self.group.xpaths:
                status, cfgreg = group.register_xpath(xpath, flags, callback)
                self.tasklet.appconf_xpaths.append(cfgreg)

            # Tell DTS that we have finished creating this group
            group.phase_complete(RwDts.AppconfPhase.REGISTER)

        # Returning false indicates that we do not wish to suppress any
        # exceptions that were raised.
        return False
