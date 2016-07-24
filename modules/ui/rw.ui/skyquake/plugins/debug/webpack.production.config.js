/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var Webpack = require('webpack');
var path = require('path');
var nodeModulesPath = path.resolve(__dirname, 'node_modules');
var buildPath = path.resolve(__dirname, 'public', 'build');
var mainPath = path.resolve(__dirname, 'src', 'main.js');
var uiPluginCmakeBuild = process.env.ui_plugin_cmake_build || false;
var frameworkPath = uiPluginCmakeBuild?'../../../../skyquake/skyquake-build/framework':'../../framework';
var HtmlWebpackPlugin = require('html-webpack-plugin');
// Added to overcome node-sass bug https://github.com/iam4x/isomorphic-flux-boilerplate/issues/62
process.env.UV_THREADPOOL_SIZE=64;
var config = {
    devtool: 'source-map',
    entry: mainPath,
    output: {
        path: buildPath,
        filename: 'bundle.js',
        publicPath: "build/"
    },
    resolve: {
        extensions: ['', '.js', '.jsx', '.css', '.scss'],
        root: path.resolve(frameworkPath),
        alias: {
            'widgets': path.resolve(frameworkPath) + '/widgets',
            'style':  path.resolve(frameworkPath) + '/style',
            'utils':  path.resolve(frameworkPath) + '/utils'
        }
    },
    module: {
        loaders: [{
                test: /\.(jpe?g|png|gif|svg|ttf|otf|eot|svg|woff(2)?)(\?[a-z0-9]+)?$/i,
                loader: "file-loader"
            },
            {
                test: /\.(js|jsx)$/,
                exclude: /node_modules/,
                loader: 'babel-loader',
                query: {
                    presets: ["es2015", "stage-0", "react"]
                }
            }, {
                test: /\.css$/,
                loader: 'style!css'
            }, {
                test: /\.scss/,
                loader: 'style!css!sass'
            }
        ]
    },
    plugins: [
        new HtmlWebpackPlugin({
            filename: '../index.html'
            , templateContent: '<div id="app"></div>'
        }),
        new Webpack.optimize.CommonsChunkPlugin("vendor", "vendor.js", Infinity)
    ]
};
module.exports = config;
