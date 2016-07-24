/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/*
 * Webpack distribution configuration
 *
 * This file is set up for serving the distribution version. It will be compiled to dist/ by default
 */

'use strict';

var webpack = require('webpack');
var path = require('path');
var uiPluginCmakeBuild = process.env.ui_plugin_cmake_build || false;
var frameworkPath = uiPluginCmakeBuild?'../../../../skyquake/skyquake-build/framework':'../../framework';
var HtmlWebpackPlugin = require('html-webpack-plugin');
// Added to overcome node-sass bug https://github.com/iam4x/isomorphic-flux-boilerplate/issues/62
process.env.UV_THREADPOOL_SIZE=64;
module.exports = {
	devtool: 'source-map',
	output: {
		publicPath: 'assets/',
		path: 'public/assets/',
		filename: 'src/main.js'
	},

	debug: false,
	entry: [path.resolve(__dirname) + '/src/index.js'],

	stats: {
		colors: true,
		reasons: false
	},
	resolve: {
        extensions: ['', '.js', '.jsx', '.css', '.scss'],
        root: path.resolve(frameworkPath),
        alias: {
            'widgets': path.resolve(frameworkPath) + '/widgets',
            'style':  path.resolve(frameworkPath) + '/style',
            'utils':  path.resolve(frameworkPath) + '/utils',
            'styles': path.join(process.cwd(), './src/styles/'),
			'libraries': path.join(process.cwd(), './src/libraries/'),
			'components': path.join(process.cwd(), './src/components/'),
			//'stores': '../../../src/stores/',
			//'actions': '../../../src/actions/',
			'helpers': path.join(process.cwd(), './test/helpers/')
        }
    },
	plugins: [
		// new webpack.optimize.DedupePlugin(),
		// new webpack.optimize.UglifyJsPlugin(),
		// new webpack.optimize.OccurenceOrderPlugin(),
		// new webpack.optimize.AggressiveMergingPlugin(),
		// new webpack.NoErrorsPlugin(),
		new HtmlWebpackPlugin({
            filename: '../index.html'
            , templateContent: '<div id="app"></div>'
        })
	],

	module: {
		noParse: [/autoit.js/],
		// preLoaders: [
		// 	{
		// 		test: /\.(js|jsx)$/,
		// 		exclude: /node_modules/,
		// 		loader: 'eslint-loader'
		// 	}
		// ],
		loaders: [
			{
				test: /\.(js|jsx)$/,
				exclude: /node_modules/,
				loader: 'babel-loader',
				query: {
					presets: ['es2015', 'stage-0', 'react']
				}
			}, {
				test: /\.css$/,
				loader: 'style-loader!css-loader'
			}, {
				test: /\.scss/,
				loader: 'style-loader!css-loader!sass-loader?outputStyle=expanded'
			},{
				test: /\.(jpe?g|png|gif|svg|ttf|otf|eot|svg|woff(2)?)(\?[a-z0-9]+)?$/i,
				loader: "file-loader"
			},
			{ test: /\.json$/, loader: "json-loader" },
		]
	}
};
