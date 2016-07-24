import collections
import re


class Attribute(collections.namedtuple("Attribute", "module name")):
    def __repr__(self):
        return "{}:{}".format(self.module, self.name)


class ListElement(collections.namedtuple("List", "module name key value")):
    def __repr__(self):
        return "{}:{}[{}={}]".format(self.module, self.name, self.key, self.value)


def tokenize(xpath):
    """Return a list of tokens representing an xpath

    The types of xpaths that this selector supports is extremely limited.
    The xpath is required to be an absolute path delimited by a
    forward-slash. Each of the parts (elements between delimiters) is
    treated as one of two possible types:

        - an attribute
        - a list element

    An attribute is a normal python attribute on an object. A list element
    is an element within a list, which is identified by a key value (like a
    yang list, although this is more properly a dict in python).

    Each attribute is expected to have the form,

        <namespace>:<variable-name>

    A valid variable name (or namespace) follows the python regular expression,

        [a-zA-Z0-9-_]+

    A list entry has the form,

        <namespace>:<variable-name>[<namespace>:<variable-name>=<value>]

    The expression in the square brackets is the key of the required
    element, and the value that that key must have.

    Arguments:
        xpath - a string containing an xpath expression

    Raises:
        A ValueError is raised if the xpath cannot be parsed.

    Returns:
        a list of tokens

    """
    # define the symbols that are valid for a variable name in yang
    name = "[a-zA-Z0-9-_]+"

    # define a set of regular expressions for parsing the xpath
    pattern_attribute = re.compile("({t}):({t})$".format(t=name))
    pattern_key_value = re.compile("^{t}:({t})\s*=\s*(.*)$".format(t=name))
    pattern_quote = re.compile("^[\'\"](.*)[\'\"]$")
    pattern_list = re.compile("^(.*)\[(.*)\]$")

    def dash_to_underscore(text):
        return text.replace('-', '_')

    # Iterate through the parts of the xpath (NB: because the xpaths are
    # required to be absolute paths, the first character is going to be the
    # forward slash. As a result, when the string is split, the first
    # element with be an empty string).
    tokens = list()
    for part in xpath.split("/")[1:]:

        # Test the part to see if it is a attribute
        result = pattern_attribute.match(part)
        if result is not None:
            module, name = result.groups()

            # Convert the dashes to underscores
            name = dash_to_underscore(name)
            module = dash_to_underscore(module)

            tokens.append(Attribute(module, name))

            continue

        # Test the part to see if it is a list
        result = pattern_list.match(part)
        if result is not None:
            attribute, keyvalue = result.groups()

            module, name = pattern_attribute.match(attribute).groups()
            key, value = pattern_key_value.match(keyvalue).groups()

            # Convert the dashes to underscore (but not in the key value)
            key = dash_to_underscore(key)
            name = dash_to_underscore(name)
            module = dash_to_underscore(module)

            result = pattern_quote.match(value)
            if result is not None:
                value = result.group(1)

            tokens.append(ListElement(module, name, key, value))

            continue

        raise ValueError("cannot parse '{}'".format(part))

    return tokens


class XPathAttribute(object):
    """
    This class is used to represent a reference to an attribute. If you use
    getattr on an attribute, it may give you the value of the attribute rather
    than a reference to it. What is really wanted is a representation of the
    attribute so that its value can be both retrieved and set. That is what
    this class provides.
    """

    def __init__(self, obj, name):
        """Create an instance of XPathAttribute

        Arguments:
            obj  - the object containing the attribute
            name - the name of an attribute

        Raises:
            A ValueError is raised if the provided object does not have the
            associated attribute.

        """
        if not hasattr(obj, name):
            msg = "The provided object does not contain the associated attribute"
            raise ValueError(msg)

        self.obj = obj
        self.name = name

    def __repr__(self):
        return self.value

    @property
    def value(self):
        return getattr(self.obj, self.name)

    @value.setter
    def value(self, value):
        """Set the value of the attribute

        Arguments:
            value - the new value that the attribute should take

        Raises:
            An TypeError is raised if the provided value cannot be cast the
            current type of the attribute.

        """
        attr_type = type(self.value)
        attr_value = value

        # The only way we can currently get the type of the atrribute is if it
        # has an existing value. So if the attribute has an existing value,
        # cast the value to the type of the attribute value.
        if attr_type is not type(None):
            try:
                attr_value = attr_type(attr_value)

            except ValueError:
                msg = "expected type '{}', but got '{}' instead"
                raise TypeError(msg.format(attr_type.__name__, type(value).__name__))

        setattr(self.obj, self.name, attr_value)


