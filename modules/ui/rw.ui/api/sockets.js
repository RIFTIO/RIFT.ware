
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
//SOCKET MANAGER
// test
//Supports localhost node polling subscriptions and pass through subscriptions to other websockets
//
//TODO REFACTOR: this needs to happen. there's too much boilerplate code in here.
//TODO Document after refactoring
//TODO Improved logging for debugging
//TODO List of URLS

var WebSocketServer = require('ws').Server,
  WebSocket = require('ws'),
  Request = require('request');
var _ = require('underscore');
var constants = require('./common/constants.js');
var Promise = require('promise');

var Subscriptions = function(){
  this.ID = 1;
  this.socketServers = {};
  this.freePorts = [];
  for (var i = 0; i < constants.SOCKET_POOL_LENGTH; i++) {
    this.freePorts[i] = constants.SOCKET_BASE_PORT + i;
  };
};

Subscriptions.prototype.configure = function(config) {
  this.config = config;
  this.ready = true;
}

/**
 * [subscribe description]
 * @param  {Object}   req
 * @param  {String}   req.body.url May be http, https, or ws
 * @param  {Function} req.body.transform A function that will transform
 *                                      the data before sending it out
 *                                      through the socket. Receives one
 *                                      argument, which is the data
 *                                      returned from the subscription.
 * @return {Object} SubscriptionReference  An object containing the subscription information.
 * @return {Number} SubscriptionReference.id The subscription ID
 * @return {Number} SubscriptionReference.port The subscription port
 * @return {String} SubscriptionReference.socketPath The path the to socket.
 */
