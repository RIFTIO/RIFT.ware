/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

 /**
  * constants module. Provides constants for use within the skyquake instance
  * @module api_utils/constants
  */

var constants = {};
constants.DEFAULT_ADMIN_AUTH = {
    'user': 'admin',
    'pass': 'admin',
};
constants.FOREVER_ON = true;
constants.HTTP_HEADERS = {
    accept: {
        data: {
            'Accept': 'application/vnd.yang.data+json'
        },
        collection: {
            'Accept': 'application/vnd.yang.collection+json'
        }
    },
    content_type: {
        data: {
            'Content-Type': 'application/vnd.yang.data+json'
        },
        collection: {
            'Content-Type': 'application/vnd.yang.collection+json'
        }
    }
};

// (Incomplete) set of expected HTTP response codes
constants.HTTP_RESPONSE_CODES = {
    SUCCESS: {
        OK:                         200,
        CREATED:                    201,
        ACCEPTED:                   202,
        NO_CONTENT:                 204,
        MOVED_PERMANENTLY:          301,
        NOT_MODIFIED:               304
    },
    ERROR: {
        BAD_REQUEST:                400,
        UNAUTHORIZED:               401,
        FORBIDDEN:                  403,
        NOT_FOUND:                  404,
        METHOD_NOT_ALLOWED:         405,
        NOT_ACCEPTABLE:             406,
        CONFLICT:                   409,
        INTERNAL_SERVER_ERROR:      500,
        NOT_IMPLEMENTED:            501,
        BAD_GATEWAY:                502,
        SERVICE_UNAVAILABLE:        504,
        HTTP_VERSION_UNSUPPORTED:   505

    }
}
constants.SOCKET_BASE_PORT = 3500;
constants.SOCKET_POOL_LENGTH = 20;
constants.SERVER_PORT = process.env.SERVER_PORT || 8000;
constants.SECURE_SERVER_PORT = process.env.SECURE_SERVER_PORT || 8443;

module.exports = constants;