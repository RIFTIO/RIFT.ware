
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var Webpack = require('webpack');
var WebpackDevServer = require('webpack-dev-server');
var webpackConfig = require('./../webpack.config.js');
var path = require('path');
var fs = require('fs');
var mainPath = path.resolve(__dirname, '..', 'app', 'main.js');

module.exports = function () {
  var bundleStart = null;
  var compiler = Webpack(webpackConfig);

  compiler.plugin('compile', function() {
    console.log('Bundling...');
    bundleStart = Date.now();
  });

  compiler.plugin('done', function() {
    console.log('Bundled in ' + (Date.now() - bundleStart) + 'ms!');
  });

  var bundler = new WebpackDevServer(compiler, {
    contentBase: "http://localhost",
    publicPath: '/build/',

    hot: true,
    progress: true,

    quiet: true,
    noInfo: false,
    stats: {
      colors: true
    }
  });
  bundler.listen(8080, 'localhost', function () {
    console.log('Bundling project, please wait...');
  });

};
