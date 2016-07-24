
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

var about = {};

about.getVCS = function(req) {
  var api_server = req.query["api_server"];

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/vcs/info?deep',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false,
      },
      function(error, response, body) {
        var data;
        console.log(error);
        if (utils.validateResponse('about/vcs.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)["rw-base:info"]
          } catch (e) {
            return reject({});
          }

          return resolve(data);
        }
      });
  });
}

about.getVersion = function(req) {
  var api_server = req.query["api_server"];

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/version?deep',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false,
      },
      function(error, response, body) {
        var data;
        console.log(error);
        if (utils.validateResponse('about/version.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rw-base:version']
          } catch (e) {
            return reject({});
          }

          return resolve(data);
        }
      });
  });
}

about.get = function(req) {

  var api_server = req.query["api_server"];

  return new Promise(function(resolve, reject) {
    Promise.all([
        about.getVCS(req),
        about.getVersion(req)
      ])
      .then(function(results) {
        var aboutObject = {};
        aboutObject.vcs = results[0];
        aboutObject.version = results[1];
        resolve(aboutObject);
      }, function(error) {
        console.log('error getting vcs data', error);
        reject(error)
      });
  });
};

about["about"] = about;

module.exports = about;