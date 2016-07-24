#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file test_gi_pbc.py
#
"""
import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import gi
import re
import gc
import gi
gi.require_version('TestPyGiPbYang', '1.0')
from gi.repository import TestPyGiPbYang

logger = logging.getLogger(__name__)

class TesttPyGiPb(unittest.TestCase):
    def setUp(self):
        self._reset_refs()

    def _set_serial(self, obj):
        obj.v = str(self._serial)
        self._serial += 1

    def _add_list(self, list):
        self._refs.append(list)
        return list

    def _add_list_entry(self, list):
        entry = list.add()
        self._set_serial(entry)
        self._refs.append(entry)
        return entry

    def _add_cont(self, cont):
        self._set_serial(cont)
        self._refs.append(cont)
        return cont

    def _reset_refs(self):
        self._refs = []
        self._serial = 1

    def _build_top(self, top):
        self._reset_refs()

        # list li1aa
        li1aa = self._add_list(top.li1aa)
        li1aa_0 = self._add_list_entry(li1aa)
        li1aa_1 = self._add_list_entry(li1aa)

        # list li1aa_1.li2ab
        li1aa_1_li2ab = self._add_list(li1aa_1.li2ab)
        li1aa_1_li2ab_0 = self._add_list_entry(li1aa_1_li2ab)
        li1aa_1_li2ab_1 = self._add_list_entry(li1aa_1_li2ab)
        li1aa_1_li2ab_2 = self._add_list_entry(li1aa_1_li2ab)

        # container li1aa.li2ab.ct3ac
        self._add_cont(li1aa_1_li2ab_0.ct3ac)
        li1aa_1_li2ab_1_ct3ac = self._add_cont(li1aa_1_li2ab_1.ct3ac)
        li1aa_1_li2ab_2_ct3ac = self._add_cont(li1aa_1_li2ab_2.ct3ac)

        # list li1aa.li2ab.ct3ac.li4ad
        self._add_list(li1aa_1_li2ab_1_ct3ac.li4ad)
        li1aa_1_li2ab_1_ct3ac_li4ad = self._add_list(li1aa_1_li2ab_1_ct3ac.li4ad)
        self._add_list_entry(li1aa_1_li2ab_1_ct3ac_li4ad)
        li1aa_1_li2ab_1_ct3ac_li4ad_1 = self._add_list_entry(li1aa_1_li2ab_1_ct3ac_li4ad)
        self._add_list_entry(li1aa_1_li2ab_1_ct3ac_li4ad)

        # container li1aa.li2ab.ct3ac.ct5ae
        # container li1aa.li2ab.ct3ac.ct5af
        self._add_cont(li1aa_1_li2ab_2_ct3ac.ct5ae)
        self._add_cont(li1aa_1_li2ab_2_ct3ac.ct5ae)
        self._add_cont(li1aa_1_li2ab_2_ct3ac.ct5af)
        self._add_cont(li1aa_1_li2ab_2_ct3ac.ct5af)

        # container li1aa.li2ab.ct3ag
        li1aa_1_li2ab_0_ct3ag = self._add_cont(li1aa_1_li2ab_0.ct3ag)
        self._add_cont(li1aa_1_li2ab_1.ct3ag)

        # list li1aa.li2ab.ct3ag.li4ah
        li1aa_1_li2ab_0_ct3ag_li4ah = self._add_list(li1aa_1_li2ab_0_ct3ag.li4ah )
        self._add_list_entry(li1aa_1_li2ab_0_ct3ag_li4ah)

        # container li1aa.li2ab.ct3ag.ct5ai
        self._add_cont(li1aa_1_li2ab_0_ct3ag.ct5ai )

        # container li1aa.li2ab.ct3ag.ct5aj
        # None

        # list li1aa.li2ab.li3ak
        # None

        # container li1aa.li2ab.li3ak.ct4al
        # None

        # container li1aa.li2ab.li3ak.ct4am
        # None

        # list li1aa.li2bb
        li1aa_0_li2bb_0 = self._add_list_entry(li1aa_0.li2bb)
        li1aa_0_li2bb_1 = self._add_list_entry(li1aa_0.li2bb)

        # container li1aa.li2bb.ct3bc
        # Indirect only

        # list li1aa.li2bb.ct3bc.li4bd
        self._add_list_entry(li1aa_0_li2bb_1.ct3bc.li4bd)
        self._add_list_entry(li1aa_0_li2bb_1.ct3bc.li4bd)

        # container li1aa.li2bb.ct3bc.ct4be
        self._add_cont(li1aa_0_li2bb_1.ct3bc.ct4be)

        # container li1aa.li2bb.ct3bc.ct4bf
        # None

        # list li1aa.li2bb.li3bk
        # None
        li1aa_0_li2bb_0_li3bk_0 = self._add_list_entry(li1aa_0_li2bb_0.li3bk)
        li1aa_0_li2bb_0_li3bk_1 = self._add_list_entry(li1aa_0_li2bb_0.li3bk)

        # container li1aa.li2bb.li3bk.ct4bl
        self._add_cont(li1aa_0_li2bb_0_li3bk_0.ct4bl)

        # container li1aa.li2bb.li3bk.ct4bm
        self._add_cont(li1aa_0_li2bb_0_li3bk_0.ct4bm)

        # container li1aa.ct2cc
        li1aa_1_ct2cc = self._add_cont(li1aa_1.ct2cc)

        # list li1aa.ct2cc.li3cd
        self._add_list_entry(li1aa_1_ct2cc.li3cd)
        self._add_list_entry(li1aa_1_ct2cc.li3cd)

        # container li1aa.ct2cc.ct3ce
        # None

        # container li1aa.ct2cc.ct3cf
        self._add_cont(li1aa_1_ct2cc.ct3cf)

        # container li1aa_1.ct2cg
        # Indirect only

        # list li1aa_1.ct2cg.li3ch
        self._add_list_entry(li1aa_1.ct2cg.li3ch)

        # container li1aa_1.ct2cg.ct3cj
        self._add_cont(li1aa_1.ct2cg.ct3cj)

        # container ct1dc
        # Indirect only

        # list ct1dc.li2dd
        self._add_list_entry(top.ct1dc.li2dd)

        # container ct1dc.ct2de
        self._add_cont(top.ct1dc.ct2de)

        # container ct1dg
        ct1dg = self._add_cont(top.ct1dg)

        # container ct1dg.ct2di
        self._add_cont(ct1dg.ct2di)

        # container ct1dg.ct2dj
        self._add_cont(ct1dg.ct2dj)

        # list li1ek
        self._add_list_entry(top.li1ek)
        self._add_list_entry(top.li1ek)
        
        # container li1ek.ct2el
        self._add_cont(self._add_list_entry(top.li1ek).ct2el)

        # list li1fa
        li1fa_0 = self._add_list_entry(top.li1fa)

        # list li1fa.li2fb
        li1fa_0_li2fb_0 = self._add_list_entry(li1fa_0.li2fb)

        # list li1fa.li2fb.li3fc
        self._add_list_entry(li1fa_0_li2fb_0.li3fc)

    def _pop_stride(self,n):
        c = 0
        while len(self._refs):
            if len(self._refs) < n:
                v = self._refs.pop()
                if c == n:
                    c = 0
                    v = None
                    gc.collect()
                else:
                    self._refs.insert(0, v)
                    c = c + 1
            else:
                self._refs = self._refs[n:] + self._refs[:n]
                v = self._refs.pop()
                v = None

    def test_a(self):
        for constr in [
                TestPyGiPbYang.YangData_TestPyGiPb_TopBumpy,
                TestPyGiPbYang.YangData_TestPyGiPb_TopFlat,
                TestPyGiPbYang.YangData_TestPyGiPb_TopContInl,
                TestPyGiPbYang.YangData_TestPyGiPb_TopListInl,
                TestPyGiPbYang.YangData_TestPyGiPb_TopOddInl,
                TestPyGiPbYang.YangData_TestPyGiPb_TopEvenInl]:
            for n in [ 1, 2, 3, 5, 7, 13, 16, 17, 25, 31, 43 ]:
                print( "Iter: %s %d" % (str(constr), n) )
                sys.stdout.flush()
                top = constr()
                self._build_top(top)
                self._pop_stride(n)
                self._refs = None


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose')

    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()



