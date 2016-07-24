
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var cluster = require("cluster");
var cpu = require('os').cpus().length;
var clusteredLaunch = process.env.CLUSTER_SUPPORT || false;
var constants = require('./common/constants.js')

var freePorts = [];
for (var i = 0; i < constants.SOCKET_POOL_LENGTH; i++) {
	freePorts[i] = constants.SOCKET_BASE_PORT + i;
};


if (cluster.isMaster && clusteredLaunch) {
    console.log(cpu, ' cpu\'s found');
    for (var i = 0; i < cpu; i ++) {
		var worker = cluster.fork();

		worker.on('message', function(msg) {
	    	if (msg && msg.getPort) {
				worker.send({
					port: freePorts.shift()
				});
				console.log('freePorts after shift for worker', this.process.pid, ':', freePorts);
			} else if (msg && msg.freePort) {
				freePorts.unshift(msg.port);
				console.log('freePorts after unshift of', msg.port, 'for worker', this.process.pid, ':', freePorts);
			}
	    })
    }

    cluster.on('online', function(worker) {
        console.log("Worker Started pid : " + worker.process.pid);
    });
    cluster.on('exit', function(worker, code, signal) {
        console.log('worker ' + worker.process.pid + ' stopped');
    });


} else {
	var argv = require('minimist')(process.argv.slice(2));
	var pid = process.pid;
	var fs = require('fs');
	var https = require('https');
	var http = require('http');
	var express = require('express');
	var routes = require('./routes.js');
	var session = require('express-session');
	var constants = require('./common/constants.js');
	var Sockets = require('./sockets.js');

	var httpServer = null;

	var httpsConfigured = false;

	var sslOptions = null;

	try {
		if (argv['enable-https']) {
			var keyFilePath = argv['keyfile-path'];
			var certFilePath = argv['certfile-path'];

			sslOptions = {
				key: fs.readFileSync(keyFilePath),
		    	cert: fs.readFileSync(certFilePath)
			};

			httpsConfigured = true;
		}
	} catch (e) {
		console.log('HTTPS enabled but file paths missing/incorrect');
		process.exit(code = -1);
	}

	var app = express();

	app.use(session({
		secret: 'ritio rocks',
		resave: true,
		saveUninitialized: true
	}));

	var socketManager = new Sockets();
	var socketConfig = {
		httpsConfigured: httpsConfigured
	};

	if (httpsConfigured) {
		socketConfig.sslOptions = sslOptions;
	}
	
	socketManager.configure(socketConfig);

	routes(app, socketManager);

	if (httpsConfigured) {
		httpServer = https.createServer(sslOptions, app);
	} else {
		httpServer = http.createServer(app);
	}

	httpServer.listen(constants.NODE_API_PORT, function() {
		console.log('Express server listening on port', constants.NODE_API_PORT, 'HTTPS configured:', httpsConfigured ? 'YES': 'NO');
	});
}
