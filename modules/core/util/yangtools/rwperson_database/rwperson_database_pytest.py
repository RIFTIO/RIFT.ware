#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file rwperson_database_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""
import logging
import os
import sys
import unittest
import xmlrunner
import gc
import gi
gi.require_version('RwpersonDbYang', '1.0')
gi.require_version('YangModelPlugin', '1.0')
from gi.repository import RwpersonDbYang, GLib
from gi.repository import RwYang
import rw_peas

logger = logging.getLogger(__name__)

def _load_person_db():
    model = RwYang.Model.create_libncx()
    module = model.load_module("rwperson-db")

    return model, module

def _make_person():
    person = RwpersonDbYang.Person()
    person.name = "test-name"
    person.email ="test@test.com"
    person.employed=True
    person.id=12345
    person.emergency_phone.number = "123-456-7890"
    person.emergency_phone.type_yang = 'WORK'
    new_phone = [ RwpersonDbYang.PhoneNumber(),
                  RwpersonDbYang.PhoneNumber() ]
    new_phone[0].number = '#1'
    new_phone[1].number = '#2'
    person.phone = new_phone

    return person

class TestRwpersonDB(unittest.TestCase):
    def setUp(self):
        self.person = RwpersonDbYang.Person()
        self.flat_person = RwpersonDbYang.FlatPerson()

    def test_basic_person(self):
        self.person.name = "test-name"
        self.assertEqual(self.person.name, "test-name")

        self.person.email = "test@test.com"
        self.assertEqual(self.person.email, "test@test.com")

        self.person.employed=True
        self.person.id=12345

        self.person.blob=bytearray([0x1, 0x2, 0x3, 0x0, 0x4])
        self.assertEqual(self.person.blob, bytearray([0x1, 0x2, 0x3, 0x0, 0x4]))
        self.person.blob=bytearray([])
        self.assertEqual(self.person.blob, bytearray([]))

    def test_basic_person_pbcm_methods(self):
        self.test_basic_person()
        self.pbcm = self.person.to_pbcm()
        # Ensure that the the to_pbcm is returning the right type
        self.assertIsInstance(self.pbcm, gi.repository.ProtobufC.Message)
        self.person_copy = RwpersonDbYang.Person.from_pbcm(self.pbcm)
        self.assertEqual(self.person.name, self.person_copy.name)
        self.assertEqual(self.person.email, self.person_copy.email)
        self.assertEqual(self.person.employed, self.person_copy.employed)
        self.assertEqual(self.person.id, self.person_copy.id)

    def test_containter_in_person(self):
        emergency_phone = self.person.emergency_phone

        # Check that emergency_phone points to phone's memory
        emergency_phone.number = "123-456-7890"
        self.assertEqual(emergency_phone.number, '123-456-7890')
        self.assertEqual(self.person.emergency_phone.number, '123-456-7890')

    def test_new_container_assignment_in_person(self):
        new_emergency_phone = RwpersonDbYang.PhoneNumber1();
        new_emergency_phone.number = '098-765-4321'
        self.assertEqual(new_emergency_phone.number, '098-765-4321')
        # Check that person's emergency phone is updated
        self.person.emergency_phone = new_emergency_phone
        self.assertEqual(self.person.emergency_phone.number, '098-765-4321')

    def test_invalid_enum_in_person(self):
        self.person.emergency_phone.type_yang = 'WORK'
        self.assertEqual(self.person.emergency_phone.type_yang, 'WORK')

        try:
          self.person.emergency_phone.type_yang = 'WORK_INVALID' 
        except GLib.GError:
          pass
        except e:
          self.fail('Unexpected exception thrown:', e)
        else:
          self.fail('ExpectedException not thrown')

    def test_zombie_child(self):
        person_fav = RwpersonDbYang.Person_Favourite()
        person_fav.numbers = [100, 101]
        person_fav.colors = ('red', 'green')
        person_fav.places = ['bangalore', 'boston']
        self.person.favourite = person_fav
        person_fav_new = RwpersonDbYang.Person_Favourite()
        person_fav_new.numbers = [200, 202]
        person_fav_new.colors = ('red', 'purple')
        person_fav_new.places = ['LA', 'boston']
        self.person.favourite = person_fav_new;
        #person_fav should become zombie now.
        try:
          print(person_fav)
        except GLib.GError as e:
          print(e)
          pass
        except e:
          self.fail('Unexpected exception thrown:', e)
        else:
          self.fail('ExpectedException not thrown')

    def test_list_in_person(self):
        self.assertNotEqual(self.person.phone, None)
        self.assertEqual(len(self.person.phone), 0)
        new_phone = [ RwpersonDbYang.PhoneNumber(),
                      RwpersonDbYang.PhoneNumber() ]

        new_phone[0].number = '#1'
        new_phone[1].number = '#2'
        self.person.phone = new_phone
        self.assertEqual(self.person.phone[0], new_phone[0])
        self.assertEqual(self.person.phone[1], new_phone[1])

    def test_primitive_list_in_person(self):
        self.test_basic_person()
        favourite = self.person.favourite
        favourite.numbers = [31, 14]
        favourite.colors = ['green', 'purple']
        favourite.places = ['bangalore', 'boston']
        self.assertEqual(self.person.favourite.numbers[0], 31)
        self.assertEqual(self.person.favourite.numbers[1], 14)
        self.assertEqual(self.person.favourite.colors[0], 'green')
        self.assertEqual(self.person.favourite.colors[1], 'purple')
        self.assertEqual(self.person.favourite.places[0], 'bangalore')
        self.assertEqual(self.person.favourite.places[1], 'boston')
        favourite.places = ['bangalore', 'boston', 'burlington']
        print("Person           : ", self.person)
        favourite.places = []

    def test_empty_leaf_in_person(self):
        # test the empty leaf.
        self.assertFalse(self.person.nri)
        self.person.nri = True
        self.assertTrue(self.person.nri)
        self.person.nri = False
        self.assertFalse(self.person.nri)

    def test_copy_from_person(self):
        person1 = RwpersonDbYang.Person()
        person1.name = "Alice"
        person1.email = "alice@wonderland.uk"
        person1.employed=True
        person1.id=12345
        person1.blob=bytearray([0x1, 0x2, 0x3, 0x0, 0x4])
        person1.emergency_phone.number = "123-456-7890"
        person1.phone.add()
        person1.phone[0].number = '100'
        person1.favourite.numbers = [10, 8]
        person1.favourite.colors = ['blue', 'purple']
        person1.favourite.places = ['LA', 'MA']
        self.person.copy_from(person1)
        self.assertEqual(self.person.name, "Alice")
        self.assertEqual(self.person.email, "alice@wonderland.uk")
        self.assertTrue(self.person.employed)
        self.assertEqual(self.person.id, 12345)
        self.assertEqual(self.person.blob, bytearray([0x1, 0x2, 0x3, 0x0, 0x4]))
        self.assertEqual(self.person.emergency_phone.number, "123-456-7890")
        self.assertEqual(self.person.phone[0].number, '100')
        self.assertEqual(self.person.favourite.numbers[0], 10)
        self.assertEqual(self.person.favourite.numbers[1], 8)
        self.assertEqual(self.person.favourite.colors[0], 'blue')
        self.assertEqual(self.person.favourite.colors[1], 'purple')
        self.assertEqual(self.person.favourite.places[0], 'LA')
        self.assertEqual(self.person.favourite.places[1], 'MA')

    def test_has_field_in_person(self): 
        self.test_basic_person()
        self.assertTrue(self.person.has_field('name'))
        self.assertTrue(self.person.has_field('email'))
        self.assertTrue(self.person.has_field('employed'))
        self.assertTrue(self.person.has_field('id'))
        self.assertFalse(self.person.has_field('phone'))
        self.assertFalse(self.person.has_field('emergency_phone'))
        self.assertFalse(self.person.has_field('favourite'))
        self.assertFalse(self.person.has_field('nri'))
        self.assertFalse(self.person.has_field('secret'))
        self.person.emergency_phone.number = "123-456-7890"
        self.assertTrue(self.person.has_field('emergency_phone'))
        self.person.phone.add()
        self.person.phone[0].number = '100'
        self.assertTrue(self.person.has_field('phone'))
        self.person.favourite.numbers = [10, 8]
        self.assertTrue(self.person.has_field('favourite'))

    def test_ctype_in_person(self):
        self.test_basic_person()
        self.person.laptop_ip = "10.1.0.36"
        self.assertEqual(self.person.laptop_ip, "10.1.0.36")
        self.person.laptop_mac = "AA:AA:AA:AA:AA:AA"
        self.assertEqual(self.person.laptop_mac, "aa:aa:aa:aa:aa:aa")
        ipv4_list = [ "192.168.1.10", "192.168.1.11" ]
        self.person.other_ip4 = ipv4_list
        self.assertEqual(self.person.other_ip4[0], "192.168.1.10");
        self.assertEqual(self.person.other_ip4[1], "192.168.1.11");
        self.person.other_ip4 = []
        self.assertEqual(len(self.person.other_ip4), 0)
        try:
           self.person.laptop_ip = "10.0.1.300"
        except GLib.GError as e:
           print(e)
        except e:
           self.fail('Unexpected exception thrown:', e)
        else:
           self.fail('Expected exception not thrown')

    def test_basic_flat_person(self):
        self.flat_person.name = "flat-test-name"
        self.assertEqual(self.flat_person.name, "flat-test-name")
        self.flat_person.blob=bytearray(b"flat-test-blob")
        self.assertEqual(self.flat_person.blob, bytearray(b"flat-test-blob"))

        self.flat_person.blob=bytearray([0x1, 0x2, 0x3, 0x0, 0x4])
        self.assertEqual(self.flat_person.blob, bytearray([0x1, 0x2, 0x3, 0x0, 0x4]))
        self.flat_person.blob=bytearray([])
        self.assertEqual(self.flat_person.blob, bytearray([]))
        
    def test_containter_in_flat_person(self):
        emergency_phone_inline = self.flat_person.emergency_phone

        ## Check that emergency_phone_inline points to phone's memory
        emergency_phone_inline.number = "123-456-7890"
        self.assertEqual(emergency_phone_inline.number, '123-456-7890')
        self.assertEqual(self.flat_person.emergency_phone.number, '123-456-7890')

    def test_new_container_assignment_in_flat_person(self):
        emergency_phone_inline = self.flat_person.emergency_phone
        new_emergency_phone_inline = RwpersonDbYang.FlatPhoneNumber1()
        new_emergency_phone_inline.number = '098-765-4321'
        self.assertEqual(new_emergency_phone_inline.number, '098-765-4321')

        # Check that person's emergency phone inline is updated
        self.flat_person.emergency_phone = new_emergency_phone_inline
        self.assertEqual(self.flat_person.emergency_phone.number, '098-765-4321')

        ## Check that emergency_phone_inline no longer works
        try:
          emergency_phone_inline.number
        except GLib.GError:
          pass
        except e:
          self.fail('Unexpected exception thrown:', e)
        else:
          self.fail('ExpectedException not thrown')
 
        # Check that new_emergency_phone now points to phone's memory
        new_emergency_phone_inline.number = '123-456-7890'
        self.assertEqual(self.flat_person.emergency_phone.number, '123-456-7890')

    def test_list_in_flat_person(self):
        self.assertNotEqual(self.flat_person.phone, None)
        self.assertEqual(len(self.flat_person.phone), 0)

        new_phone_inline = [ RwpersonDbYang.FlatPhoneNumber(),
                             RwpersonDbYang.FlatPhoneNumber() ]
        new_phone_inline[0].number = '#1'
        new_phone_inline[1].number = '#2'
        self.flat_person.phone = new_phone_inline
        self.assertEqual(self.flat_person.phone[0], new_phone_inline[0])
        self.assertEqual(self.flat_person.phone[1], new_phone_inline[1])

    def test_primitive_list_in_flat_person(self):
        favourite = self.flat_person.favourite
        favourite.numbers = [31, 14]
        favourite.colors = ['green', 'purple']
        favourite.places = ['bangalore', 'boston']
        self.assertEqual(self.flat_person.favourite.numbers[0], 31)
        self.assertEqual(self.flat_person.favourite.numbers[1], 14)
        self.assertEqual(self.flat_person.favourite.colors[0], 'green')
        self.assertEqual(self.flat_person.favourite.colors[1], 'purple')
        self.assertEqual(self.flat_person.favourite.places[0], 'bangalore')
        self.assertEqual(self.flat_person.favourite.places[1], 'boston')
        print("Person           : ", self.flat_person)

    def test_ctype_in_flat_person(self):
        self.flat_person.laptop_ip = "10.1.0.36"
        self.assertEqual(self.flat_person.laptop_ip, "10.1.0.36")
        self.flat_person.laptop_mac = "AA:AA:AA:AA:AA:AA"
        self.assertEqual(self.flat_person.laptop_mac, "aa:aa:aa:aa:aa:aa")
        ipv4_list = [ "192.168.1.10", "192.168.1.11" ]
        self.flat_person.other_ip4 = ipv4_list
        self.assertEqual(self.flat_person.other_ip4[0], "192.168.1.10");
        self.assertEqual(self.flat_person.other_ip4[1], "192.168.1.11");
        self.flat_person.other_ip4 = []
        self.assertEqual(len(self.flat_person.other_ip4), 0)

    def test_person_protobuf_conversion(self):
        self.person.name = "test-name"
        self.person.email ="test@test.com"
        self.person.employed=True
        self.person.id=12345
        self.person.emergency_phone.number = "123-456-7890"
        self.person.emergency_phone.type_yang = 'WORK'
        new_phone = [ RwpersonDbYang.PhoneNumber(),
                      RwpersonDbYang.PhoneNumber() ]
        new_phone[0].number = '#1'
        new_phone[1].number = '#2'
        self.person.phone = new_phone

        # recreate a person using to_pbuf and from_pbuf methods
        pbuf = self.person.to_pbuf()
        recreated_person = RwpersonDbYang.Person()
        recreated_person.from_pbuf(pbuf)
        print("Person           : ", self.person)
        print("Recreated Person : ", recreated_person)

        # check the newly created person is identical
        self.assertEqual(self.person.name, recreated_person.name)
        self.assertEqual(self.person.email, recreated_person.email)
        self.assertEqual(self.person.employed, recreated_person.employed)
        self.assertEqual(self.person.id, recreated_person.id)
        self.assertEqual(self.person.emergency_phone.number, 
                         recreated_person.emergency_phone.number)
        self.assertEqual(self.person.emergency_phone.type_yang, 
                         recreated_person.emergency_phone.type_yang)
        self.assertEqual(len(self.person.phone), len(recreated_person.phone))
        for phone, recreated_phone in zip(self.person.phone, recreated_person.phone):
          self.assertEqual(phone.number, recreated_phone.number)
          self.assertEqual(phone.type_yang, recreated_phone.type_yang)

        ##
        # Need a testcase for this.
        ##
        ptr = recreated_person.to_ptr();

    def test_flat_person_protobuf_conversion(self):
        self.flat_person.name = "test-name"
        self.flat_person.email ="test@test.com"
        self.flat_person.employed=True
        self.flat_person.id=12345
        self.flat_person.emergency_phone.number = "123-456-7890"
        self.flat_person.emergency_phone.type_yang = 'WORK'
        new_phone = [ RwpersonDbYang.FlatPhoneNumber(),
                      RwpersonDbYang.FlatPhoneNumber() ]
        new_phone[0].number = '#1'
        new_phone[1].number = '#2'
        self.flat_person.phone = new_phone
        
        # recreate a flat person using to_pbuf and from_pbuf methods
        flat_pbuf = self.flat_person.to_pbuf()
        recreated_flat_person = RwpersonDbYang.FlatPerson()
        recreated_flat_person.from_pbuf(flat_pbuf)
        print("Flat Person           : ", self.flat_person)
        print("Recreated Flat Person : ", recreated_flat_person)

        # check the newly created flat person is identical
        self.assertEqual(self.flat_person.name, recreated_flat_person.name)
        self.assertEqual(self.flat_person.email, recreated_flat_person.email)
        self.assertEqual(self.flat_person.employed, recreated_flat_person.employed)
        self.assertEqual(self.flat_person.id, recreated_flat_person.id)
        self.assertEqual(self.flat_person.emergency_phone.number, 
                         recreated_flat_person.emergency_phone.number)
        self.assertEqual(self.flat_person.emergency_phone.type_yang, 
                        recreated_flat_person.emergency_phone.type_yang)
        self.assertEqual(len(self.flat_person.phone), len(recreated_flat_person.phone))
        for phone, recreated_phone in zip(self.flat_person.phone, recreated_flat_person.phone):
          self.assertEqual(phone.number, recreated_phone.number)
          self.assertEqual(phone.type_yang, recreated_phone.type_yang)

    def test_person_xml_conversion(self):
        # Open rwperson-database yang model      
        yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
        yang_model = yang.get_interface('Model')
        yang_module = yang.get_interface('Module')
        yang_node = yang.get_interface('Node')
        yang_key = yang.get_interface('Key')
        model = yang_model.alloc()
        module = yang_model.load_module(model, "rwperson-db")

        #populate the person
        self.person.name = "test-name"
        self.person.email ="test@test.com"
        self.person.employed=True
        self.person.id=12345
        self.person.emergency_phone.number = "123-456-7890"
        self.person.emergency_phone.type_yang = 'WORK'
        new_phone = [ RwpersonDbYang.PhoneNumber(),
                      RwpersonDbYang.PhoneNumber() ]
        new_phone[0].number = '#1'
        new_phone[1].number = '#2'
        self.person.phone = new_phone

        # recreate a person using to_xml and from_xml methods
        xml_str = self.person.to_xml(model)
        recreated_person = RwpersonDbYang.Person()
        recreated_person.from_xml(model, xml_str)
        print("Person           : ", self.person)
        print("Recreated Person : ", recreated_person)

        # check the newly created person is identical
        self.assertEqual(self.person.name, recreated_person.name)
        self.assertEqual(self.person.email, recreated_person.email)
        self.assertEqual(self.person.employed, recreated_person.employed)
        self.assertEqual(self.person.id, recreated_person.id)
        self.assertEqual(self.person.emergency_phone.number, 
                         recreated_person.emergency_phone.number)
        self.assertEqual(self.person.emergency_phone.type_yang, 
                         recreated_person.emergency_phone.type_yang)
        self.assertEqual(len(self.person.phone), len(recreated_person.phone))
        for phone, recreated_phone in zip(self.person.phone, recreated_person.phone):
          self.assertEqual(phone.number, recreated_phone.number)
          self.assertEqual(phone.type_yang, recreated_phone.type_yang)

        person = None
        recreated_person = None
        new_phone = None

    def test_flat_person_xml_conversion(self):
        # Open rwperson-database yang model      
        yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
        yang_model = yang.get_interface('Model')
        yang_module = yang.get_interface('Module')
        yang_node = yang.get_interface('Node')
        yang_key = yang.get_interface('Key')
        model = yang_model.alloc()
        module = yang_model.load_module(model, "rwperson-db")

        #populate the person
        self.flat_person.name = "test-name"
        self.flat_person.email ="test@test.com"
        self.flat_person.employed=True
        self.flat_person.id=12345
        self.flat_person.emergency_phone.number = "123-456-7890"
        self.flat_person.emergency_phone.type_yang = 'WORK'
        new_flat_phone = [ RwpersonDbYang.FlatPhoneNumber(),
                      RwpersonDbYang.FlatPhoneNumber() ]
        new_flat_phone[0].number = '#1'
        new_flat_phone[1].number = '#2'
        self.flat_person.phone = new_flat_phone

        # recreate a person using to_xml and from_xml methods
        xml_str = self.flat_person.to_xml(model)
        recreated_flat_person = RwpersonDbYang.FlatPerson()
        recreated_flat_person.from_xml(model, xml_str)
        print("Person           : ", self.flat_person)
        print("Recreated Person : ", recreated_flat_person)

        # check the newly created person is identical
        self.assertEqual(self.flat_person.name, recreated_flat_person.name)
        self.assertEqual(self.flat_person.email, recreated_flat_person.email)
        self.assertEqual(self.flat_person.employed, recreated_flat_person.employed)
        self.assertEqual(self.flat_person.id, recreated_flat_person.id)
        self.assertEqual(self.flat_person.emergency_phone.number, 
                         recreated_flat_person.emergency_phone.number)
        self.assertEqual(self.flat_person.emergency_phone.type_yang, 
                         recreated_flat_person.emergency_phone.type_yang)
        self.assertEqual(len(self.flat_person.phone), len(recreated_flat_person.phone))
        for phone, recreated_phone in zip(self.flat_person.phone, recreated_flat_person.phone):
          self.assertEqual(phone.number, recreated_phone.number)
          self.assertEqual(phone.type_yang, recreated_phone.type_yang)

        flat_person = None
        recreated_flat_person = None
        new_flat_phone = None

    def test_module_root(self):
        module_data_root = RwpersonDbYang.YangData_RwpersonDb()
        module_data_root.person_list.add()
        module_data_root.person_list[0].name = "Alice"
        module_data_root.person_list[0].person_info.aadhar_id = "12345678"
        module_data_root.person_list.add()
        module_data_root.person_list[1].name = "Calvin"
        module_data_root.person_list[1].person_info.aadhar_id = "45677888"
        model = RwYang.Model.create_libncx()
        module = model.load_module("rwperson-db")
        xml_data = module_data_root.to_xml_v2(model, 1)
        print(xml_data)
        recreated_mdr = RwpersonDbYang.YangData_RwpersonDb()
        recreated_mdr.from_xml_v2(model, xml_data)
        print(recreated_mdr)
        person_list = RwpersonDbYang.PersonList()
        try:
          person_list.from_xml_v2(model, xml_data)
        except GLib.GError as e:
          print(e)
          pass
        except e:
          self.fail('Unexpected exception thrown:', e)
        else:
          self.fail('ExpectedException not thrown')

    def test_pb_xml_rpc(self):
        model = RwYang.Model.create_libncx()
        module = model.load_module("rwperson-db")
        rpci = RwpersonDbYang.DuplicateDBI()
        rpci.target.name = "abc"
        rpci.target.details.location = "192.168.2.1:9090/root/tmp"
        rpci.target.details.timeout = 5000
        xml_str=rpci.to_xml_v2(model)
        print(xml_str)
        rpco = RwpersonDbYang.DuplicateDBO() 
        rpco.target.name = "backup1"
        rpco.target.result.timetaken = 1000
        xml_str=rpco.to_xml_v2(model)
        print(xml_str)

    def test_nullable(self):
        person = RwpersonDbYang.Person()
        person.name = "jeff"
        person.emergency_phone.type_yang = 'WORK'
        person.blob=bytearray([0x1, 0x2, 0x3, 0x0, 0x4])

        self.assertTrue(person.has_field('name'))
        try:
            person.name = None
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            pass
        self.assertFalse(person.has_field('name'))

        self.assertTrue(person.emergency_phone.has_field('type'))
        try:
            person.emergency_phone.type_yang = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(person.emergency_phone.has_field('type'))

        self.assertTrue(person.has_field('emergency_phone'))
        try:
            person.emergency_phone = None
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            pass
        self.assertFalse(person.has_field('emergency_phone'))

        self.assertTrue(person.has_field('blob'))
        try:
            person.blob = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(person.has_field('blob'))



        flat_person = RwpersonDbYang.FlatPerson()
        flat_person.name = "jeff"
        flat_person.emergency_phone.type_yang = 'WORK'
        flat_person.blob=bytearray([0x1, 0x2, 0x3, 0x0, 0x4])

        self.assertTrue(flat_person.has_field('name'))
        try:
            flat_person.name = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(flat_person.has_field('name'))

        self.assertTrue(flat_person.emergency_phone.has_field('type'))
        try:
            flat_person.emergency_phone.type_yang = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(flat_person.emergency_phone.has_field('type'))

        self.assertTrue(flat_person.has_field('emergency_phone'))
        try:
            flat_person.emergency_phone = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(flat_person.has_field('emergency_phone'))

        self.assertTrue(flat_person.has_field('blob'))
        try:
            flat_person.blob = None
        except TypeError:
            pass
        except Exception as e:
            self.fail('Unexpected exception thrown: ' + str(e))
        else:
            self.fail('ExpectedException not thrown')
        self.assertTrue(flat_person.has_field('blob'))


