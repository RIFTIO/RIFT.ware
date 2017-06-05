#!/usr/bin/env python3

"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file rwdts-pytest.py
@author Sameer Nayak (sameer.nayak@riftio.com)
@date 06/28/2015
"""

import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import tempfile
import re
import time
import weakref
import datetime
import json

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwTasklet', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwDtsYang', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')


from gi.repository import RwDts
import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDtsToyTaskletYang as toyyang
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
from gi.repository import CF, RwTasklet, RwDts, RwDtsToyTaskletYang
from gi.repository import RwDtsYang
from gi.repository import GObject, RwTaskletPlugin
from gi.repository import RwBaseYang, RwTypes


""" 
Class: TestRwDtsApi
"""
logger = logging.getLogger(__name__)
class TestRwDtsApi(unittest.TestCase):
    def setUp(self):
      logger.info("%s "%self._testMethodName)

    def test_json_api_onelevel(self):
      """
      Step: Create Message with one level 
      Step: Call Api
      Validate: Valid Json print
      """
      company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
      company_entry.name = 'Rift'
      company_entry.description = 'cloud technologies'
      company_property = company_entry.property.add()
      company_property.name = 'test-property'
      company_property.value = 'test-value'
      #self.apih.trace(self.default_xpath)
      logger.debug("dir  (%s)" %dir(RwDts))

      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      json_data = json.loads(json_str)
      logger.info("string  (%s)" %json.dumps(json_data))

      str1 = str(RwDtsToyTaskletYang.CompanyConfig.from_pbcm(pbcm))
      logger.info("string  (%s)" %json.dumps(str1))
      self.assertEqual(len(str1),len(json_str))

    def test_json_api_array(self):
      """
      Step: Create Message with Array
      Step: Call Api
      Validate: strlen check
      """
      company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
      company_entry.name = 'Rift'
      company_entry.description = 'cloud technologies'
      for i in range(1,10):
        company_property = company_entry.property.add()
        company_property.name = 'test-property'+str(i)
        company_property.value = 'test-value'+str(i)

      logger.debug("dir  (%s)" %dir(RwDts))
      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      logger.debug("string  (%s)" %json_str)

      str1 = str(RwDtsToyTaskletYang.CompanyConfig.from_pbcm(pbcm))
      logger.debug("string  (%s)" %str1)
      self.assertEqual(len(str1),len(json_str))

    def test_json_api_container(self):
      """
      Step: Create Message with many arrays list
      Step: Call Api
      Validate: Valid Json print
      """
      company_entry = RwDtsToyTaskletYang.Company.new()
      company_entry.name = 'Rift'
      for id in range(1, 50):
        emp = company_entry.employee.add()
        emp.id = id
        emp.name = 'jdoe' + str(id)
        emp.title = '978-863-00' + str(id)

      for id in range(1, 20):
        product = company_entry.product.add()
        product.id = id
        product.name = 'nfv-' +str(id)
        product.msrp = '1c-' +str(id)

      for id in range(1, 10):
        bm = company_entry.board.member.add()
        bm.name = 'rick' + str(id)
        bm.details = '978-863-00' + str(id)

      logger.debug("dir  (%s)" %dir(RwDts))
      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      logger.debug("string  (%s)" %json_str)

      str1 = str(RwDtsToyTaskletYang.Company.from_pbcm(pbcm))
      logger.debug("string  (%s)" %str1)
      self.assertEqual(len(str1),len(json_str))

    def test_json_api_manyarrays(self):
      """
      Step: Create Message with many arrays list
      Step: Call Api
      Validate: Valid Json print
      """
      company_entry = RwDtsToyTaskletYang.Company.new()
      company_entry.name = 'Rift'
      for id in range(1, 50):
        emp = company_entry.employee.add()
        emp.id = id
        emp.name = 'jdoe' + str(id)
        emp.title = '978-863-00' + str(id)

      for id in range(1, 20):
        product = company_entry.product.add()
        product.id = id
        product.name = 'nfv-' +str(id)
        product.msrp = '1c-' +str(id)

      logger.debug("dir  (%s)" %dir(RwDts))
      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      logger.debug("string  (%s)" %json_str)

      str1 = str(RwDtsToyTaskletYang.Company.from_pbcm(pbcm))
      logger.debug("string  (%s)" %str1)
      self.assertEqual(len(str1),len(json_str))

    def test_json_api_error(self):
      """
      Step: Create Message with one level 
      Step: Call Api
      Validate: strlen compare shows difference
      """
      company_entry = RwDtsToyTaskletYang.CompanyConfig.new()
      company_entry.name = 'Rift'
      company_entry.description = 'cloud technologies'
      for i in range(1,10):
        company_property = company_entry.property.add()
        company_property.name = 'test-property'+str(i)
        company_property.value = 'test-value'+str(i)

      logger.debug("dir  (%s)" %dir(RwDts))
      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      logger.debug("string  (%s)" %json_str)

      """ 
      Error
      """
      company_property = company_entry.property.add()
      company_property.name = 'test-property'
      company_property.value = 'test-value'

      str1 = str(RwDtsToyTaskletYang.CompanyConfig.from_pbcm(pbcm))
      logger.debug("string  (%s)" %str1)

      self.assertNotEqual(len(str1),len(json_str))

    def test_json_api_emptyfields(self):
      """
      Step: Create Message with empty list
      Step: Call Api
      Validate: Valid Json print
      """
      company_entry = RwDtsToyTaskletYang.Company.new()
      company_entry.name = 'Rift'
      for id in range(1, 2):
        emp = company_entry.employee.add()
        emp.name = 'jdoe' + str(id)
        emp.title = '978-863-00' + str(id)

      logger.debug("dir  (%s)" %dir(RwDts))
      pbcm = company_entry.to_pbcm()
      json_str = RwDts.rwdts_json_from_pbcm(pbcm)
      logger.debug("Jstring  (%s)" %json_str)

      str1 = str(RwDtsToyTaskletYang.Company.from_pbcm(pbcm))
      logger.debug("Pstring  (%s)" %str1)
      self.assertEqual(len(str1),len(json_str))

    def test_json_api_bigdata(self):
      stats = RwDtsToyTaskletYang.StatsData.new()
      stats.payload = "x"*(1024*1024*8*10)
      json_str = RwDts.rwdts_json_from_pbcm(stats.to_pbcm())

    def test_json_api_ctype(self):
      stats = RwDtsToyTaskletYang.StatsData.new()
      stats.srcip = "12.1.2.1"
      json_str = RwDts.rwdts_json_from_pbcm(stats.to_pbcm())
      logger.info("Jstring  (%s)" %json_str)
      str1 = str(RwDtsToyTaskletYang.StatsData.from_pbcm(stats.to_pbcm()))
      logger.info("Pstring  (%s)" %str1)
      self.assertEqual(len(str1),len(json_str))

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--test')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.INFO)
    testname = args.test

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__], defaultTest=testname,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))
if __name__ == '__main__':
    main()
