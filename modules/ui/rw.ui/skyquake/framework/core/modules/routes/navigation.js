
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

/**
 * navigation routes module. Provides a RESTful API for this
 * skyquake instance's navigation state.
 * @module framework/core/modules/routes/navigation
 * @author Kiran Kashalkar <kiran.kashalkar@riftio.com>
 */

var cors = require('cors');
var bodyParser = require('body-parser');
var navAPI = require('../api/navigation');
var Router = require('express').Router();
var utils = require('../../api_utils/utils');

Router.use(bodyParser.json());
Router.use(cors());
Router.use(bodyParser.urlencoded({
    extended: true
}));

Router.get('/', cors(), function(req, res, next) {
	res.redirect('/launchpad/?api_server=' + req.protocol + '://' + req.hostname + '&upload_server=' + req.protocol + '://' + req.hostname);
});

Router.get('/nav', cors(), function(req, res) {
	navAPI.get(req).then(function(data) {
		utils.sendSuccessResponse(data, res);
	}, function(error) {
		utils.sendErrorResponse(error, res);
	});
});

Router.get('/nav/:plugin_id', cors(), function(req, res) {
	navAPI.get(req).then(function(data) {
		utils.sendSuccessResponse(data, res);
	}, function(error) {
		utils.sendErrorResponse(error, res);
	});
});

Router.post('/nav/:plugin_id', cors(), function(req, res) {
	navAPI.create(req).then(function(data) {
		utils.sendSuccessResponse(data, res);
	}, function(error) {
		utils.sendErrorResponse(error, res);
	});
});

Router.put('/nav/:plugin_id/:route_id', cors(), function(req, res) {
	navAPI.update(req).then(function(data) {
		utils.sendSuccessResponse(data, res);
	}, function(error) {
		utils.sendErrorResponse(error, res);
	});
});

Router.delete('/nav/:plugin_id/:route_id', cors(), function(req, res) {
	navAPI.delete(req).then(function(data) {
		utils.sendSuccessResponse(data, res);
	}, function(error) {
		utils.sendErrorResponse(error, res);
	});
});


module.exports = Router;
