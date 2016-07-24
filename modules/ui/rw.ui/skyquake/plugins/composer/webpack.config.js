
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/*
 * Webpack development server configuration
 *
 * This file is set up for serving the webpack-dev-server, which will watch for changes and recompile as required if
 * the subfolder /webpack-dev-server/ is visited. Visiting the root will not automatically reload.
 */
'use strict';
var path = require('path');
var webpack = require('webpack');

module.exports = {

	output: {
		filename: 'main.js',
		publicPath: '/assets/'
	},

	cache: true,
	debug: true,
	devtool: 'sourcemap',
	entry: [
		'webpack/hot/only-dev-server',
		'./src/components/ComposerApp.js'
	],

	stats: {
		colors: true,
		reasons: true
	},

	module: {
		preLoaders: [
			{
				test: /\.(js|jsx)$/,
				exclude: /node_modules/,
				loader: 'eslint-loader'
			}
		],
		loaders: [
			{
				test: /\.(js|jsx)$/,
				exclude: /node_modules/,
				loader: 'react-hot'
			}, {
				test: /\.(js|jsx)$/,
				exclude: /node_modules/,
				loader: 'babel-loader',
				query: {
					presets: ['react', 'es2015']
				}
			}, {
				test: /\.scss/,
				loader: 'style-loader!css-loader!sass-loader?outputStyle=expanded'
			}, {
				test: /\.css$/,
				loader: 'style-loader!css-loader'
			}, {
				test: /\.(jpg|woff|woff2|png)$/,
				loader: 'url-loader'
			}, {
				test: /\.(svg)(\?[a-z0-9]+)?$/i,
				loader: "file-loader"
			}
		]
	},
	resolve: {
		alias: {
			'styles': path.join(process.cwd(), './src/styles/'),
			'libraries': path.join(process.cwd(), './src/libraries/'),
			'components': path.join(process.cwd(), './src/components/'),
			//'stores': '../../../src/stores/',
			//'actions': '../../../src/actions/',
			'helpers': path.join(process.cwd(), './test/helpers/')
		}
	},

	plugins: [
		new webpack.HotModuleReplacementPlugin()
	]

};
