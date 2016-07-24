"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file   rwcli_writer.py
@author Balaji Rajappa (balaji.rajappa@riftio.com)
@date   10/21/2015
@brief CLI pretty print for config xml 

The config data is displayed as a set of CLI commands, so that it can be loaded
are re-executed again.
"""

import lxml.etree as et
import io
import logging
import sys

logger = logging.getLogger(__name__)

RW_BASE_NS = "http://riftio.com/ns/riftware-1.0/rw-base"

class Command(object):
    """Represents a single command line. A command line may be composed of a
    single yang node or several yang nodes.
    """
    EVENT_NONE = 0
    EVENT_PUSH = 1
    EVENT_POP  = 2
    EVENT_UNKNOWN = 3

    def __init__(self, name, ynode, out_file, indent, text = None):
        """Constructor for a Command.

        Arguments:
            name - Starting keyword of the command
            ynode - Yang node representing the top of the command
            out_file - File to which the output is to be written to
            indent - Command starts at the given indentation
            text - Value of the starting keyword
        """
        self.name = name
        self.tokens = [name]
        self.ynode_stack = [ynode]
        self.out_file = out_file
        self.indent = indent
        if text is not None:
            self.add(text)

    def add(self, token, text = None):
        """Adds a child node to the command.

        Arguments:
            token - Keyword of the child element
            text  - Value of the child element
        """
        self.tokens.append(token)
        if text is not None: self.tokens.append(text)

    def drain(self):
        """Outputs the gathered command line to the file
        """ 
        if self.tokens:
            self.out_file.write(self.indent*' ' + ' '.join(self.tokens) +'\n')
            self.tokens.clear()

    def handle_event(self, event, element, text):
        """This method is invoked when an XML element is either available or
        ended. This method will be invoked only for non-mode nodes, which 
        completes in a single line.

        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_UNKNOWN - when the model doesn't have the encountered element
            EVENT_POP - Instructs the ConfigWriter to Pop this node
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        if event == 'start':
            ynode = self.ynode_stack[-1].search_child(
                                element.localname, element.namespace)
            if ynode is None:
                return Command.EVENT_UNKNOWN, None
            if ynode.is_key():
                # Skip the key keyword and add only the value
                self.add(text)
            else:
                # Add the keyword and value to command line
                self.add(element.localname, text)
            self.ynode_stack.append(ynode)
        elif event == 'end':
            self.ynode_stack.pop()
            if element.localname == self.name:
                # The root node of the command had ended, complete the command
                self.drain()
                return Command.EVENT_POP, None
        return Command.EVENT_NONE, None

    @staticmethod
    def is_mode(ynode):
        """Checks if the Yang node is a mode or not"""
        ext = ynode.search_extension(
                'http://riftio.com/ns/riftware-1.0/rw-cli-ext', 'new-mode')
        return ext is not None

    @staticmethod
    def is_mode_path(ynode):
        """Checks if the Yang node is a mode or not"""
        if Command.is_mode(ynode): return True
        child_node = ynode.get_first_child()
        while child_node is not None:
            if Command.is_mode_path(child_node): return True
            child_node = child_node.get_next_sibling()
        return False

    @staticmethod
    def is_list_with_keys(ynode):
        return ynode.is_listy() and ynode.has_keys()

    @staticmethod
    def is_list_with_keys_path(ynode):
        # A list with key node shall be treated as a Command
        if Command.is_list_with_keys(ynode):
            return True
        child_node = ynode.get_first_child()
        while child_node is not None:
            if Command.is_list_with_keys(child_node): return True
            child_node = child_node.get_next_sibling()
        return False


    @staticmethod
    def new(ynode, out_file, element, text, indent):
        """Creates a new Command or a ModeBlock. If the yang node representing
        the element is a mode, then create a ModeBlock. Otherwise a Command is
        created.

        Arguments:
            out_file - File to which the config is to be written
            element - qualified element representing the keyword
            text    - Value of the element
            indent  - indentation at which the new Command should start
        """
        if Command.is_mode(ynode):
            return ModeBlock(element.localname, ynode, out_file, indent)
        elif Command.is_mode_path(ynode):
            return ModePathBlock(element.localname, ynode, out_file, indent)
        elif Command.is_list_with_keys_path(ynode):
            return ListKeyBlock(element.localname, ynode, out_file, indent)
        else:
            return Command(element.localname, ynode, out_file, indent, text)

    def __str__(self):
        """String representation of Command"""
        return 'Command({})'.format(self.tokens)

