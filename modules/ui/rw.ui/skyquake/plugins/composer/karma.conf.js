/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

var path = require('path');

module.exports = function (config) {
	config.set({
		basePath: '',
		frameworks: ['jasmine', 'es6-shim'],
		files: [
			'test/spec/**/*.js'
		],
		preprocessors: {
			'test/spec/**/*.js': ['webpack']
		},
		webpack: require('./webpack.config.js'),
		webpackMiddleware: {
			noInfo: true,
			stats: {
				colors: true
			}
		},
		webpackServer: {
			noInfo: true //please don't spam the console when running in karma!
		},
		exclude: [],
		port: 8080,
		logLevel: config.LOG_INFO,
		colors: true,
		autoWatch: true,
		browsers: ['Chrome'],
		reporters: ['dots'],
		captureTimeout: 60000,
		singleRun: false,
		plugins: [
			require('karma-webpack'),
			require('karma-jasmine'),
			require('karma-chrome-launcher'),
			require('karma-phantomjs-launcher'),
			require('karma-es6-shim')
		]
	});
};