class XPathElement(XPathAttribute):
    """
    This class is used to represent a reference to an element within a list.
    Unlike scalar attributes, it is not entirely necessary to have this class
    to represent the attribute because the element cannot be a simple scalar.
    However, this class is used because it creates a uniform interface that can
    be used by the setxattr and getxattr functions.
    """

    def __init__(self, container, key, value):
        """Create an instance of XPathElement

        Arguments:
            container - the object that contains the element
            key       - the name of the field that is used to identify the
                        element
            value     - the value of the key that identifies the element

        """
        self._container = container
        self._value = value
        self._key = key

    @property
    def value(self):
        for element in self._container:
            if getattr(element, self._key) == self._value:
                return element

        raise ValueError("specified element does not exist")

    @value.setter
    def value(self, value):
        existing = None
        for element in self._container:
            if getattr(element, self._key) == self._value:
                existing = element
                break

        if existing is not None:
            self._container.remove(existing)

        self._container.append(value)


class XPathSelector(object):
    def __init__(self, xpath):
        """Creates an instance of XPathSelector

        Arguments:
            xpath - a string containing an xpath expression

        """
        self._tokens = tokenize(xpath)


    def __call__(self, obj):
        """Returns a reference to an attribute on the provided object

        Using the defined xpath, an attribute is selected from the provided
        object and returned.

        Arguments:
            obj - a GI object

        Raises:
            A ValueError is raised if the specified element in a list cannot be
            found.

        Returns:
            an XPathAttribute that reference the specified attribute

        """
        current = obj
        for token in self._tokens[:-1]:
            # If the object is contained within a list, we will need to iterate
            # through the tokens until we find a token that is a field of the
            # object.
            if token.name not in current.fields:
                if current is obj:
                    continue

                raise ValueError('cannot find attribute {}'.format(token.name))

            # If the token is a ListElement, try to find the matching element
            if isinstance(token, ListElement):
                for element in getattr(current, token.name):
                    if getattr(element, token.key) == token.value:
                        current = element
                        break

                else:
                    raise ValueError('unable to find {}'.format(token.value))

            else:
                # Attribute the variable matching the name of the token
                current = getattr(current, token.name)

        # Process the final token
        token = self._tokens[-1]

        # If the token represents a list element, find the element in the list
        # and return an XPathElement
        if isinstance(token, ListElement):
            container = getattr(current, token.name)
            for element in container:
                if getattr(element, token.key) == token.value:
                    return XPathElement(container, token.key, token.value)

            else:
                raise ValueError('unable to find {}'.format(token.value))

        # Otherwise, return the object as an XPathAttribute
        return XPathAttribute(current, token.name)

    @property
    def tokens(self):
        """The tokens in the xpath expression"""
        return self._tokens


# A global cache to avoid repeated parsing of known xpath expressions
__xpath_cache = dict()


def reset_cache():
    global __xpath_cache
    __xpath_cache = dict()


def getxattr(obj, xpath):
    """Return an attribute on the provided object

    The xpath is parsed and used to identify an attribute on the provided
    object. The object is expected to be a GI object where each attribute that
    is accessible via an xpath expression is contained in the 'fields'
    attribute of the object (NB: this is not true of GI lists, which do not
    have a 'fields' attribute).

    A selector is create for each xpath and used to find the specified
    attribute. The accepted xpath expressions are those supported by the
    XPathSelector class. The parsed xpath expression is cached so that
    subsequent parsing is unnecessary. However, selectors are stored in a
    global dictionary and this means that this function is not thread-safe.

    Arguments:
        obj   - a GI object
        xpath - a string containing an xpath expression

    Returns:
        an attribute on the provided object

    """
    if xpath not in __xpath_cache:
        __xpath_cache[xpath] = XPathSelector(xpath)

    return __xpath_cache[xpath](obj).value


def setxattr(obj, xpath, value):
    """Set the attribute referred to by the xpath

    Arguments:
        obj   - a GI object
        xpath - a string containing an xpath expression
        value - the new value of the attribute

    """
    if xpath not in __xpath_cache:
        __xpath_cache[xpath] = XPathSelector(xpath)

    __xpath_cache[xpath](obj).value = value
