/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var request = require('request');
var Promise = require('promise');
var utils = require('../utils/utils.js');
var constants = require('../common/constants.js');
var _ = require('underscore');

var crashDetails = {};
var debug = {};

crashDetails.get = function(req) {
  var api_server = req.query["api_server"];

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/crash?deep',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false,
      },
      function(error, response, body) {
        var data;
        console.log(error);
        if (utils.validateResponse('crashDetails.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rwshell-mgmt:crash'].list.vm;
          } catch (e) {
            return reject(e);
          }

          return resolve(data);
        }
      });
  });
}
debug["crash-details"] = crashDetails;

module.exports = debug;