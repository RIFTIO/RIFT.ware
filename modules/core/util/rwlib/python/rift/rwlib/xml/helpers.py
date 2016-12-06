# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
