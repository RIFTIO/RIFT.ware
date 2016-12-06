from rift.rwlib.schema import (
    collect_child_names,
)

from .util import(
    get_protobuf_type,
)

class SchemaElement(object):
    '''
    Base class for member data intermediate representation.

    In essence a SchemaElement is a wrapper around a protobuf to provide "undo" capability.
    We want this "undo" capability in order to be less "chatty" with DTS.
    '''
    def __init__(self, protobuf_schema, descriptor, path=None, rooted=False, **kwargs):
        self.protobuf_schema = protobuf_schema
        self.descriptor = descriptor
        self.path = path
        self.children = dict()
        self.parent = None
        self.protobuf = None
        self._protobuf_type = None
        self.is_rooted = rooted # true if this element corresponds to a dts registration point
        self.dirty = False
        self.operation = "create"
        self.old_values = dict() # used to "undo" changes before they've been executed
        self.deleted = False # used to lazily delete the protobuf

    @property
    def protobuf_type(self):
        '''
        Used to return a constructor for the protobuf associated with the SchemaElement
        '''

        if self._protobuf_type is None:
            self._protobuf_type = get_protobuf_type(self.path, self.protobuf_schema)
        return self._protobuf_type

    @property
    def name(self):
        '''
        Used to return the name of the descriptor.
        '''

        return self.descriptor.get_name()

    @property
    def legal_children(self):
        '''
        Used to return the list of names of this SchemaElement's descriptor's children.
        '''

        return collect_child_names(self.descriptor)

    def add_child(self, child):
        '''
        Add the given child to this SchemaElement's children.
        '''

        assert child.name in self.legal_children, "%s not in %s" % (child.name, self.legal_children)

        child.parent = self
        
        self.children[child.name] = child

    def remove_child(self, child):
        '''
        Remove the given child from this SchemaElement's children and delete it's protobuf
        '''

        assert child.name in self.legal_children, "%s not in %s" % (child.name, self.legal_children)

        del self.children[child.name].protobuf
        del self.children[child.name]

    def find_child(self, key):
        '''
        Used to return this SchemaElement's child that matches the given key.
        Or None if there is no such child.
        '''

        return self.children.get(key, None)

    def is_dirty(self):
        '''
        Returns True if this SchemaElement, or any of it's descendents, is dirty
        '''

        if self.dirty:
            return True

        is_dirty = False
        for _, child in self.children.items():
            is_dirty = is_dirty or child.is_dirty()

        return is_dirty

    def mark_dirty(self):
        '''
        Marks the current SchemaElement as dirty, and propagates that state upwards.
        '''

        self.dirty = True

        if (not self.is_rooted) and (self.parent is not None):
            # don't propagate upwards past registration points
            self.parent.mark_dirty()

    def mark_clean(self):
        '''
        Marks the current SchemaElement as clean, and propagates that stat to it's children.
        '''

        self.dirty = False

        for _, child in self.children.items():
            child.mark_clean()

    def delete(self):
        '''
        Mark the current SchemaElement for deletion.
        '''

        self.deleted = True
        self.mark_dirty()
        self.operation = "delete"

    def un_delete(self):
        '''
        Remove the deletion marking.
        '''

        assert self.deleted
        self.deleted = False
        self.mark_clean()
        self.operation = "create"

    def set(self, name, value):
        '''
        Set the attribute on this SchemaElement's protobuf with the given name to the given value.
        '''

        self.operation = "update"
        if self.protobuf is None:
            self.construct_protobuf()
            self.operation = "create"

        self.mark_dirty()
        try:
            self.old_values[name] = getattr(self.protobuf, name)
            setattr(self.protobuf, name, value)
        except AttributeError:
            # retry with mangled name
            new_name = name.replace("-","_")
            self.old_values[name] = getattr(self.protobuf, new_name)
            setattr(self.protobuf, new_name, value)
        
        # ATTN: keep delta

    def get(self, name):
        '''
        Return the attribute on this SchemaElement's protobuf with the given name, or None.
        '''

        if self.protobuf is not None:
            try:
                return getattr(self.protobuf, name)
            except AttributeError:
                # retry with mangled name
                new_name = name.replace("-","_")
                return getattr(self.protobuf, new_name)
        else:
            return None
    
class Container(SchemaElement):
    '''
    IR representing a Yang Container.
    '''

    def construct_protobuf(self):
        '''
        Build the current Container's protobuf, and the parent's protobuf.
        '''

        if self.protobuf is not None:
            return

        if (self.parent is not None) and (not self.is_rooted):
            # if we are not at a registration point, continue constructing upwards
            self.parent.construct_protobuf()

        self.protobuf = self.protobuf_type()

class List(SchemaElement):
    '''
    IR representing a Yang List.
    '''

    def add_child(self, key, child):
        '''
        Add the child with the given key to the list.
        The child must be a ListEntry associated with this list.
        Overrides SchemaElement's add_child becuase lists are not first-class protobufs.
        '''

        assert child.name == self.name
        assert isinstance(child, ListEntry), "%s is not a ListEntry" % type(child)
        
        child.parent = self

        self.children[key] = child

    def remove_child(self, child):
        '''
        Remove the given ListElement from the IR tree and delete it's associated protobuf.
        '''

        assert child.name == self.name
        assert isinstance(child, ListEntry), "%s is not a ListEntry" % type(child)

        del_key = None
        for key, value in self.children.items():
            if value is child:
                del_key = key
                break

        if del_key is None:
            raise ValueError("Cannot remove list entry (%s) since it is not a child of %s" % (child.descriptor.get_name(),
                                                                                              seld.descriptor.get_name()))
        # delete protobuf out of the list
        i = 0
        while i < len(self.protobuf):
            
            pb = self.protobuf[i]
            found = True
            for k,v in self.children[del_key].kv_pairs.items():
                k = k.replace("-","_")
                found = found and (getattr(pb,k) == v)

            if found:
                del self.protobuf[i]
                break

            i += 1
        assert found, "didn't find protobuf to delete"

        # remove child from ir tree
        del self.children[del_key].protobuf 
        del self.children[del_key]

    def construct_protobuf(self):
        if self.protobuf is not None:
            return

        if self.parent is not None:
            # lists always need their parents to be constructed becuase they are not first-class protobufs
            self.parent.construct_protobuf()

        assert self.parent.protobuf is not None, "Failed to construct protobuf from path %s" % self.parent.path

        self.protobuf = getattr(self.parent.protobuf, self.name)

class ListEntry(SchemaElement):
    '''
    IR representing an entry in a Yang list.
    '''

    def __init__(self, *args, **kwargs):
        '''
        Special case for ListEntry since it has concrete keys.
        '''

        super(ListEntry, self).__init__(*args, **kwargs)

        self.kv_pairs = kwargs["kv_pairs"]
        self.path = kwargs["path"]
        self.key = kwargs["key"]
        self.pending = True

        kv_pairs = dict()
        for k, v in self.kv_pairs:
            self.path += "[%s='%s']" % (k,v)
            kv_pairs[k] = v
        self.kv_pairs = kv_pairs # save the keys and values from the zip iterator

    def construct_protobuf(self):
        '''

        '''

        if (self.protobuf is not None):
            return

        # list entries always need the parent list constructed becuase lists are not first-class protobufs
        self.parent.construct_protobuf()

        self.protobuf = self.parent.protobuf.add()

        for key_name, key_value in self.kv_pairs.items():
            try:
                setattr(self.protobuf, key_name, key_value)
            except AttributeError:
                # retry with mangled name
                new_name = key_name.replace("-","_")
                setattr(self.protobuf, new_name, key_value)


