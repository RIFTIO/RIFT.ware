/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
// Cloud-Account APIs


var Cloud = {};
var request = require('request');
var Promise = require('promise');
var utils = require('../utils/utils.js');
var constants = require('../common/constants.js');
var _ = require('underscore');

Cloud.get = function(req) {
  var self = this;

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id) {
    // Get all cloud accounts
    return new Promise(function(resolve, reject) {

      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.collection, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/cloud/account',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Cloud.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body).collection['rw-cloud:account']
            } catch (e) {
              console.log('Problem with "Cloud.get"', e);
              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Cloud.get": ' + e // + e.toString()
              }
              return reject(err);
            }
            console.log(data);
            return resolve({
              statusCode: response.statusCode,
              data: self.poolAggregate(data)
            });
          };
        });
    });
  } else {
    //Get a specific cloud account
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.data, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + id,
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Cloud.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body)['rw-cloud:account'];
            } catch (e) {
              console.log('Problem with "Cloud.get"', e);
              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Cloud.get": ' + e.toString()
              }
              return reject(err);
            }

            return resolve({
              statusCode: response.statusCode,
              data: data
            });
          }
        });
    });
  }
};

Cloud.create = function(req) {

  var api_server = req.query["api_server"];
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "account": Array.isArray(data) ? data : [data]
    };

    console.log('Creating with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/cloud',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.create', error, response, body, resolve, reject)) {
        return resolve({
          statusCode: response.statusCode,
          data: JSON.stringify(response.body)
        });
      };
    });
  });
};

Cloud.update = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-cloud:account": data
    };

    console.log('Updating ', id, ' with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/cloud/account/' + id,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.update', error, response, body, resolve, reject)) {
        return resolve({
          statusCode: response.statusCode,
          data: JSON.stringify(response.body)
        });
      };
    });
  });
};

Cloud.delete = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id || !api_server) {
    return new Promise(function(resolve, reject) {
      console.log('Must specifiy api_server and id to delete cloud account');
      return reject({
        statusCode: 500,
        errorMessage: {
          error: 'Must specifiy api_server and id to delete cloud account'
        }
      });
    });
  };

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
      url: utils.confdPort(api_server) + '/api/config/cloud/account/' + id,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.delete', error, response, body, resolve, reject)) {
        return resolve({
          statusCode: response.statusCode,
          data: JSON.stringify(response.body)
        });
      };
    });
  });
};

Cloud.getResources = function(req) {

  var api_server = req.query["api_server"];
  var cloudAccount = req.query["cloud_account"];

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });

    request({
        url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + cloudAccount + '/resources?deep',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;
        if (utils.validateResponse('Cloud.getResources', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rw-cloud:resources']
          } catch (e) {
            console.log('Problem with "Cloud.getResources"', e);

            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "Cloud.getResources": ' + e.toString()
            }

            return reject(err);
          }

          return resolve(data);
        };
      });
  });
};

Cloud.getPools = function(req) {

  var api_server = req.query["api_server"];
  var cloudAccount = req.query["cloud-account"];

  return new Promise(function(resolve, reject) {

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });

    request({
        url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + cloudAccount + '/pools',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;
        if (utils.validateResponse('Cloud.getPools', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rw-cloud:pools']
          } catch (e) {
            console.log('Problem with "Cloud.getPools"', e);
            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "Cloud.getPools": ' + e.toString()
            }

            return reject(err);
          }

          return resolve({
            statusCode: response.statusCode,
            data: data
          });
        }
      });
  });
}

Cloud.poolAggregate = function(cloudAccounts) {
  cloudAccounts.forEach(function(ca) {
    var oldPools = ca.pools;
    var newPools = [];
    for (type in oldPools) {
      oldPools[type].forEach(function(pool) {
        pool.type = type;
        newPools.push(pool);
      })
    }
    ca.pools = newPools;
  });
  return cloudAccounts;
}

module.exports = Cloud;