class TestPersonDict(unittest.TestCase):
    def test_simple_from_dict(self):
        person = RwpersonDbYang.Person.from_dict({"name":"elvis"})
        self.assertTrue(person.has_field("name"))
        self.assertEquals(person.name, "elvis")
        self.assertTrue("name" in person)

    def test_simple_to_dict(self):
        name_dict = {"name":"elvis"}
        person = RwpersonDbYang.Person.from_dict(name_dict)
        self.assertEqual(dict(person), name_dict)

    def test_unknown_field(self):
        with self.assertRaises(ValueError):
            RwpersonDbYang.Person.from_dict({"notaname":"elvis"})

    def test_with_list(self):
        person_dict = {
                "name": "test-name",
                "phone": [
                    {"number": "#1"},
                    {"number": "#2"},
                    ],
                "favourite":{
                    "colors": ['red', 'green']
                    }
                }
        person = RwpersonDbYang.Person.from_dict(person_dict)
        self.assertTrue("phone" in person)
        self.assertEquals(2, len(person.phone))
        self.assertEquals(person_dict, dict(person))

    def test_bad_list(self):
        person_dict = {
                "name": "test-name",
                "phone": ["#1", "#2"],
                }
        with self.assertRaises(TypeError):
            RwpersonDbYang.Person.from_dict(person_dict)

    def test_multi_level_from_dict(self):
        person_dict = {
                "name": "test-name",
                "email": "test@test.com",
                "employed": True,
                "id": 12345,
                "emergency_phone":{
                    "number": "123-456-7890",
                    "type_yang": 'WORK',
                    }
                }
        person = RwpersonDbYang.Person.from_dict(person_dict)
        self.assertEqual(dict(person), person_dict)
        self.assertTrue("type_yang" in person.emergency_phone)