class UnknownCommand(Command):
    """Special command that handles Unknown nodes. This command consumes the
    entire unknown element tree and instructs the ConfigWriter to use the parent
    until the unknown subtree is consumed.
    """
    def __init__(self, name):
        """Constructor of UnkknownCommand

        Arguments:
            name - Keyword of the unknown command
        """
        super(UnknownCommand, self).__init__(name, None, None, 0)

    def handle_event(self, event, element, text):
        """Invoked when an UnknownCommand subtree elements are encountered.

        Consumes all the elements of the subtree.
        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_POP - Instructs the ConfigWriter to Pop this node
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        # Consume everything except for the end event of this node
        if event == 'end' and element.localname == self.tokens[0]:
            return Command.EVENT_POP, None
        return Command.EVENT_NONE, None

    def __str__(self):
        """String representation of UnknownCommand"""
        return 'UnknownCommand({})'.format(self.tokens[0])


class ModeCommand(Command):
    """Special type of command dealing with the the mode enter command.
    A mode enter command is handled within a ModeBlock. Unlike other commands,
    a mode enter command should be drained when a non-key element is 
    encountered. Other commands are drained when their element tag ends.
    """
    def __init__(self, name, ynode, out_file, indent):
        """Constructor for a ModeCommand.

        Arguments:
            name - Starting keyword of the mode
            ynode - Yang node representing the top of the mode
            out_file - File to which the output is to be written to
            indent - Command starts at the given indentation
            text - Value of the starting keyword
        """
        super(ModeCommand, self).__init__(name, ynode, out_file, indent)

    def handle_event(self, event, element, text):
        """This method is invoked when an XML element is either available or
        ended. For a Mode enter command, it waits until all the keys are
        available. Once they are availble, instructs the ModeBlock to proceed
        with other elements. 

        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_UNKNOWN - when the model doesn't have the encountered element
            EVENT_PUSH - A new command is created. The created command/block is
                         returned along with this return code.
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        if event == 'start':
            ynode = self.ynode_stack[-1].search_child(
                            element.localname, element.namespace)
            if ynode is None:
                return Command.EVENT_UNKNOWN, None
            if ynode.is_key():
                # Skip the key keyword and add only the value
                self.add(text)
                # To handle keys with child nodes (like enums)
                self.ynode_stack.append(ynode)
            else:
                # node that is not a key, create a new node
                self.drain()
                return (Command.EVENT_PUSH, 
                        Command.new(ynode, self.out_file, element,
                                    text, self.indent + 2))
        elif event == 'end':
            # Will drain before the mode ends
            self.ynode_stack.pop()
        return Command.EVENT_NONE, None

    def __str__(self):
        """String representation of ModeCommand"""
        return 'ModeCommand({})'.format(self.tokens)

