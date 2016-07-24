# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2013 Dieter Verfaillie <dieterv@optionexplicit.be>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#


from __future__ import absolute_import


try:
    from collections import Counter
except ImportError:
    # collections.Counter for Python 2.6, backported from
    # http://hg.python.org/cpython/file/d047928ae3f6/Lib/collections/__init__.py#l402

    from operator import itemgetter
    from heapq import nlargest
    from itertools import repeat, ifilter

    class Counter(dict):
        '''Dict subclass for counting hashable items.  Sometimes called a bag
        or multiset.  Elements are stored as dictionary keys and their counts
        are stored as dictionary values.

        >>> c = Counter('abcdeabcdabcaba')  # count elements from a string

        >>> c.most_common(3)                # three most common elements
        [('a', 5), ('b', 4), ('c', 3)]
        >>> sorted(c)                       # list all unique elements
        ['a', 'b', 'c', 'd', 'e']
        >>> ''.join(sorted(c.elements()))   # list elements with repetitions
        'aaaaabbbbcccdde'
        >>> sum(c.values())                 # total of all counts
        15

        >>> c['a']                          # count of letter 'a'
        5
        >>> for elem in 'shazam':           # update counts from an iterable
        ...     c[elem] += 1                # by adding 1 to each element's count
        >>> c['a']                          # now there are seven 'a'
        7
        >>> del c['b']                      # remove all 'b'
        >>> c['b']                          # now there are zero 'b'
        0

        >>> d = Counter('simsalabim')       # make another counter
        >>> c.update(d)                     # add in the second counter
        >>> c['a']                          # now there are nine 'a'
        9

        >>> c.clear()                       # empty the counter
        >>> c
        Counter()

        Note:  If a count is set to zero or reduced to zero, it will remain
        in the counter until the entry is deleted or the counter is cleared:

        >>> c = Counter('aaabbc')
        >>> c['b'] -= 2                     # reduce the count of 'b' by two
        >>> c.most_common()                 # 'b' is still in, but its count is zero
        [('a', 3), ('c', 1), ('b', 0)]

        '''
        # References:
        #   http://en.wikipedia.org/wiki/Multiset
        #   http://www.gnu.org/software/smalltalk/manual-base/html_node/Bag.html
        #   http://www.demo2s.com/Tutorial/Cpp/0380__set-multiset/Catalog0380__set-multiset.htm
        #   http://code.activestate.com/recipes/259174/
        #   Knuth, TAOCP Vol. II section 4.6.3

        def __init__(self, iterable=None, **kwds):
            '''Create a new, empty Counter object.  And if given, count elements
            from an input iterable.  Or, initialize the count from another mapping
            of elements to their counts.

            >>> c = Counter()                           # a new, empty counter
            >>> c = Counter('gallahad')                 # a new counter from an iterable
            >>> c = Counter({'a': 4, 'b': 2})           # a new counter from a mapping
            >>> c = Counter(a=4, b=2)                   # a new counter from keyword args

            '''
            self.update(iterable, **kwds)

        def __missing__(self, key):
            'The count of elements not in the Counter is zero.'
            # Needed so that self[missing_item] does not raise KeyError
            return 0

        def most_common(self, n=None):
            '''List the n most common elements and their counts from the most
            common to the least.  If n is None, then list all element counts.

            >>> Counter('abcdeabcdabcaba').most_common(3)
            [('a', 5), ('b', 4), ('c', 3)]

            '''
            # Emulate Bag.sortedByCount from Smalltalk
            if n is None:
                return sorted(self.iteritems(), key=itemgetter(1), reverse=True)
            return nlargest(n, self.iteritems(), key=itemgetter(1))

        def elements(self):
            '''Iterator over elements repeating each as many times as its count.

            >>> c = Counter('ABCABC')
            >>> sorted(c.elements())
            ['A', 'A', 'B', 'B', 'C', 'C']

            # Knuth's example for prime factors of 1836:  2**2 * 3**3 * 17**1
            >>> prime_factors = Counter({2: 2, 3: 3, 17: 1})
            >>> product = 1
            >>> for factor in prime_factors.elements():     # loop over factors
            ...     product *= factor                       # and multiply them
            >>> product
            1836

            Note, if an element's count has been set to zero or is a negative
            number, elements() will ignore it.

            '''
            # Emulate Bag.do from Smalltalk and Multiset.begin from C++.
            for elem, count in self.iteritems():
                for _ in repeat(None, count):
                    yield elem

        # Override dict methods where necessary

        @classmethod
        def fromkeys(cls, iterable, v=None):
            # There is no equivalent method for counters because setting v=1
            # means that no element can have a count greater than one.
            raise NotImplementedError(
                'Counter.fromkeys() is undefined.  Use Counter(iterable) instead.')

        def update(self, iterable=None, **kwds):
            '''Like dict.update() but add counts instead of replacing them.

            Source can be an iterable, a dictionary, or another Counter instance.

            >>> c = Counter('which')
            >>> c.update('witch')           # add elements from another iterable
            >>> d = Counter('watch')
            >>> c.update(d)                 # add elements from another counter
            >>> c['h']                      # four 'h' in which, witch, and watch
            4

            '''
            # The regular dict.update() operation makes no sense here because the
            # replace behavior results in the some of original untouched counts
            # being mixed-in with all of the other counts for a mismash that
            # doesn't have a straight-forward interpretation in most counting
            # contexts.  Instead, we implement straight-addition.  Both the inputs
            # and outputs are allowed to contain zero and negative counts.

            if iterable is not None:
                if hasattr(iterable, 'iteritems'):
                    if self:
                        self_get = self.get
                        for elem, count in iterable.iteritems():
                            self[elem] = self_get(elem, 0) + count
                    else:
                        dict.update(self, iterable)  # fast path when counter is empty
                else:
                    self_get = self.get
                    for elem in iterable:
                        self[elem] = self_get(elem, 0) + 1
            if kwds:
                self.update(kwds)

        def subtract(self, iterable=None, **kwds):
            '''Like dict.update() but subtracts counts instead of replacing them.
            Counts can be reduced below zero.  Both the inputs and outputs are
            allowed to contain zero and negative counts.

            Source can be an iterable, a dictionary, or another Counter instance.

            >>> c = Counter('which')
            >>> c.subtract('witch')             # subtract elements from another iterable
            >>> c.subtract(Counter('watch'))    # subtract elements from another counter
            >>> c['h']                          # 2 in which, minus 1 in witch, minus 1 in watch
            0
            >>> c['w']                          # 1 in which, minus 1 in witch, minus 1 in watch
            -1

            '''
            if hasattr(iterable, 'iteritems'):
                for elem, count in iterable.iteritems():
                    self[elem] -= count
            else:
                for elem in iterable:
                    self[elem] -= 1

        def copy(self):
            'Return a shallow copy.'
            return self.__class__(self)

        def __reduce__(self):
            return self.__class__, (dict(self), )

        def __delitem__(self, elem):
            'Like dict.__delitem__() but does not raise KeyError for missing values.'
            if elem in self:
                dict.__delitem__(self, elem)

        def __repr__(self):
            if not self:
                return '%s()' % self.__class__.__name__
            items = ', '.join(map('%r: %r'.__mod__, self.most_common()))
            return '%s({%s})' % (self.__class__.__name__, items)

        # Multiset-style mathematical operations discussed in:
        #       Knuth TAOCP Volume II section 4.6.3 exercise 19
        #       and at http://en.wikipedia.org/wiki/Multiset
        #
        # Outputs guaranteed to only include positive counts.
        #
        # To strip negative and zero counts, add-in an empty counter:
        #       c += Counter()

        def __add__(self, other):
            '''Add counts from two counters.

            >>> Counter('abbb') + Counter('bcc')
            Counter({'b': 4, 'c': 2, 'a': 1})

            '''
            if not isinstance(other, Counter):
                return NotImplemented
            result = Counter()
            for elem in set(self) | set(other):
                newcount = self[elem] + other[elem]
                if newcount > 0:
                    result[elem] = newcount
            return result

        def __sub__(self, other):
            ''' Subtract count, but keep only results with positive counts.

            >>> Counter('abbbc') - Counter('bccd')
            Counter({'b': 2, 'a': 1})

            '''
            if not isinstance(other, Counter):
                return NotImplemented
            result = Counter()
            for elem in set(self) | set(other):
                newcount = self[elem] - other[elem]
                if newcount > 0:
                    result[elem] = newcount
            return result

        def __or__(self, other):
            '''Union is the maximum of value in either of the input counters.

            >>> Counter('abbb') | Counter('bcc')
            Counter({'b': 3, 'c': 2, 'a': 1})

            '''
            if not isinstance(other, Counter):
                return NotImplemented
            _max = max
            result = Counter()
            for elem in set(self) | set(other):
                newcount = _max(self[elem], other[elem])
                if newcount > 0:
                    result[elem] = newcount
            return result

        def __and__(self, other):
            ''' Intersection is the minimum of corresponding counts.

            >>> Counter('abbb') & Counter('bcc')
            Counter({'b': 1})

            '''
            if not isinstance(other, Counter):
                return NotImplemented
            _min = min
            result = Counter()
            if len(self) < len(other):
                self, other = other, self
            for elem in ifilter(self.__contains__, other):
                newcount = _min(self[elem], other[elem])
                if newcount > 0:
                    result[elem] = newcount
            return result

    if __name__ == '__main__':
        import doctest
        print(doctest.testmod())
