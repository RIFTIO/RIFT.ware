# -*- test-case-name: pyflakes -*-
# (c) 2005-2008 Divmod, Inc.
# See LICENSE file for details

import __builtin__
import compiler
import sys
import os

from compiler import ast


class Message(object):
    message = ''
    message_args = ()

    def __init__(self, filename, lineno):
        self.filename = filename
        self.lineno = lineno

    def __str__(self):
        return '%s:%s: %s' % (self.filename,
                              self.lineno,
                              self.message % self.message_args)


class UnusedImport(Message):
    message = '%r imported but unused'

    def __init__(self, filename, lineno, name):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, )


class RedefinedWhileUnused(Message):
    message = 'redefinition of unused %r from line %r'

    def __init__(self, filename, lineno, name, orig_lineno):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, orig_lineno)


class ImportShadowedByLoopVar(Message):
    message = 'import %r from line %r shadowed by loop variable'

    def __init__(self, filename, lineno, name, orig_lineno):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, orig_lineno)


class ImportStarUsed(Message):
    message = "'from %s import *' used; unable to detect undefined names"

    def __init__(self, filename, lineno, modname):
        Message.__init__(self, filename, lineno)
        self.message_args = (modname, )


class UndefinedName(Message):
    message = 'undefined name %r'

    def __init__(self, filename, lineno, name):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, )


class UndefinedLocal(Message):
    message = ("local variable %r (defined in enclosing scope on line %r) "
               "referenced before assignment")

    def __init__(self, filename, lineno, name, orig_lineno):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, orig_lineno)


class DuplicateArgument(Message):
    message = 'duplicate argument %r in function definition'

    def __init__(self, filename, lineno, name):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, )


class RedefinedFunction(Message):
    message = 'redefinition of function %r from line %r'

    def __init__(self, filename, lineno, name, orig_lineno):
        Message.__init__(self, filename, lineno)
        self.message_args = (name, orig_lineno)


class LateFutureImport(Message):
    message = 'future import(s) %r after other statements'

    def __init__(self, filename, lineno, names):
        Message.__init__(self, filename, lineno)
        self.message_args = (names, )


class Binding(object):
    """
    @ivar used: pair of (L{Scope}, line-number) indicating the scope and
                line number that this binding was last used
    """

    def __init__(self, name, source):
        self.name = name
        self.source = source
        self.used = False

    def __str__(self):
        return self.name

    def __repr__(self):
        return '<%s object %r from line %r at 0x%x>' % (
            self.__class__.__name__,
            self.name,
            self.source.lineno,
            id(self))


class UnBinding(Binding):
    '''Created by the 'del' operator.'''


class Importation(Binding):

    def __init__(self, name, source):
        name = name.split('.')[0]
        super(Importation, self).__init__(name, source)


class Assignment(Binding):
    pass


class FunctionDefinition(Binding):
    pass


class Scope(dict):
    importStarred = False       # set to True when import * is found

    def __repr__(self):
        return '<%s at 0x%x %s>' % (self.__class__.__name__, id(self),
                                    dict.__repr__(self))

    def __init__(self):
        super(Scope, self).__init__()


class ClassScope(Scope):
    pass


class FunctionScope(Scope):
    """
    I represent a name scope for a function.

    @ivar globals: Names declared 'global' in this function.
    """

    def __init__(self):
        super(FunctionScope, self).__init__()
        self.globals = {}


class ModuleScope(Scope):
    pass


