
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

/*Define dependencies.*/

var express = require('express');
var multer = require('multer');
var cors = require('cors');
var launchpadServer = express();
var apiServer = express();
var done = false;
var path = require('path');

/*Configure the multer.*/

//apiServer.use(function(req, res, next) {
//	res.header('Access-Control-Allow-Origin', '*');
//	res.header('Access-Control-Allow-Headers', '*');
//	next();
//});

var uploadPayload = {
	"events": [
		{
			"timestamp": 1445452293.688965,
			"value": "onboard-started",
			"text": "onboarding process started"
		},
		{
			"timestamp": 1445452293.7836523,
			"value": "onboard-pkg-upload",
			"text": "uploading package"
		},
		{
			"timestamp": 1445452293.8315961,
			"value": "onboard-img-upload",
			"text": "uploading image"
		},
		{
			"timestamp": 1445452293.8316283,
			"value": "onboard-dsc-validation",
			"text": "descriptor validation"
		},
		{
			"timestamp": 1445452294.00448,
			"value": "onboard-success",
			"text": "onboarding process completed"
		}
	],
	"warnings": [],
	"status": "success",
	"errors": [
		{
			"value": "onboard vnfd/ping_vnfd.xml",
			"timestamp": 1445452294.0032463
		}
	]
};

var updatePayload = {
	"events": [
		{
			"timestamp": 1445452293.688965,
			"value": "update-started",
			"text": "updating process started"
		},
		{
			"timestamp": 1445452293.7836523,
			"value": "update-pkg-upload",
			"text": "uploading package"
		},
		{
			"timestamp": 1445452293.8315961,
			"value": "update-img-upload",
			"text": "uploading image"
		},
		{
			"timestamp": 1445452293.8316283,
			"value": "update-dsc-validation",
			"text": "descriptor validation"
		},
		{
			"timestamp": 1445452294.00448,
			"value": "update-success",
			"text": "updating process completed"
		}
	],
	"warnings": [],
	"status": "success",
	"errors": [
		{
			"value": "update vnfd/ping_vnfd.xml",
			"timestamp": 1445452294.0032463
		}
	]
};

var exportPayload = {
	"events": [
		{
			"timestamp": 1445452293.688965,
			"value": "update-started",
			"text": "download process started"
		},
		{
			"timestamp": 1445452293.7836523,
			"value": "update-pkg-upload",
			"text": "building package"
		},
		{
			"timestamp": 1445452293.8315961,
			"value": "update-img-upload",
			"text": "building image"
		},
		{
			"timestamp": 1445452293.8316283,
			"value": "update-dsc-validation",
			"text": "descriptor validation"
		},
		{
			"timestamp": 1445452294.00448,
			"value": "download-success",
			"text": "download process completed"
		}
	],
	"warnings": [],
	"status": "success",
	"errors": [
		{
			"value": "download nsd/ping-pong-nsd.xml",
			"timestamp": 1445452294.0032463
		}
	]
};

apiServer.use(cors());
launchpadServer.use(cors());

apiServer.use(multer({
	dest: './uploads/',
	rename: function (fieldname, filename) {
		return filename + '-' + Date.now();
	},
	onFileUploadStart: function (file) {
		console.log(file.originalname + ' is starting ...')
	},
	onFileUploadComplete: function (file) {
		console.log(file.fieldname + ' uploaded to  ' + file.path)
		done = true;
	}
}));

/*Handling routes.*/

launchpadServer.get('/launchpad/catalog', function (req, res) {
	res.sendFile(path.join(__dirname, '../../src/assets/ping-pong-catalog.json'));
});

launchpadServer.delete('/launchpad/catalog/nsd/:id', function (req, res) {
	res.end();
});

launchpadServer.put('/launchpad/catalog/nsd/:id', function (req, res) {
	res.end();
});

apiServer.post('/api/upload', function (req, res) {
	if (done == true) {
		res.end('{"transaction_id": "' + Date.now() + '"}');
	}
});

apiServer.get('/api/upload/:transaction_id/state', function (req, res) {
	if (done == true) {
		console.log('transaction id state request', req.params.transaction_id);
		res.end(JSON.stringify(uploadPayload));
	}
});

apiServer.post('/api/update', function (req, res) {
	if (done == true) {
		res.end('{"transaction_id": "' + Date.now() + '"}');
	}
});

apiServer.get('/api/update/:transaction_id/state', function (req, res) {
	if (done == true) {
		console.log('transaction id state request', req.params.transaction_id);
		res.end(JSON.stringify(updatePayload));
	}
});

apiServer.get('/api/export', function (req, res) {
	res.end('{"transaction_id": "' + Date.now() + '"}');
});

apiServer.get('/api/export/:transaction_id/state', function (req, res) {
	console.log('transaction id state request', req.params.transaction_id);
	res.end(JSON.stringify(exportPayload));
});

/*Run the servers.*/
launchpadServer.listen(3000, function () {
	console.log('LaunchPad Server listening on port 3000');
});

apiServer.listen(4567, function () {
	console.log('API Server listening on port 4567');
});
