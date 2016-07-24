/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var express = require('express');
var path = require('path');
var httpProxy = require('http-proxy');
var bodyParser = require('body-parser');
var cors = require('cors');
var session = require('express-session');
var proxy = httpProxy.createProxyServer();
var app = express();

var isProduction = process.env.NODE_ENV === 'production';
var port = isProduction ? 8080 : 8888;
var publicPath = path.resolve(__dirname, 'public');

if (!isProduction) {

  //Routes for local development
  var lpRoutes = require('./routes.js');

  app.use(express.static(publicPath));
  app.use(session({
    secret: 'ritio rocks',
  }));
  app.use(bodyParser.urlencoded({
      extended: true
  }));
  app.use(bodyParser.json());
  app.use(cors());
  app.use('/', lpRoutes);
  var bundle = require('./server/bundle.js');
  bundle();

  app.all('/build/*', function (req, res) {
    proxy.web(req, res, {
        target: 'http://localhost:8080'
    });
  });

}
proxy.on('error', function(e) {
  console.log('Could not connect to proxy, please try again...');
});

app.listen(port, function () {
  console.log('Server running on port ' + port);
});