class Checker(object):
    nodeDepth = 0
    traceTree = False

    def __init__(self, tree, filename='(none)'):
        self.deferred = []
        self.dead_scopes = []
        self.messages = []
        self.filename = filename
        self.scopeStack = [ModuleScope()]
        self.futuresAllowed = True

        self.handleChildren(tree)
        for handler, scope in self.deferred:
            self.scopeStack = scope
            handler()
        del self.scopeStack[1:]
        self.popScope()
        self.check_dead_scopes()

    def defer(self, callable):
        '''Schedule something to be called after just before completion.

        This is used for handling function bodies, which must be deferred
        because code later in the file might modify the global scope. When
        `callable` is called, the scope at the time this is called will be
        restored, however it will contain any new bindings added to it.
        '''
        self.deferred.append((callable, self.scopeStack[:]))

    def scope(self):
        return self.scopeStack[-1]
    scope = property(scope)

    def popScope(self):
        self.dead_scopes.append(self.scopeStack.pop())

    def check_dead_scopes(self):
        for scope in self.dead_scopes:
            for importation in scope.itervalues():
                if (isinstance(importation, Importation) and
                    not importation.used):
                    self.report(UnusedImport,
                                importation.source.lineno, importation.name)

    def pushFunctionScope(self):
        self.scopeStack.append(FunctionScope())

    def pushClassScope(self):
        self.scopeStack.append(ClassScope())

    def report(self, messageClass, *args, **kwargs):
        self.messages.append(messageClass(self.filename, *args, **kwargs))

    def handleChildren(self, tree):
        for node in tree.getChildNodes():
            self.handleNode(node)

    def handleNode(self, node):
        if self.traceTree:
            print '  ' * self.nodeDepth + node.__class__.__name__
        self.nodeDepth += 1
        nodeType = node.__class__.__name__.upper()
        if nodeType not in ('STMT', 'FROM'):
            self.futuresAllowed = False
        try:
            handler = getattr(self, nodeType)
            handler(node)
        finally:
            self.nodeDepth -= 1
        if self.traceTree:
            print '  ' * self.nodeDepth + 'end ' + node.__class__.__name__

    def ignore(self, node):
        pass

    STMT = PRINT = PRINTNL = TUPLE = LIST = ASSTUPLE = ASSATTR = \
    ASSLIST = GETATTR = SLICE = SLICEOBJ = IF = CALLFUNC = DISCARD = \
    RETURN = ADD = MOD = SUB = NOT = UNARYSUB = INVERT = ASSERT = COMPARE = \
    SUBSCRIPT = AND = OR = TRYEXCEPT = RAISE = YIELD = DICT = LEFTSHIFT = \
    RIGHTSHIFT = KEYWORD = TRYFINALLY = WHILE = EXEC = MUL = DIV = POWER = \
    FLOORDIV = BITAND = BITOR = BITXOR = LISTCOMPFOR = LISTCOMPIF = \
    AUGASSIGN = BACKQUOTE = UNARYADD = GENEXPR = GENEXPRFOR = GENEXPRIF = \
    IFEXP = handleChildren

    CONST = PASS = CONTINUE = BREAK = ELLIPSIS = ignore

    def addBinding(self, lineno, value, reportRedef=True):
        '''Called when a binding is altered.

        - `lineno` is the line of the statement responsible for the change
        - `value` is the optional new value, a Binding instance, associated
          with the binding; if None, the binding is deleted if it exists.
        - if `reportRedef` is True (default), rebinding while unused will be
          reported.
        '''
        if (isinstance(self.scope.get(value.name), FunctionDefinition)
                    and isinstance(value, FunctionDefinition)):
            self.report(RedefinedFunction,
                        lineno, value.name,
                        self.scope[value.name].source.lineno)

        if not isinstance(self.scope, ClassScope):
            for scope in self.scopeStack[::-1]:
                if (isinstance(scope.get(value.name), Importation)
                        and not scope[value.name].used
                        and reportRedef):

                    self.report(RedefinedWhileUnused,
                                lineno, value.name,
                                scope[value.name].source.lineno)

        if isinstance(value, UnBinding):
            try:
                del self.scope[value.name]
            except KeyError:
                self.report(UndefinedName, lineno, value.name)
        else:
            self.scope[value.name] = value

    def WITH(self, node):
        """
        Handle C{with} by adding bindings for the name or tuple of names it
        puts into scope and by continuing to process the suite within the
        statement.
        """
        # for "with foo as bar", there is no AssName node for "bar".
        # Instead, there is a Name node. If the "as" expression assigns to
        # a tuple, it will instead be a AssTuple node of Name nodes.
        #
        # Of course these are assignments, not references, so we have to
        # handle them as a special case here.

        self.handleNode(node.expr)

        if isinstance(node.vars, ast.AssTuple):
            varNodes = node.vars.nodes
        elif node.vars is not None:
            varNodes = [node.vars]
        else:
            varNodes = []

        for varNode in varNodes:
            self.addBinding(varNode.lineno, Assignment(varNode.name, varNode))

        self.handleChildren(node.body)

    def GLOBAL(self, node):
        """
        Keep track of globals declarations.
        """
        if isinstance(self.scope, FunctionScope):
            self.scope.globals.update(dict.fromkeys(node.names))

    def LISTCOMP(self, node):
        for qual in node.quals:
            self.handleNode(qual)
        self.handleNode(node.expr)

    GENEXPRINNER = LISTCOMP

    def FOR(self, node):
        """
        Process bindings for loop variables.
        """
        vars = []

        def collectLoopVars(n):
            if hasattr(n, 'name'):
                vars.append(n.name)
            else:
                for c in n.getChildNodes():
                    collectLoopVars(c)

        collectLoopVars(node.assign)
        for varn in vars:
            if (isinstance(self.scope.get(varn), Importation)
                    # unused ones will get an unused import warning
                    and self.scope[varn].used):
                self.report(ImportShadowedByLoopVar,
                            node.lineno, varn, self.scope[varn].source.lineno)

        self.handleChildren(node)

    def NAME(self, node):
        """
        Locate the name in locals / function / globals scopes.
        """
        # try local scope
        importStarred = self.scope.importStarred
        try:
            self.scope[node.name].used = (self.scope, node.lineno)
        except KeyError:
            pass
        else:
            return

        # try enclosing function scopes

        for scope in self.scopeStack[-2:0:-1]:
            importStarred = importStarred or scope.importStarred
            if not isinstance(scope, FunctionScope):
                continue
            try:
                scope[node.name].used = (self.scope, node.lineno)
            except KeyError:
                pass
            else:
                return

        # try global scope

        importStarred = importStarred or self.scopeStack[0].importStarred
        try:
            self.scopeStack[0][node.name].used = (self.scope, node.lineno)
        except KeyError:
            if ((not hasattr(__builtin__, node.name))
                    and node.name not in ['__file__']
                    and not importStarred):
                self.report(UndefinedName, node.lineno, node.name)

    def FUNCTION(self, node):
        if getattr(node, "decorators", None) is not None:
            self.handleChildren(node.decorators)
        self.addBinding(node.lineno, FunctionDefinition(node.name, node))
        self.LAMBDA(node)

    def LAMBDA(self, node):
        for default in node.defaults:
            self.handleNode(default)

        def runFunction():
            args = []

            def addArgs(arglist):
                for arg in arglist:
                    if isinstance(arg, tuple):
                        addArgs(arg)
                    else:
                        if arg in args:
                            self.report(DuplicateArgument, node.lineno, arg)
                        args.append(arg)

            self.pushFunctionScope()
            addArgs(node.argnames)
            for name in args:
                self.addBinding(node.lineno, Assignment(name, node),
                                reportRedef=False)
            self.handleNode(node.code)
            self.popScope()

        self.defer(runFunction)

    def CLASS(self, node):
        self.addBinding(node.lineno, Assignment(node.name, node))
        for baseNode in node.bases:
            self.handleNode(baseNode)
        self.pushClassScope()
        self.handleChildren(node.code)
        self.popScope()

    def ASSNAME(self, node):
        if node.flags == 'OP_DELETE':
            if (isinstance(self.scope, FunctionScope) and
                node.name in self.scope.globals):
                del self.scope.globals[node.name]
            else:
                self.addBinding(node.lineno, UnBinding(node.name, node))
        else:
            # if the name hasn't already been defined in the current scope
            if (isinstance(self.scope, FunctionScope) and
                node.name not in self.scope):
                # for each function or module scope above us
                for scope in self.scopeStack[:-1]:
                    if not isinstance(scope, (FunctionScope, ModuleScope)):
                        continue
                    # if the name was defined in that scope, and the name has
                    # been accessed already in the current scope, and hasn't
                    # been declared global
                    if (node.name in scope
                            and scope[node.name].used
                            and scope[node.name].used[0] is self.scope
                            and node.name not in self.scope.globals):
                        # then it's probably a mistake
                        self.report(UndefinedLocal,
                                    scope[node.name].used[1],
                                    node.name,
                                    scope[node.name].source.lineno)
                        break

            self.addBinding(node.lineno, Assignment(node.name, node))

    def ASSIGN(self, node):
        self.handleNode(node.expr)
        for subnode in node.nodes[::-1]:
            self.handleNode(subnode)

    def IMPORT(self, node):
        for name, alias in node.names:
            name = alias or name
            importation = Importation(name, node)
            self.addBinding(node.lineno, importation)

    def FROM(self, node):
        if node.modname == '__future__':
            if not self.futuresAllowed:
                self.report(LateFutureImport,
                            node.lineno, [n[0] for n in node.names])
        else:
            self.futuresAllowed = False

        for name, alias in node.names:
            if name == '*':
                self.scope.importStarred = True
                self.report(ImportStarUsed, node.lineno, node.modname)
                continue
            name = alias or name
            importation = Importation(name, node)
            if node.modname == '__future__':
                importation.used = (self.scope, node.lineno)
            self.addBinding(node.lineno, importation)


