
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

/**
 * plugin_discoverer module. Static plugin discovery at bootstrap
 * and dynamic plugin discovery during runtime.
 * @module framework/core/modules/plugin_discoverer
 * @author Kiran Kashalkar <kiran.kashalkar@riftio.com>
 */
var path = require('path');
var fs = require('fs');
var util = require('util');
var skyquakeEmitter = require('./skyquakeEmitter');


var plugins_path = '';

/**
 * Module initialization function
 */
function init() {

}

/**
 * Module configuration function
 * @param {Object} config - configuration object
 */
function config(config) {
	plugins_path = config.plugins_path;
}

/**
 * Module run function.
 */
function run() {

	fs.readdir(plugins_path, function(err, filenames) {
		if (!err) {
			filenames.forEach(function(filename) {
				fs.stat(plugins_path + '/' + filename, function(err, stats) {
					if (!err) {
						if (stats.isDirectory()) {
							skyquakeEmitter.emit('plugin_discoverer.plugin_discovered', filename, path.join(plugins_path, path.sep, filename));
						}
					}
				});
			});
		}
	});

	// Watch for modifications and new plugins
	fs.watch(plugins_path, {persistent: true, recursive: true}, function(event, filename) {
		var splitPath = filename.split(path.sep)
		skyquakeEmitter.emit('plugin_discoverer.plugin_updated', splitPath[0], filename);
	});
}


module.exports = {
	init: init,
	config: config,
	run: run
};