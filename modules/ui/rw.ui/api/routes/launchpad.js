
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var cors = require('cors');
var fs = require('fs');
// var online = require('../isOnline.js');
var path = require('path');
var launchpadAPI = require('../launchpad/launchpad.js');
var bodyParser = require('body-parser');
// var LP_PORT = '5000'
module.exports = function(app) {

  app.use(bodyParser.json());
  app.use(cors());
  app.use(bodyParser.urlencoded({
    extended: true
  }));

  app.get('/api/launchpad/network-service', cors(), function(req, res) {
    var api_server = req.query["api_server"]// + ':' + LP_PORT;
    launchpadAPI.getNetworkService(api_server).then(function(data) {
      res.send(data);
    });
  });
  app.get('/api/launchpad/pools', cors(), function(req, res) {
    var api_server = req.query["api_server"]// + ':' + LP_PORT;
    launchpadAPI.getPools(api_server).then(function(data) {
      res.send(data);
    });
  });
  app.get('/api/launchpad/sla-params', cors(), function(req, res) {
    var api_server = req.query["api_server"]// + ':' + LP_PORT;
    launchpadAPI.getSLAParams(api_server).then(function(data) {
      res.send(data);
    });
  });
  app.post('/api/launchpad/environment', cors(), function(req, res) {
    var api_server = req.query["api_server"]// + ':' + LP_PORT;
    launchpadAPI.createEnvironment(api_server, req.body).then(function(data) {
      res.send(data);
    });
  });

}
