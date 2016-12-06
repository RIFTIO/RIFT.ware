#!/usr/bin/node

// Requires the websocket node module , node version >=0.10.32
var WebSocket = require('websocket').w3cwebsocket;
 
//var client = new WebSocket('ws://localhost:8888/ws_streams/NETCONF');
var client = new WebSocket('ws://localhost:8888/ws_streams/NETCONF-JSON');
 
client.onerror = function() {
    console.log('Connection Error');
};
 
client.onopen = function() {
    console.log('WebSocket Client Connected');
};
 
client.onclose = function() {
    console.log('Websocket Client Closed');
};
 
client.onmessage = function(e) {
    console.log("onmessage==");
    if (typeof e.data === 'string') {
        console.log("Received: '" + e.data + "'");
    }
};
