/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
// SDN-Account APIs


var Sdn = {};
var request = require('request');
var Promise = require('promise');
var utils = require('../utils/utils.js');
var constants = require('../common/constants.js');
var _ = require('underscore');

//Sdn Account APIs
Sdn.get = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  var self = this;
  if (!id) {
    // Get all sdn accounts
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.collection, {
          'Authorization': req.get('Authorization')
        });
      request({
          url: utils.confdPort(api_server) + '/api/operational/sdn-account?deep',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Sdn.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body).collection['rw-sdn:sdn-account']
            } catch (e) {
              console.log('Problem with "Sdn.get"', e);

              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Sdn.get": ' + e.toString()
              }

              return reject(err);
            }

            return resolve({
              data: data,
              statusCode: response.statusCode
            });
          }
        });
    });
  } else {
    //Get a specific sdn account
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.data, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/sdn-account/' + id + '?deep',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Sdn.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body)['rw-sdn:sdn-account'];
            } catch (e) {
              console.log('Problem with "Sdn.get"', e);

              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Sdn.get": ' + e.toString()
              }

              return reject(err);
            }

            return resolve({
              data: data,
              statusCode: response.statusCode
            });
          }
        });
    });
  }
};

Sdn.create = function(req) {

  var api_server = req.query["api_server"];
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "sdn-account": Array.isArray(data) ? data : [data]
    };

    console.log('Creating SDN account with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/sdn-account',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.create', error, response, body, resolve, reject)) {
        return resolve({
          data: JSON.stringify(response.body),
          statusCode: response.statusCode
        });
      }
    });
  });
};

Sdn.update = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-sdn:sdn-account": data
    };

    console.log('Updating SDN account ', id, ' with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/sdn-account/' + id,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.update', error, response, body, resolve, reject)) {
        return resolve({
          data: JSON.stringify(response.body),
          statusCode: response.statusCode
        });
      }
    });
  });
};

Sdn.delete = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id || !api_server) {
    return new Promise(function(resolve, reject) {
      console.log('Must specifiy api_server and id to delete sdn account');
      reject({
        statusCode: 500,
        errorMessage: {
          error: 'Must specifiy api_server and id to delete sdn account'
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
      url: utils.confdPort(api_server) + '/api/config/sdn-account/' + id,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.delete', error, response, body, resolve, reject)) {
        return resolve({
          data: JSON.stringify(response.body),
          statusCode: response.statusCode
        });
      }
    });
  });
};

module.exports = Sdn