class PersonPhoneList(unittest.TestCase):
    def setUp(self, cls=RwpersonDbYang, phone_cls_name="PhoneNumber", phone_attr="phone"):
        self.cls = cls
        self.phone_cls_name = phone_cls_name
        self.phone_attr = phone_attr

        self.person = cls.Person()
        self.length = 0
        self.cur_num = 0
        self.num_list = []

    def _set_list(self, new_list):
        setattr(self.person, self.phone_attr, new_list)

    def _get_list(self):
        return getattr(self.person, self.phone_attr)

    def _get_phone(self, index):
        phone = self._get_list()[index]
        self.assertIsNotNone(phone)
        return phone

    def _insert_phone(self, lst, index, phone):
        lst.insert(index, phone)

    def _add_phones(self, num=1, index=None):
        assert num > 0
        new_list = self._get_list()

        if index is None:
            index = self.length

        for i in range(num):
            new_phone = getattr(self.cls, self.phone_cls_name)()
            new_phone.number = str(self.cur_num)

            self._insert_phone(new_list, index, new_phone)
            self.num_list.insert(index, self.cur_num)

            self.length += 1
            self.cur_num += 1

            # Step up the index so we add phones in order
            index += 1

        self._set_list(new_list)

    def _delete_phones(self, num=1, index=None):
        if index is None:
            index = -1
        else:
            self.assertGreaterEqual(index, 0)
            self.assertLess(index, self.length)

        self.assertLessEqual(num, self.length)

        new_list = self._get_list()

        invalidate_list = []
        for i in range(num):
            obj = new_list.pop(index)
            invalidate_list.append(obj)

            self.num_list.pop(index)

            self.length -= 1

        self._set_list(new_list)

        def access_invalidated_object(index):
            invalidate_list[index].number

        # All the deleted objects should now throw exceptions when we
        # try to access them since they are now stale.
        for index in range(len(invalidate_list)):
            self.assertRaises(GLib.GError, access_invalidated_object, index)

    def _assert_phone_numbers(self, num_list=None):
        if num_list is None:
            num_list = self.num_list

        new_list = self._get_list()
        for i, phone in enumerate(new_list):
            self.assertEqual(phone.number, str(num_list[i]))

    def _assert_phone_len(self):
        cur_list = self._get_list()
        self.assertEqual(len(cur_list), self.length)

    def _add_phones_and_verify(self, num, start_idx=None):
        self._add_phones(num, start_idx)
        self._assert_phone_len()
        self._assert_phone_numbers()

    def _delete_phones_and_verify(self, num, index=None):
        self._delete_phones(num, index)
        self._assert_phone_len()
        self._assert_phone_numbers()

    def test_add_to_end(self):
        self._add_phones_and_verify(10)
        self._add_phones_and_verify(10)

    def test_add_to_begining(self):
        self._add_phones_and_verify(10)
        self._add_phones_and_verify(10, 0)
        self._assert_phone_numbers(list(range(10, 20)) + list(range(0, 10)))

    def test_add_to_middle(self):
        self._add_phones_and_verify(90)
        self._add_phones_and_verify(10, 50)
        self._assert_phone_numbers(list(range(50)) + list(range(90, 100)) + list(range(50, 90)))

    def test_add_delete_from_end(self):
        self._add_phones_and_verify(100)
        self._delete_phones_and_verify(10)
        self._assert_phone_numbers(list(range(90)))

    def test_add_delete_from_beginning(self):
        self._add_phones_and_verify(100)
        self._delete_phones_and_verify(10, 0)
        self._assert_phone_numbers(list(range(10, 100)))

    def test_add_delete_from_middle(self):
        self._add_phones_and_verify(90)
        self._add_phones_and_verify(10, 50)
        self._delete_phones_and_verify(10, 50)
        self._assert_phone_numbers(list(range(90)))

    def test_add_delete_every_other(self):
        self._add_phones_and_verify(4)
        for i in range(0, 2):
            self._delete_phones_and_verify(1, i)

        self._assert_phone_numbers(range(1, 4, 2))


