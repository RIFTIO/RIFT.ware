
from rift.rwlib.schema import (
    collect_child_names,
    find_child_by_name,
    get_key_names,
)

from .util import (
    get_protobuf_type,
    KeyCategory,
)

class PathWalker(object):
    '''
    Base class for all mechanisms wanting the schema path-walking syntax.
    '''

    def __init__(self, schema):

        '''
        this awkward initialization is used so that we can walk the yang schema with dotted-indexing.
        '''
        super(__class__, self).__setattr__("_schema",schema)
        super(__class__, self).__setattr__("_root_node",schema.root_node)
        super(__class__, self).__setattr__("_current_descriptor", self._root_node)
        super(__class__, self).__setattr__("_found_module", False)

    def __dir__(self):
        return collect_child_names(self._current_descriptor)

    def __getattr__(self, name):
        '''
        Walk the schema tree with the given name.
        Throws an AttributeError if there is no schema element with that name.
        In the case where a schema element conflicts with either the api or python keywords, 
          use a leading underscore to ensure proper access to that element.
        '''

        # yang names "lose" when the clash with api names and python keywords
        if name.startswith("_"):
            name = name[1:]


        if (self._current_descriptor is self._root_node) and (not self._found_module):
            # look for the module name
            name = name.replace("_","-")
            child = self._schema.model.search_module_name_rev(name, None)

        else:
            # look for the yang element name
            child = find_child_by_name(self._current_descriptor, name)

            if child is None:
                # retry with unmangled name
                new_name = name.replace("_","-")
                child = find_child_by_name(self._current_descriptor, new_name)

        if child is None:
            raise AttributeError("%s not a child of %s" % (name, self._current_descriptor.get_name()))

        if self._found_module:
            self._current_descriptor = child
        else:
            self._found_module = True
            return self # no need for callback on module

        value = self._getattr_hook(name)
        if value is not None:
            return value
        else:
            return self

    def __getitem__(self, keys):
        '''
        Walk the schema tree to the list with the given keys.
        Throws a ValueErro if the wrong number of keys is used.
        '''

        actual_keys = get_key_names(self._current_descriptor)

        if isinstance(keys, tuple):
            kv_pairs = zip(actual_keys, keys)
        else:
            if len(actual_keys) > 1:
                raise ValueError("expected %d keys, received 1 key" % len(actual_keys))
            kv_pairs = [(actual_keys[0], keys)]

        value = self._getitem_hook(kv_pairs, keys)
        if value is not None:
            return value
        else:
            return self

class ProtobufBuilder(PathWalker):
    '''
    Used to construct protobufs with the same syntax as member data and keys.
    '''

    def __init__(self, schema):
        super(ProtobufBuilder, self).__init__(schema)
        super(__class__, self).__setattr__("_schema", schema)
        super(__class__, self).__setattr__("_path", "")

    def _getattr_hook(self, name):

        self._path += "/%s:%s" % (self._current_descriptor.get_prefix(), self._current_descriptor.get_name())

    def _getitem_hook(self, kv_pairs, keys):
        raise KeyError("protobuf constructor doesn't take keys")

    def __call__(self):
        '''
        Return a constructed protobuf.
        '''

        protobuf_type = get_protobuf_type(self._path, self._schema.protobuf_schema)
        return protobuf_type()
        

class KeyBuilder(PathWalker):
    '''
    Used to construct keys with the same syntax as member data and protobuf constructors.
    '''

    def __init__(self, schema):
        super(KeyBuilder, self).__init__(schema)
        super(__class__, self).__setattr__("_keys", dict())
        super(__class__, self).__setattr__("_path", None)
        super(__class__, self).__setattr__("_category", KeyCategory.Any)

    def __call__(self, category):
        '''
        Set the type of key.
        '''

        if self._path is not None:
            # must "call" the KeyBuilder to set the category before doing any path traversal
            raise ValueError("%s is not callable" % self._current_descriptor.get_name())

        assert isinstance(category, KeyCategory), "Received type %s, expected type %s" % (type(category), type(KeyCategory))

        self._category = category
        return self

    def _getattr_hook(self, name):

        if self._path is None:
            self._path = self._category.value + ","
        

        self._path += "/%s:%s" % (self._current_descriptor.get_prefix(), self._current_descriptor.get_name())

    def _getitem_hook(self, kv_pairs, keys):
        self._keys[self._path] = keys
        predicate = ""

        for k,v in kv_pairs:
            predicate += "[%s='%s']" % (k,v)


        self._path += predicate
