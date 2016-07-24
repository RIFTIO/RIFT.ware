# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 5/4/16
# 

import lxml.etree as et

def compare_attributes(path, lhs, rhs):
    are_equal = True
    errors = list()

    # compare attributes
    def collect_attributes(attributes):
        # translate the lxml _Attrib class into a set for non-ordered comparison
        collection = set()
        for a in attributes:
            value = attributes.get(a)
            collection.add("%s%s" %(a, value))
        return collection
                
    lhs_attribs = collect_attributes(lhs.attrib)
    rhs_attribs = collect_attributes(rhs.attrib)

    if lhs.attrib != rhs.attrib:
        error_message = "Elements at %s have differing attributes: %s | %s" % (path, lhs.attrib, rhs.attrib)
        are_equal = False
        errors.append(error_message)

    return are_equal, errors

def are_xml_doms_equal(xml_blob1, xml_blob2):
    '''
    Recursively compare xml doms. Elements of a different order are considered
    different, attributes of a different order are not considered different.
    '''
    def walk_and_compare(lhs, rhs, path='/', depth=0):
        errors = list()
        are_equal = True
        errors = list()

        # the doms are expected to be the same so we only need a single xpathy representation
        lhs_tag = et.QName(lhs)
        path += lhs_tag.localname + "/"

        if len(lhs) != len(rhs):
            error_message = "Elements at %s don't have the same number of children: %d | %d" % (path, len(lhs), len(rhs))
            errors.append(error_message)
            return False, errors

        # compare tags
        if lhs.tag != rhs.tag:
            error_message = "Elements at %s are different elements: %s | %s" % (path, lhs.tag, rhs.tag)
            errors.append(error_message)
            are_equal = False
        else:
            if lhs.text != rhs.text:
                error_message = "Elements at %s have differing text: %s | %s" % (path, lhs.text, rhs.text)
                errors.append(error_message)
                are_equal = False
                
            elements_are_equal, new_errors = compare_attributes(path, lhs, rhs)
            if not elements_are_equal:
                are_equal = False
                errors.extend(new_errors)

        if len(lhs) > 0:
            # recurse on containers/lists

            for lhs_child, rhs_child in zip(lhs, rhs):

                # the doms are expected to be the same so we only need a single xpathy representation
                lhs_child_tag = et.QName(lhs_child)
                sub_path = path + lhs_child_tag.localname + "/"
                
                # recurse
                subtrees_are_equal, new_errors = walk_and_compare(lhs_child, rhs_child, path, depth+1)
                if not subtrees_are_equal:
                    errors.extend(new_errors)
                    are_equal = False

        return are_equal, errors


    def setup_dom(dom):
        if isinstance(dom, (str, bytes)):
            return et.fromstring(dom)
        elif isinstance(dom, et._Element):
            return dom
        else:
            raise ValueError("argument is neither an lxml element nor a string")

    root1 = setup_dom(xml_blob1)
    root2 = setup_dom(xml_blob2)

    return walk_and_compare(root1, root2)

if __name__ == "__main__":

    d1 = r'''
<data foo="asdf">
<car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='harmon kadon'>5</speakers>
  </extras>
</car>
<car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='harmon kadon'>5</speakers>
  </extras>
</car>
</data>
'''

    d2 = b'''
<data>
<car foo="asdf" xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-aasdf">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='bose'>5</speakers>
  </extras>
</car>
<car foo="fdsa" xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='harmon kadon'>5</speakers>
  </extras>
</car>
</data>
'''
    d3 = r'''
<data foo='asdf'>
<car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='harmon kadon'>5</speakers>
  </extras>
</car>
</data>
'''

    d4 = b'''
<data>
<car xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-a">
  <brand>subaru</brand>    
  <extras xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">
    <engine xmlns="http://riftio.com/ns/core/mgmt/rwrestconf/test/vehicle-augment-a">turbo</engine>
    <speakers watts='200' brand='harmon kadon'>5</speakers>
  </extras>
</car>
</data>
'''


    equal, errors = are_xml_doms_equal(d3, d4)
    if not equal:
        for e in errors:
            print(e)
