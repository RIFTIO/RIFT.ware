#!/usr/bin/env python
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file rw_pbc_fref_test.py
# @author Sujithra Periasamy
# @date 2015/28/08
#
"""
import argparse
import logging
import unittest
import sys
import os
import xmlrunner
from gi.repository import RwYang, GLib
from gi.repository import RwKeyspec
from gi.repository import CompanyYang
from gi.repository import ProtobufC

logger = logging.getLogger(__name__)

class TestFieldRefs(unittest.TestCase):
    def setUp(self):
        pass

    def test_fref_methods(self):
        fref = ProtobufC.FieldReference.alloc()
        self.assertTrue(fref)
        co = CompanyYang.Company()
        cinfo = co.contact_info
        cinfo.name = "abc@riftio.com"
        cinfo.address = "Burlington"
        pbcmp = cinfo.to_pbcm()
        fref.goto_proto_name(pbcmp, "name")
        fref.pb_text_set("abc@riftio.com")
        fref.goto_proto_name(pbcmp, "address")
        fref.pb_text_set("Burlington")
        fref.goto_proto_name(pbcmp, "name")
        self.assertTrue(fref.is_field_present())
        fref.goto_proto_name(pbcmp, "address")
        self.assertTrue(fref.is_field_present())
        fref.goto_proto_name(pbcmp, "phone_number")
        self.assertFalse(fref.is_field_present())
        fref.mark_field_deleted()
        self.assertTrue(fref.is_field_deleted())

    def test_fref_goto_xpath(self):
        fref = ProtobufC.FieldReference.alloc()
        self.assertTrue(fref)
        co = CompanyYang.Company()
        co.contact_info.name = "abc@riftio.com"
        co.contact_info.address = "Burlington"
        co.contact_info.phone_number = "1234567"
        emp = co.employee.add()
        emp.id = 1234
        emp.name = "abc"
        emp.title = "MTS"
        co1 = CompanyYang.Company()
        pbcmc = co1.to_pbcm()
        fref.goto_xpath(pbcmc, "D,/company:company/company:contact-info")
        self.assertTrue(fref.is_field_missing())
        cinfo = co1.contact_info
        fref.goto_xpath(pbcmc, "D,/company:company/company:contact-info")
        self.assertTrue(fref.is_field_present())
        fref.goto_xpath(pbcmc, "D,/company:company/company:contact-info/company:name")
        self.assertTrue(fref.is_field_missing())
        fref.pb_text_set("abc@riftio.com")
        self.assertTrue(fref.is_field_present())
        fref.goto_xpath(pbcmc, "D,/company:company/company:contact-info/company:address")
        self.assertTrue(fref.is_field_missing())
        fref.pb_text_set("Burlington")
        self.assertTrue(fref.is_field_present())
        fref.goto_xpath(pbcmc, "D,/company:company/company:contact-info/company:phone-number")
        self.assertTrue(fref.is_field_missing())
        fref.pb_text_set("1234567")
        self.assertTrue(fref.is_field_present())
        emp = co1.employee.add()
        emp.id = 1234
        fref.goto_xpath(pbcmc, "D,/company:company/company:employee[company:id='1234']/company:name")
        self.assertTrue(fref.is_field_missing())
        fref.pb_text_set("abc")
        self.assertTrue(fref.is_field_present())
        fref.goto_xpath(pbcmc, "D,/company:company/company:employee[company:id='1234']/company:title")
        self.assertTrue(fref.is_field_missing())
        fref.pb_text_set("MTS")
        self.assertTrue(fref.is_field_present())
        self.assertTrue(ProtobufC.Message.is_equal_deep(None, pbcmc, co.to_pbcm()))
        fref.goto_proto_name(emp.to_pbcm(), "id")
        try:
          fref.pb_text_set("hello")
        except GLib.GError:
          pass
        except e:
          self.fail('Unexpected exception thrown:', e)
        else:
          self.fail('ExpectedException not thrown')
        

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
