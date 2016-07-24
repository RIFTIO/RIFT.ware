/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

/**
 * Main skyquake module.
 * @module skyquake
 * @author Kiran Kashalkar <kiran.kashalkar@riftio.com>
 */

// Standard library imports for forking
var cluster = require("cluster");
var cpu = require('os').cpus().length;
var clusteredLaunch = process.env.CLUSTER_SUPPORT || false;
var constants = require('./framework/core/api_utils/constants');

var freePorts = [];
for (var i = 0; i < constants.SOCKET_POOL_LENGTH; i++) {
	freePorts[i] = constants.SOCKET_BASE_PORT + i;
};


if (cluster.isMaster && clusteredLaunch) {
    console.log(cpu, 'CPUs found');
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
	    });
    }

    cluster.on('online', function(worker) {
        console.log("Worker Started pid : " + worker.process.pid);
    });
    cluster.on('exit', function(worker, code, signal) {
        console.log('worker ' + worker.process.pid + ' stopped');
    });
} else {
	// Standard library imports
	var argv = require('minimist')(process.argv.slice(2));
	var pid = process.pid;
	var fs = require('fs');
	var https = require('https');
	var http = require('http');
	var express = require('express');
	var session = require('express-session');
	var cors = require('cors');
	var bodyParser = require('body-parser');
	var _ = require('lodash');
	var reload = require('require-reload')(require);

	require('require-json');

	var httpServer = null;
	var secureHttpServer = null;

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
	app.use(bodyParser.json());
	app.use(cors());
	app.use(bodyParser.urlencoded({
		extended: true
	}));

	// Rift framework imports
	var constants = require('./framework/core/api_utils/constants');
	var skyquakeEmitter = require('./framework/core/modules/skyquakeEmitter');
	var navigation_routes = require('./framework/core/modules/routes/navigation');

	/**
	 * Processing when a plugin is added or modified
	 * @param {string} plugin_name - Name of the plugin
	 */
	function onPluginAdded(plugin_name) {
		// Load plugin config
		var plugin_config = reload('./plugins/' + plugin_name + '/config.json');

		// Load all app's views
		app.use('/' + plugin_name, express.static('./plugins/' + plugin_name + '/' + plugin_config.root));

		// Load all app's routes
		app.use('/' + plugin_name, require('./plugins/' + plugin_name + '/routes'));

		// Publish navigation links
		if (plugin_config.routes && _.isArray(plugin_config.routes)) {
			skyquakeEmitter.emit('config_discoverer.navigation_discovered', plugin_name, plugin_config);
		}

	}

	/**
	 * Start listening on a port
	 * @param {string} port - Port to listen on
	 * @param {object} httpServer - httpServer created with http(s).createServer
	 */
	function startListening(port, httpServer) {
		var server = httpServer.listen(port, function () {
			var host = server.address().address;

			var port = server.address().port;

			console.log('Express server listening on port', port);
		});
		return server;
	}

	/**
	 * Initialize skyquake
	 */
	function init() {
		skyquakeEmitter.on('plugin_discoverer.plugin_discovered', onPluginAdded);
		skyquakeEmitter.on('plugin_discoverer.plugin_updated', onPluginAdded);
	}

	/**
	 * Configure skyquake
	 */
	function config() {
		app.use(navigation_routes);
	}

	/**
	 * Run skyquake functionality
	 */
	function run() {

		// Start plugin_discoverer
		var navigation_manager = require('./framework/core/modules/navigation_manager');
		var plugin_discoverer = require('./framework/core/modules/plugin_discoverer');

		// Initialize asynchronous modules
		navigation_manager.init();
		plugin_discoverer.init();

		// Configure asynchronous modules
		navigation_manager.config()
		plugin_discoverer.config({
			plugins_path: './plugins'
		});

		// Run asynchronous modules
		navigation_manager.run();
		plugin_discoverer.run();


		// Server start
		if (httpsConfigured) {
			console.log('HTTPS configured. Will start 2 servers');
			secureHttpServer = https.createServer(sslOptions, app);
			var secureServer = startListening(constants.SECURE_SERVER_PORT, secureHttpServer);
			
			// Add redirection on SERVER_PORT
			httpServer = http.createServer(function(req, res) {
				var host = req.headers['host'];
				host = host.replace(/:\d+$/, ":" + constants.SECURE_SERVER_PORT);

				res.writeHead(301, { "Location": "https://" + host + req.url });
    			res.end();
			});
		} else {
			httpServer = http.createServer(app);
		}

		var server = startListening(constants.SERVER_PORT, httpServer);

		
	}

	init();

	config();

	run();
}
