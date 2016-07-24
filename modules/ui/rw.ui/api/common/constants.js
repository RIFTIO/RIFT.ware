
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
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

constants.SOCKET_BASE_PORT = 3500;
constants.SOCKET_POOL_LENGTH = 20;
constants.NODE_API_PORT = process.env.NODE_API_PORT || 3000;


module.exports = constants;