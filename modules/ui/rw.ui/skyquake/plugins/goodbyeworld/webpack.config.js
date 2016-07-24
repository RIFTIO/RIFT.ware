
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
var frameworkPath = '../../../framework';
var HtmlWebpackPlugin = require('html-webpack-plugin');

var config = {
    devtool: 'eval',
    entry:  [
            'webpack/hot/dev-server',
            'webpack-dev-server/client?http://localhost:8080',
            mainPath
        ]
    ,
    output: {
        path: buildPath,
        filename: 'bundle.js',
        publicPath: 'http://localhost:8000/build/'
    },
    resolve: {
        extensions: ['', '.js', '.jsx', '.css', '.scss'],
        root: path.resolve(frameworkPath),
        alias: {
            'widgets': path.resolve(frameworkPath) + '/widgets',
            'style':  path.resolve(frameworkPath) + '/style'
        }
    },
    module: {
        loaders: [{
                test: /\.(jpe?g|png|gif|svg|ttf|otf|eot|svg|woff(2)?)(\?[a-z0-9]+)?$/i,
                loader: "file-loader"
            },
            {
                test: /\.(js|jsx)$/,
                exclude: [nodeModulesPath],
                loader: 'babel-loader',
                query: {
                    presets: ["es2015", "stage-0", "react"]
                },
            },
            {
                test: /\.css$/,
                loader: 'style!css?sourceMap'
            }, {
                test: /\.scss/,
                loader: 'style!css?sourceMap!sass'
            }
        ]
    },
    plugins: [
        new HtmlWebpackPlugin({
            filename: '../index.html',
            templateContent: '<div id="content"></div>'
        }),
        new Webpack.HotModuleReplacementPlugin(),
        new Webpack.optimize.CommonsChunkPlugin("vendor", "vendor.js", Infinity)
    ]
};
module.exports = config;
