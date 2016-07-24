# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 11/9/15
# 

def get_prefix_from_namespace(namespace):
    split_namespace = namespace.split("/")

    if len(split_namespace) == 1:
        split_namespace = namespace.split(":")            

    prefix = '%s:' % split_namespace[-1]
    return prefix

def get_xml_element_name(element):
    pieces = element.tag.split("}")
    try:
        return pieces[1]
    except IndexError:
        return pieces[0]
    
def collect_siblings(xml_node):
    grouping = dict()
    children = xml_node.getchildren()

    for child in children:
        tag = get_xml_element_name(child)

        if tag not in grouping:
            grouping[tag] = list()

        grouping[tag].append(child)
    return grouping
