
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
// Helper Functions


/**
 * Utils for use across the api_server.
 * @module framework/core/utils
 */

var fs = require('fs');
var request = require('request');
var Promise = require('promise');

var CONFD_PORT = '8008';


var requestWrapper = function(request) {
	if (process.env.HTTP_PROXY && process.env.HTTP_PROXY != '') {
		request = request.defaults({
			'proxy': process.env.HTTP_PROXY
		});
	}
	return request;
}

var confdPort = function(api_server) {
	try {
		api_server = api_server.replace(api_server.match(/[0-9](:[0-9]+)/)[1],'')
	} catch (e) {

	}
	return api_server + ':' + CONFD_PORT;
};


var validateResponse = function(callerName, error, response, body, resolve, reject) {
	var res = {};

	if (error) {
		console.log('Problem with "', callerName, '": ', error);
		res.statusCode = 500;
		res.errorMessage = {
			error: 'Problem with ' + callerName + ': ' + error
		};
		reject(res);
		return false;
	} else if (response.statusCode >= 400) {
		console.log('Problem with "', callerName, '": ', response.statusCode, ':', body);
		res.statusCode = response.statusCode;

		// auth specific
		if (response.statusCode == 401) {
			res.errorMessage = {
				error: 'Authentication needed' + body
			};
			reject(res);
			return false;
		}

		res.errorMessage = {
			error: 'Problem with ' + callerName + ': ' + response.statusCode + ': ' + body
		};

		reject(res);
		return false;
	} else if (response.statusCode == 204) {
		resolve();
		return false;
	} else {
		return true;
	}
};


var checkAuthorizationHeader = function(req) {
	return new Promise(function(resolve, reject) {
		if (req.get('Authorization') == null) {
			reject();
		} else {
			resolve();
		}
	});
};

if (process.env.LOG_REQUESTS) {
	var logFile = process.env.REQUESTS_LOG_FILE;

	if (logFile && logFile != '') {
		validateResponse = function(callerName, error, response, body, resolve, reject) {
			var res = {};

			if (error) {
				console.log('Problem with "', callerName, '": ', error);
				res.statusCode = 500;
				res.errorMessage = {
					error: 'Problem with ' + callerName + ': ' + error
				};
				reject(res);
				fs.appendFileSync(logFile, 'Request API: ' + response.request.uri.href + ' ; ' + 'Error: ' + error);
				return false;
			} else if (response.statusCode >= 400) {
				console.log('Problem with "', callerName, '": ', response.statusCode, ':', body);
				res.statusCode = response.statusCode;

				// auth specific
				if (response.statusCode == 401) {
					res.errorMessage = {
						error: 'Authentication needed' + body
					};
					reject(res);
					fs.appendFileSync(logFile, 'Request API: ' + response.request.uri.href + ' ; ' + 'Error Body: ' + body);
					return false;
				}

				res.errorMessage = {
					error: 'Problem with ' + callerName + ': ' + response.statusCode + ': ' + body
				};

				reject(res);
				fs.appendFileSync(logFile, 'Request API: ' + response.request.uri.href + ' ; ' + 'Error Body: ' + body);
				return false;
			} else if (response.statusCode == 204) {
				resolve();
				fs.appendFileSync(logFile, 'Request API: ' + response.request.uri.href + ' ; ' + 'Response Body: ' + body);
				return false;
			} else {
				fs.appendFileSync(logFile, 'Request API: ' + response.request.uri.href + ' ; ' + 'Response Body: ' + body);
				return true;
			}
		};
	}
}

/**
 * Serve the error response back back to HTTP requester
 * @param {Object} error - object of the format
 *					{
 *						statusCode - HTTP code to respond back with
 *						error - actual error JSON object to serve
 *					}
 * @param {Function} res - a handle to the express response function
 */
var sendErrorResponse = function(error, res) {
    res.status(error.statusCode);
    if (error.statusCode == 401) {
        res.append('WWW-Authenticate', 'Basic');
    }
    res.send(error);
}

/**
 * Serve the success response back to HTTP requester
 * @param {Object} response - object of the format
 *					{
 *						statusCode - HTTP code to respond back with
 *						data - actual data JSON object to serve
 *					}
 * @param {Function} res - a handle to the express response function
 */
var sendSuccessResponse = function(response, res) {
    res.status(response.statusCode);
    res.send(response.data);
}


module.exports = {
	/**
	 * Ensure confd port is on api_server variable.
	 **/
	confdPort: confdPort,

	validateResponse: validateResponse,

	checkAuthorizationHeader: checkAuthorizationHeader,

	request: requestWrapper.call(null, request),

	sendErrorResponse: sendErrorResponse,

	sendSuccessResponse: sendSuccessResponse
};