def check(codeString, filename):
    try:
        tree = compiler.parse(codeString)
    except (SyntaxError, IndentationError):
        value = sys.exc_info()[1]
        try:
            (lineno, offset, line) = value[1][1:]
        except IndexError:
            print >> sys.stderr, 'could not compile %r' % (filename, )
            return 1
        if line.endswith("\n"):
            line = line[:-1]
        print >> sys.stderr, '%s:%d: could not compile' % (filename, lineno)
        print >> sys.stderr, line
        print >> sys.stderr, " " * (offset-2), "^"
        return 1
    else:
        w = Checker(tree, filename)
        w.messages.sort(lambda a, b: cmp(a.lineno, b.lineno))
        for warning in w.messages:
            print warning
        return len(w.messages)


def checkPath(filename):
    if os.path.exists(filename):
        return check(file(filename, 'U').read(), filename)


def main(args):
    warnings = 0
    if args:
        for arg in args:
            if os.path.isdir(arg):
                for dirpath, dirnames, filenames in os.walk(arg):
                    for filename in filenames:
                        if filename.endswith('.py'):
                            warnings += checkPath(
                                os.path.join(dirpath, filename))
            else:
                warnings += checkPath(arg)
    else:
        warnings += check(sys.stdin.read(), '<stdin>')

    return warnings > 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
