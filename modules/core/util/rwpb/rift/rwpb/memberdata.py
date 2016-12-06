import contextlib

from .ir import (
    Container,
    List,
    ListEntry,
)
from .pathwalker import(
    PathWalker
)

@contextlib.contextmanager
def accumulate(protobuf_builder):
    '''
    Context manager to build up changes in the pending data.
    Throws a RuntimeError if there are pending changes when control enters the block.
    On any exception, the pending changes are discarded, and that exception is re-raised.
    '''

    if protobuf_builder.data._current_element.is_dirty():
        raise RuntimeError("member data has uncommited changes")

    try:
        yield
        protobuf_builder.data.execute()
    except:
        protobuf_builder.data.discard()        
        raise

class MemberData(PathWalker):
    '''
    PathWalker subclass to used to build the IR for the member data and to handle the interations with DTS.

    Given this Yang:

      module test {
      list foo {
        key bar;
        leaf bar {
          type int32;
        }
        leaf goo {
          type string;
        }
        leaf baz {
          type string;
        }
        container cont {
          leaf bot {
            type string;
          }
        }
      }
    }

    A registry that publishes this list as member data can be used to do the following:

    some_registry.data.foo[0].goo = "asdf"
    some_registry.data.foo[0].baz = "qwer"

    What this will do is construct a list element with the key of "0" and assign to the two leaves. 
    If the list entry with key of "0" doesn't exist, that list entry will be implicitly constructed.

    As a shortcut you could do this:

    list = some_registry.data.foo[0]
    list.goo = "asdf"
    list.baz = "qwer"


    But not this:

    list = some_registry.data.foo[0]
    list.cont.bot = "qwer"
    list.goo = "asdf"

    Because MemberData is a singleton cursor that walks over the schema tree, the second line will
    move the cursor to the container. And this container does not have a child named "goo".

    '''

    def __init__(self, schema, registrations, registration_paths):
        super(MemberData, self).__init__(schema)

        '''
        this awkward initialization is used so that we can walk the yang schema with dotted-indexing.
        '''
        super(__class__, self).__setattr__("_schema", schema)
        super(__class__, self).__setattr__("_root_element", Container(self._schema.protobuf_schema, self._schema.root_node, path="/"))
        super(__class__, self).__setattr__("_registration_paths", registration_paths) # registered paths with dts
        super(__class__, self).__setattr__("_registrations", registrations) # dts registration handles

        self.reset()

    def reset(self):
        '''
        Reset the internal state so we can walk down a different branch of schema.
        '''

        super(__class__, self).__setattr__("_current_descriptor", self._schema.root_node)
        super(__class__, self).__setattr__("_current_element", self._root_element)
        super(__class__, self).__setattr__("_current_name", None)
        super(__class__, self).__setattr__("_current_path", None)
        super(__class__, self).__setattr__("_found_module", False)

    def get_protobuf(self):
        '''
        Return the protobuf at the current point in the member data tree, or None.
        '''

        if self._current_element.protobuf is None:
            return None

        return self._current_element.protobuf

    def execute(self):
        '''
        Take the pending changes and issue DTS transactions to make them "real"
        '''

        queued_elements = [self._root_element]
        while len(queued_elements):
            current_element = queued_elements.pop(0)
            if current_element.is_rooted and current_element.dirty:
                reg_handle = self._registrations[current_element.path]

                if isinstance(current_element, List):
                    del_children = list()
                    for key, item in current_element.children.items():
                        if not item.is_dirty():
                            continue
                        if item.deleted:
                            del_children.append(item)

                        if item.operation == "create":
                            reg_handle.create_element(item.path, item.protobuf)
                        elif item.operation == "update":
                            reg_handle.update_element(item.path, item.protobuf)
                        elif item.operation == "delete":
                            reg_handle.delete_element(item.path)
                        else:
                            assert False, "un-implemented operation %s" % current_element.operation

                        item.mark_clean()
                        item.old_values = dict()
                        item.pending = False
                    for child in del_children:
                        current_element.remove_child(child)

                else:
                    if current_element.operation == "create":
                        reg_handle.create_element(current_element.path, current_element.protobuf)                    
                    elif current_element.operation == "update":
                        reg_handle.update_element(current_element.path, current_element.protobuf)                    
                    elif current_element.operation == "delete":
                        reg_handle.delete_element(current_element.path)
                    else:
                        assert False, "un-implemented operation %s" % current_element.operation

                    if current_element.deleted:
                        parent = current_element.parent
                        parent.remove_child(current_element)

                current_element.mark_clean()
                current_element.old_value = None

                continue

            for _, child in current_element.children.items():
                if child.descriptor.is_leafy():
                    # can't register for leaves
                    continue
                queued_elements.append(child)                    

    def discard(self):
        '''
        Discard the pending changes.
        '''

        queued_elements = [self._root_element]
        while len(queued_elements):
            current_element = queued_elements.pop(0)
            if isinstance(current_element, List):
                del_children = list()
                for key, item in current_element.children.items():
                    item.mark_clean()

                    if item.deleted:
                        # make new pb in parent list and copy over values
                        item.un_delete()
                    elif not item.pending:
                        # list entry exists in dts cache
                        for k, v in item.old_values.items():
                            setattr(item.protobuf, k, v)
                        item.old_values = dict()
                    else:
                        # list entry doesn't exists in dts cache
                        del_children.append(item)
                for child in del_children:
                    current_element.remove_child(child)
            else:
                current_element.mark_clean()
                if current_element.deleted:
                    current_element.un_delete()
                else:
                    for k, v in current_element.old_values.items():
                        setattr(current_element.protobuf, k, v)
                    current_element.old_values = dict()

            for _, child in current_element.children.items():
                if child.descriptor.is_leafy():
                    # can't register for leaves
                    continue
                queued_elements.append(child)                    
        
    def _getattr_hook(self, name):
        '''
        Take the requested name and walk down that branch of the schema
        It is a precondition that that the name actually corresponds to child element in the schema .
        '''

        if self._current_path is None:
            self._current_path = "D,"

        current_name = self._current_descriptor.get_name()
        self._current_path += "/%s:%s" % (self._current_descriptor.get_prefix(), current_name)
            
        # walk down the schema tree
        schema = self._current_element.find_child(current_name)

        if schema is None:
            if self._current_path in self._registration_paths:
                is_rooted = True
            else:
                is_rooted = False
            # we haven't walked this branch yet, so make the child
            if self._current_descriptor.is_leafy():
                return self._current_element.get(name)
            elif self._current_descriptor.is_listy() and self._current_descriptor.has_keys():
                # make a list
                schema = List(self._schema.protobuf_schema, self._current_descriptor, self._current_path, is_rooted)
            elif not self._current_descriptor.is_leafy():
                # make a container
                schema = Container(self._schema.protobuf_schema, self._current_descriptor, self._current_path, is_rooted)
            else:
                assert False, "other kinds of lists are not implemented"

            self._current_element.add_child(schema)

        self._current_element = schema

    def _getitem_hook(self, kv_pairs, keys):
        '''
        Walk the member data down to the list with the given keys
        '''

        assert self._current_descriptor.is_listy()

        assert self._current_descriptor.has_keys()

        schema = self._current_element.find_child(keys)
        
        if schema is None:
            assert isinstance(self._current_element, List), "can only add ListEntry to a List"
            schema = ListEntry(self._schema.protobuf_schema, self._current_descriptor, kv_pairs=kv_pairs, path=self._current_path, key=keys)
            self._current_element.add_child(keys, schema)
        
        self._current_element = schema

        return self
    
    def __setattr__(self, name, value):
        '''
        Set schema element with the given name to the given value.
        '''

        if name in self.__dict__:
            self.__dict__[name] = value
            return

        self._current_element.set(name, value)

    def __delattr__(self, name):
        '''
        Delete the schema element with the given name.
        '''

        child = self.__getattr__(name)
        if self._current_descriptor.is_leafy():
            raise RunetimeError("can't delete leafy elements")
        else:
            child._current_element.delete()

    def __delitem__(self, name):
        '''
        Delete the schema list element with the given name.
        '''

        child = self.__getitem__(name)
        child._current_element.delete()

