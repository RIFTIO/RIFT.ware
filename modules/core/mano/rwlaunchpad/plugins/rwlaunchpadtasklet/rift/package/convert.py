
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import json
import os
import tempfile

import gi
gi.require_version('RwNsdYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwYang', '1.0')
from gi.repository import (
        RwNsdYang,
        RwVnfdYang,
        NsdYang,
        VnfdYang,
        RwYang,
        )


class UnknownExtensionError(Exception):
    pass


class SerializationError(Exception):
    pass


def decode(desc_data):
    if isinstance(desc_data, bytes):
        desc_data = desc_data.decode()

    return desc_data


class ProtoMessageSerializer(object):
    """(De)Serializer/deserializer fo a specific protobuf message into various formats"""
    libncx_model = None

    def __init__(self, yang_ns, yang_pb_cls):
        """ Create a serializer for a specific protobuf message """
        self._yang_ns = yang_ns
        self._yang_pb_cls = yang_pb_cls

    @classmethod
    def _deserialize_extension_method_map(cls):
        return {
                ".xml": cls._from_xml_file_hdl,
                ".yml": cls._from_yaml_file_hdl,
                ".yaml": cls._from_yaml_file_hdl,
                ".json": cls._from_json_file_hdl,
                }

    @classmethod
    def _serialize_extension_method_map(cls):
        return {
                ".xml": cls.to_xml_string,
                ".yml": cls.to_yaml_string,
                ".yaml": cls.to_yaml_string,
                ".json": cls.to_json_string,
                }

    @classmethod
    def is_supported_file(cls, filename):
        """Returns whether a file has a supported file extension

        Arguments:
            filename - A descriptor file

        Returns:
            True if file extension is supported, False otherwise

        """
        _, extension = os.path.splitext(filename)
        extension_lc = extension.lower()

        return extension_lc in cls._deserialize_extension_method_map()

    @property
    def yang_namespace(self):
        """ The Protobuf's GI namespace class (e.g. RwVnfdYang) """
        return self._yang_ns

    @property
    def yang_class(self):
        """ The Protobuf's GI class (e.g. RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd) """
        return self._yang_pb_cls

    @property
    def model(self):
        cls = self.__class__

        # Cache the libncx model for the serializer class
        if cls.libncx_model is None:
            cls.libncx_model = RwYang.model_create_libncx()
            cls.libncx_model.load_schema_ypbc(self.yang_namespace.get_schema())

        return cls.libncx_model

    def _from_xml_file_hdl(self, file_hdl):
        xml = file_hdl.read()

        return self.yang_class.from_xml_v2(self.model, decode(xml), strict=False)

    def _from_json_file_hdl(self, file_hdl):
        json = file_hdl.read()

        return self.yang_class.from_json(self.model, decode(json), strict=False)

    def _from_yaml_file_hdl(self, file_hdl):
        yaml = file_hdl.read()

        return self.yang_class.from_yaml(self.model, decode(yaml), strict=False)

    def to_json_string(self, pb_msg):
        """ Serialize a protobuf message into JSON

        Arguments:
            pb_msg - A GI-protobuf object of type provided into constructor

        Returns:
            A JSON string representing the protobuf message

        Raises:
            SerializationError - Message could not be serialized
            TypeError - Incorrect protobuf type provided
        """
        if not isinstance(pb_msg, self._yang_pb_cls):
            raise TypeError("Invalid protobuf message type provided")

        try:
            json_str = pb_msg.to_json(self.model)

        except Exception as e:
            raise SerializationError(e)

        return json_str

    def to_yaml_string(self, pb_msg):
        """ Serialize a protobuf message into YAML

        Arguments:
            pb_msg - A GI-protobuf object of type provided into constructor

        Returns:
            A YAML string representing the protobuf message

        Raises:
            SerializationError - Message could not be serialized
            TypeError - Incorrect protobuf type provided
        """
        if not isinstance(pb_msg, self._yang_pb_cls):
            raise TypeError("Invalid protobuf message type provided")

        try:
            yaml_str = pb_msg.to_yaml(self.model)

        except Exception as e:
            raise SerializationError(e)

        return yaml_str

    def to_xml_string(self, pb_msg):
        """ Serialize a protobuf message into XML

        Arguments:
            pb_msg - A GI-protobuf object of type provided into constructor

        Returns:
            A XML string representing the protobuf message

        Raises:
            SerializationError - Message could not be serialized
            TypeError - Incorrect protobuf type provided
        """
        if not isinstance(pb_msg, self._yang_pb_cls):
            raise TypeError("Invalid protobuf message type provided")

        try:
            xml_str = pb_msg.to_xml_v2(self.model)

        except Exception as e:
            raise SerializationError(e)

        return xml_str

    def from_file_hdl(self, file_hdl, extension):
        """ Returns the deserialized protobuf message from file contents

        This function determines the serialization format based on file extension

        Arguments:
            file_hdl - The file hdl to deserialize (set at pos 0)
            extension - Extension of the file format (second item of os.path.splitext())

        Returns:
            A GI-Proto message of type that was provided into the constructor

        Raises:
            UnknownExtensionError - File extension is not of a known serialization format
            SerializationError - File failed to be deserialized into the protobuf message
        """

        extension_lc = extension.lower()
        extension_map = self._deserialize_extension_method_map()

        if extension_lc not in extension_map:
            raise UnknownExtensionError("Cannot detect message format for %s extension" % extension_lc)

        try:
            msg = extension_map[extension_lc](self, file_hdl)
        except Exception as e:
            raise SerializationError(e)

        return msg

    def to_string(self, pb_msg, extension):
        """ Returns the serialized protobuf message for a particular file extension

        This function determines the serialization format based on file extension

        Arguments:
            pb_msg - A GI-protobuf object of type provided into constructor
            extension - Extension of the file format (second item of os.path.splitext())

        Returns:
            A GI-Proto message of type that was provided into the constructor

        Raises:
            UnknownExtensionError - File extension is not of a known serialization format
            SerializationError - File failed to be deserialized into the protobuf message
        """

        extension_lc = extension.lower()
        extension_map = self._serialize_extension_method_map()

        if extension_lc not in extension_map:
            raise UnknownExtensionError("Cannot detect message format for %s extension" % extension_lc)

        try:
            msg = extension_map[extension_lc](self, pb_msg)
        except Exception as e:
            raise SerializationError(e)

        return msg


class VnfdSerializer(ProtoMessageSerializer):
    """ Creates a serializer for the VNFD descriptor"""
    def __init__(self):
        super().__init__(VnfdYang, VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd)


class NsdSerializer(ProtoMessageSerializer):
    """ Creates a serializer for the NSD descriptor"""
    def __init__(self):
        super().__init__(NsdYang, NsdYang.YangData_Nsd_NsdCatalog_Nsd)


class RwVnfdSerializer(ProtoMessageSerializer):
    """ Creates a serializer for the VNFD descriptor"""
    def __init__(self):
        super().__init__(RwVnfdYang, RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd)


class RwNsdSerializer(ProtoMessageSerializer):
    """ Creates a serializer for the NSD descriptor"""
    def __init__(self):
        super().__init__(RwNsdYang, RwNsdYang.YangData_Nsd_NsdCatalog_Nsd)