class PersonPhoneListProtoGiGen(PersonPhoneList):
    def setUp(self):
        super(PersonPhoneListProtoGiGen, self).setUp(cls=RwpersonDbYang,
                                                     phone_cls_name="PhoneNumber",
                                                     phone_attr="phone")

    def _insert_phone(self, lst, index, phone):
        lst.insert(index, phone)

class PersonPhoneInlineListProtoGiGen(PersonPhoneList):
    def setUp(self):
        super(PersonPhoneInlineListProtoGiGen, self).setUp(cls=RwpersonDbYang,
                                                           phone_cls_name="Person_PhoneNumberInline",
                                                           phone_attr="phone_inline")

    def _insert_phone(self, lst, index, phone):
        lst.insert(index, phone)


class TestNonMsgNewPerson(unittest.TestCase):
    """
        This class tests the XML and CLI generation
        functionality for the cases in yang configuration
        where the extension 'rwpb:msg-new' have not been provided.

        In such cases, its useful to provide with api's which
        can help client users to generate XML and CLI from any 
        arbitrary root.
    """
    def setUp(self):
        from gi.repository import RwYang
        self.min_person = RwpersonDbYang.YangData_RwpersonDb_MinimalPerson()
        self.model = RwYang.Model.create_libncx()
        self.module = self.model.load_module("rwperson-db")

        self.min_person.set_name("Foo")
        self.min_person.set_employed(False)
        self.min_person.set_email("foo@neverland.com")
        self.min_person.set_nationality("zombie")

        tmp_addr = self.min_person.create_address()
        tmp_addr.set_city("sine")
        tmp_addr.set_state("xyz")
        tmp_addr.set_street("jump")
        self.min_person.set_address(tmp_addr)

    def compare_min_persons(self, first_min_person, sec_min_person):
        self.assertEqual(first_min_person.get_name(),
                         sec_min_person.get_name())

        self.assertEqual(first_min_person.get_nationality(),
                         sec_min_person.get_nationality())

        self.assertEqual(first_min_person.get_employed(),
                         sec_min_person.get_employed())

        self.assertEqual(first_min_person.get_email(),
                         sec_min_person.get_email())
        return True


    def test_xml_v2(self):
        xml_data = self.min_person.to_xml_v2(self.model)
        print("XML Data: {0}".format(xml_data))

        new_person = RwpersonDbYang.YangData_RwpersonDb_MinimalPerson()
        new_person.from_xml_v2(self.model, xml_data)

        self.compare_min_persons(self.min_person, new_person)

    def test_pb_to_cli(self):
        # ATTN: Do we need to_pb_cli_v2 ?
        # If not planned below model creation should
        # probably be part of selfand initialied in 
        # 'setUp'
        yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
        yang_model = yang.get_interface('Model')
        model = yang_model.alloc()
        module = yang_model.load_module(model, "rwperson-db")

        cli_str = self.min_person.to_pb_cli(model)
        print("CLI command: {0}".format(cli_str))

        self.assertTrue("config" in cli_str)
        self.assertTrue("commit" in cli_str)

    def _assert_people_are_equal(self, first, second):
        # check the newly created person is identical
        self.assertEqual(first.name, second.name)
        self.assertEqual(first.email, second.email)
        self.assertEqual(first.employed, second.employed)
        self.assertEqual(first.id, second.id)
        self.assertEqual(first.emergency_phone.number, 
                         second.emergency_phone.number)
        self.assertEqual(first.emergency_phone.type_yang, 
                         second.emergency_phone.type_yang)
        self.assertEqual(len(first.phone), len(second.phone))

        for phone, recreated_phone in zip(first.phone, second.phone):
          self.assertEqual(phone.number, recreated_phone.number)
          self.assertEqual(phone.type_yang, recreated_phone.type_yang)
        
    def test_person_json_conversion(self):
        if sys.version_info < (3, 0):
            # to_json is python3 only
            return
        model, module = _load_person_db()
        person = _make_person()

        # recreate a person using to_json and from_json methods
        json_str = person.to_json(model)
        recreated_person = RwpersonDbYang.Person()
        recreated_person.from_json(model, json_str)

        self._assert_people_are_equal(person, recreated_person)

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
    gc.collect()