class ListKeyBlock(Command):
    """Serves as a holder for Lists with Keys, which has a parent container that
    is not a mode.

    This is a pseudo command and doesn't print a command. When a list with key
    is part of a container that is not a mode, each list item has to be printed
    in its own line along with the ancestor nodes. This is used to hold the
    ancestor nodes, create new list commands and prepend the ancestors to each
    list command.

    Example: cloud account c1 account-type cloudsim
    Here *cloud* is a container and *account* is a list. Each cloud account has
    to be printed in a seperate line.
    """
    def __init__(self, name, ynode, out_file, indent):
        """Constructor for a ListKeyBlock.

        Arguments:
            name - Starting keyword of the mode
            ynode - Yang node representing the top of the mode
            out_file - File to which the output is to be written to
            indent - Command starts at the given indentation
            text - Value of the starting keyword
        """
        super(ListKeyBlock, self).__init__(name, ynode, out_file, indent)
        self.list_done = False
        self.cmd_created = False

    def handle_event(self, event, element, text):
        """This method is invoked when an XML element is either available or
        ended.  

        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_UNKNOWN - when the model doesn't have the encountered element
            EVENT_PUSH - A new command is created. The created command/block is
                         returned along with this return code.
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        if event == 'start':
            child_node = self.ynode_stack[-1].search_child(element.localname, 
                                                element.namespace)
            if child_node is None:
                return Command.EVENT_UNKNOWN, None

            if Command.is_list_with_keys(child_node):
                # Prepend the ancestors for each list
                cmd = Command.new(child_node, self.out_file, element, 
                                text, self.indent)
                cmd.tokens = self.tokens + cmd.tokens
                self.list_done = True
                self.cmd_created = True
                return Command.EVENT_PUSH, cmd

            if self.list_done:
                # Not a list, but after a list, treat it as a new command
                cmd = Command.new(child_node, self.out_file, element, 
                                text, self.indent)
                cmd.tokens = self.tokens + cmd.tokens
                self.cmd_created = True
                return Command.EVENT_PUSH, cmd

            # Add the ancestor nodes before list, they need to be prepended to
            # each list
            self.ynode_stack.append(child_node)
            if child_node.is_key():
                self.list_done = True
            else:
                self.tokens.append(element.localname)
            if text is not None: self.tokens.append(text)

        if event == 'end':
            if element.localname == self.name:
                if not self.cmd_created:
                    self.drain()
                return Command.EVENT_POP, None
            self.ynode_stack.pop()

        return Command.EVENT_NONE, None

    def exit(self):
        pass

    def __str__(self):
        """String representation of ListKeyBlock"""
        return 'ListKeyBlock({})'.format(self.tokens)

class ModeBlock(object):
    """Represents a block of commands that are executed within a mode. This
    includes the mode enter line. A ModeBlock doesn't output anything directly,
    rather the job is delegted to Command objects.
    """
    def __init__(self, mode_name, ynode, out_file, indent):
        """Constructor of ModeBlock.

        Arguments:
            mode_name - Name of the mode (mode keyword)
            ynode     - Yand node representing the mode
            out_file  - File to which the output is written
            indent    - Start indentation for the mode
        """
        self.mode_name = mode_name
        self.out_file = out_file
        self.indent = indent
        self.ynode = ynode
        self.command = ModeCommand(mode_name, ynode, out_file, indent)

    def handle_event(self, event, element, text):
        """This method is invoked when an XML element is either available or
        ended. A Modeblock creates other Command/Blocks. Also instructs the
        ConfigWriter to pop to previous Command/Block when the ModeBlock ends. 

        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_UNKNOWN - when the model doesn't have the encountered element
            EVENT_PUSH - A new command is created. The created command/block is
                         returned along with this return code.
            EVENT_POP  - This mode is done. Pop to previous if available.
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        # A ModeBlock doesn't write any command, it delegates to ModeCommand or
        # creates new Commands within the block which then writes the respective
        # command
        # If there is a mode enter command available, let it handle the event
        if event == 'end' and  self.mode_name == element.localname:
            if self.command is not None:
                self.command.drain()
                self.command = None
        if self.command is not None:
            op, cmd = self.command.handle_event(event, element, text)
            if op == Command.EVENT_PUSH:
                # Mode Command is done
                self.command = None
            return op, cmd

        if event == 'start':
            # New child node
            child_node = self.ynode.search_child(element.localname, 
                                                element.namespace)
            if child_node is None:
                return Command.EVENT_UNKNOWN, None

            return (Command.EVENT_PUSH, 
                    Command.new(child_node, self.out_file, element, 
                                text, self.indent + 2))
        
        if event == 'end':
            # Only mode end event should be received here
            assert self.mode_name == element.localname
            self.exit()
            return Command.EVENT_POP, None
        
        # It should not reach here! 
        return Command.EVENT_NONE, None
            
    def exit(self):
        """Outputs 'exit' when the mode ends"""
        self.out_file.write(self.indent*' ' + 'exit' + '\n')

    def __str__(self):
        """String representation of ModeBlock"""
        return 'ModeBlock({})'.format(self.mode_name)

class ModePathBlock(object):
    """Represents a holder for a mode that has non-mode nodes as ancestors.

    When a mode node has non-mode nodes as ancestors, the non-mode nodes has to
    be printed along with the mode node in the mode line. 
    Example: vnf-config vnf trafgen 0, here vnf-config is a non-mode node.

    Stores the ancestor nodes in the mode path and whenever the mode node is
    encountered then a new mode block is created. Control gets back to this
    block when the ModeBlock is done.
    """
    def __init__(self, mode_name, ynode, out_file, indent):
        """Constructor of ModeBlock.

        Arguments:
            mode_name - Name of the mode (mode keyword)
            ynode     - Yand node representing the mode
            out_file  - File to which the output is written
            indent    - Start indentation for the mode
        """
        self.mode_path_tokens = [mode_name]
        self.out_file = out_file
        self.ynode_stack = [ynode]
        self.indent = indent

    def handle_event(self, event, element, text):
        """This method is invoked when an XML element is either available or
        ended.  

        Arguments:
            event - 'start' of the element or 'end' of the element
            element - Qualified element
            text - Value of the element

        Returns:
            EVENT_UNKNOWN - when the model doesn't have the encountered element
            EVENT_PUSH - A new command is created. The created command/block is
                         returned along with this return code.
            EVENT_NONE - Instructs the ConfigWriter to do nothing 
        """
        if event == 'start':
            child_node = self.ynode_stack[-1].search_child(element.localname, 
                                                element.namespace)
            if child_node is None:
                return Command.EVENT_UNKNOWN, None

            if Command.is_mode(child_node):
                # Create new mode and prepend the ancestors to the command
                mode = Command.new(child_node, self.out_file, element, 
                                text, self.indent)
                mode.command.tokens = self.mode_path_tokens + mode.command.tokens
                return Command.EVENT_PUSH, mode

            self.ynode_stack.append(child_node)
            self.mode_path_tokens.append(element.localname)
            if text is not None: self.mode_path_tokens.append(text)

        if event == 'end':
            if element.localname == self.mode_path_tokens[0]:
                return Command.EVENT_POP, None
            self.ynode_stack.pop()

        return Command.EVENT_NONE, None

    def exit(self):
        pass

    def __str__(self):
        """String representation of ModeBlock"""
        return 'ModePathBlock({})'.format(self.mode_path_tokens)