Subscriptions.prototype.subscribe = function(req) {
  var self = this;
  var URL = req.body.url;
  var SubscriptionReference;
  var sessionId = req.session.id;

  return new Promise(function(resolve, reject) {

    if (!self.ready) {
      return reject({
        statusCode: 500,
        errorMessage: 'SocketManager not configured yet. Cannot proceed'
      })
    }

    if (!self.socketServers.hasOwnProperty(sessionId)) {
      self.createWebSocketServer().then(function(successData) {

        self.socketServers[sessionId] = successData;

        //Check for more generix regex
        switch(URL.match(/(.{2,5}):\/\//)[1]) {
          case 'http' : self.socketInstance(req, self.socketServers[sessionId].wss, self.socketServers[sessionId].httpServer, PollingSocket);
            break;
          case 'https' : self.socketInstance(req, self.socketServers[sessionId].wss, self.socketServers[sessionId].httpServer, PollingSocket);
            break;
          case 'ws' : self.socketInstance(req, self.socketServers[sessionId].wss, self.socketServers[sessionId].httpServer, WebSocket);
            break;
          case 'wss' : self.socketInstance(req, self.socketServers[sessionId].wss, self.socketServers[sessionId].httpServer, WebSocket);
            break;
        }

        return resolve({
          statusCode: 200,
          data: {
            id: self.socketServers[sessionId].id,
            port: self.socketServers[sessionId].port,
            socketPath:self.socketServers[sessionId].socketPath,
            sessionId: sessionId
          }
        });
      },
      function(errorData) {
        return reject({
          statusCode: 503,
          errorMessage: errorData.error
        });
      });
    } else {
      SubscriptionReference = {
        id: self.socketServers[sessionId].id,
        port: self.socketServers[sessionId].port,
        socketPath:self.socketServers[sessionId].socketPath,
        sessionId: sessionId
      };

      resolve({
        statusCode: 200,
        data: SubscriptionReference
      });
    }
  });
};

Subscriptions.prototype.createWebSocketServer = function() {
  var self = this;

  return new Promise(function(resolve, reject) {
    if (self.freePorts.length == 0) {
      // TODO: Send back a good error
      return reject({
        error: 'No more free ports available for a socket'
      });
    }
    var port = self.freePorts.shift();
    var socketPath = '/ws/' + self.ID;

    var http = require('http');
    var https = require('https');
    var express = require('express');
    var app = express();

    var httpServer = null;
    var wss = null;

    if (self.config && self.config.httpsConfigured) {
      httpServer = https.createServer(self.config.sslOptions, app);
    } else {
      httpServer = http.createServer(app);
    }

    httpServer.listen(port);

    wss = new WebSocketServer({
      path: socketPath,
      server: httpServer
    });

    self.ID++;

    return resolve({
      id: self.ID,
      port: port,
      socketPath: socketPath,
      wss: wss,
      httpServer: httpServer
    });
  });
};

Subscriptions.prototype.socketInstance = function(req, wss, httpServer, Type) {
  console.log('Creating a new socketInstance for:', req.body.url, 'sessionId:', req.session.id);
    var self = this;
    var Socket = null;
    var Connections = [];
    var Index = 0;
    var sessionId = req.session.id;
    var wssRef = wss;
    var httpServerRef = httpServer;

    wss.on('connection', function(ws) {

      ws.on('message', function(msg) {
        console.log('Received message from client', msg);
      });

      if (!Socket) {
        if (Type == PollingSocket) {
          Socket = new Type(req, 1000, req.body);
        } else {
          Socket = new Type(req.body.url);
        }
      }
      ws.index = Index++;
      Connections.push(ws);

      ws.onclose = function() {
        Connections.splice(ws.index, 1);
        if (Connections.length == 0) {
          console.log('Closing socket, no more connections');
          try {
            Socket.close();
            self.freePorts.unshift(self.socketServers[sessionId].port);
            delete self.socketServers[sessionId];
            wssRef.close();
            httpServer.close();
          } catch (e) {
            console.log('Error: ', e);
          }
          Index = 0;
          delete Socket;
        }
      };

      Socket.onopen = function() {
          console.log('Opened a websocket to southbound server');
      };

      Socket.onerror = function(error) {
        console.log('Error on southbound connection. Error:', error);
      }

      Socket.onmessage = function(data) {
        var i;
        var self = this;
        if (req.body.transform && req.body.transform.constructor.name == "String") {
          //someTransformObject[req.body.transform](data, send)
          //req.body.transform(data, send);
        } else {
          if (Type == PollingSocket) {
            send(data);
          } else {
            send(data.data);  
          }
        }

        function send(payload) {
          var is401 = false;
          try {
            if (typeof payload == 'string') {
              var jsonPayload = JSON.parse(payload);
              is401 = jsonPayload.statusCode == 401;
            }
            else {
              is401 = payload.statusCode == 401;
            }
          } catch(e) {
            payload = {}
          }

          for (i = Connections.length - 1; i >=0; i -= 1) {
              Connections[i].send(payload, function wsError(error) {
                if (error) {
                  console.log('Error sending: ', error);
                  // Clean up sockets
                  console.log('Closing Socket');
                  try {
                    Socket.close();
                  } catch (e) {
                    console.log('Error closing Socket')
                  }
                  Connections.splice(i, 1);
                }
              });
          };
          if(is401) {
              try {
                    Socket.close();
                } catch (e) {
                    console.log('Error closing Socket')
              }
          }
        }

      };
    });
};

function PollingSocket(req, interval, config) {
  console.log('a new PollingSocket has appeared for url', req.body.url, 'sessionId:', req.session.id);
  var self = this;
  self.isClosed = false;
  var requestHeaders = {};
  _.extend(requestHeaders,
      {
        'Authorization': req.get('Authorization')
      }
    );

  var pollServer = function() {
    Request({
      url: req.body.url,
      method: config.method || 'GET',
      headers: requestHeaders,
      json: config.payload,
      rejectUnauthorized: false,
      forever: constants.FOREVER_ON
    }, function(error, response, body) {
      if (error) {
        console.log('Error polling: ' + req.body.url);
      } else {
        if(!self.isClosed) {
          self.poll = setTimeout(pollServer, 1000 || interval);
          var data = response.body;
          if (self.onmessage) {
            self.onmessage(data);
          }
        }
      }
    });
  };
  pollServer();
};

PollingSocket.prototype.close = function() {
  console.log('Closing poll');
  var self = this;
  this.isClosed = true;
  clearTimeout(self.poll);
};


module.exports = Subscriptions;