class ConfigBlock(ModeBlock):
    """Special ModeBlock that form the toplevel container for all the config
    commands. This specialization is required as the XML doesn't have a 'config'
    node.
    """
    def __init__(self, out_file, model):
        """Constructor for the config mode block.

        out_file - File to which the config output is to be written
        model    - Yand Model used by the CLI
        """
        ynode = model.get_root_node()
        super(ConfigBlock, self).__init__('config', ynode, out_file, 0)

    def exit(self):
        """Outputs 'end' instead of 'exit'
        """
        self.out_file.write(self.indent*' ' + 'end' + '\n')
        

class ConfigWriter(object):
    """Parses the XML in an event-driven way and maintains the command hierarchy
    with a command stack. Delegates the event to the current command/block and
    based on their return value, pushes a new command/block or pop's to an 
    older command/block.
    """ 
    def __init__(self, model, in_file):
        """Constructor for ConfigWriter.

        Arguments:
            model - Yang Model used by the CLI
            in_file - XML in a file like object, which is to be converted
        """
        self.model = model
        self.in_file = in_file
        self.out_file = None
        self.cmd_stack = []

    def write(self, out_file=sys.stdout):
        """Writes the config file (in CLI command format).

        Arguments:
            out_file - File to which the config is to be written
                       Defaults to stdout.
        """
        self.cmd_stack.append(ConfigBlock(out_file, self.model))
        indent = 0
        for event, element in et.iterparse(self.in_file,
                                events=('start', 'end'),
                                tag=et.Element):
            qelem = et.QName(element)
            if (qelem.localname == 'data' and qelem.namespace == RW_BASE_NS):                    
                continue

            cur_cmd = self.cmd_stack[-1] 
            etext = None if element.text is None else element.text.strip()

            logger.debug('Event: %s CurCmd: %s Element: %s', 
                         event, cur_cmd, qelem.localname)

            ret, new_cmd = cur_cmd.handle_event(event, qelem, etext)
            if ret == Command.EVENT_PUSH:
                self.cmd_stack.append(new_cmd)
            elif ret == Command.EVENT_POP:
                mode = self.cmd_stack.pop()
            elif ret == Command.EVENT_UNKNOWN:
                # Unknown element encountered
                # Ignore the entire element tree
                logger.error('Ignoring unknown element <%s>', element.tag)
                new_cmd = UnknownCommand(qelem.localname)
                self.cmd_stack.append(new_cmd)
                element.clear()

        mode = self.cmd_stack.pop() # Pop the ConfigBlock
        if mode.command is not None:
            # if the Mode command was not executed drain it
            mode.command.drain()
        mode.exit()
        assert len(self.cmd_stack) == 0

def pretty_print(
        model, 
        xml_str, 
        pretty_print_routine, 
        out_file_name=None):
    """Prints the given xml_str to Yang aware pretty format.

    If a 'config_writer' is chosen, then the XML is converted to CLI command
    format.

    Arguments:
        xml_str - XML in string format
        pretty_print_routine - The routine that decided which Writer to be used
        out_file - File to which the pretty print to be written (defaults to
                   stdout).
    """
    is_file = False
    if out_file_name is None:
        out_file = sys.stdout
    else:
        print('Writing to file', out_file_name)
        out_file = open(out_file_name, 'w')
        is_file = True
    if(pretty_print_routine == 'config_writer' or
       pretty_print_routine == 'config_writer_file'):
        xml_stream = io.BytesIO(xml_str.encode('utf-8'))
        config_writer = ConfigWriter(model, xml_stream)
        config_writer.write(out_file)
    
    if is_file: out_file.close()

# vim: ts=4 et sw=4
