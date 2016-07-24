
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
// var Replay = require('Replay');

// Uncomment and finish when backend API becomes available
// var mcAPI = require('../api/mission-control.js');
var cors = require('cors');
var fs = require('fs');
var online;
var path = require('path');


// Remove line when backend comes up
online = false;

var routes = function(app) {
	app.get('/api/config/cloud-type', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile(path.join(__dirname, '../sample_data/cloud-type.json'), function(err, data) {
				if (!err) {
					console.log('Got offline data for /api/config/cloud-type');
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});


	app.get('/api/config/pool-scaling-options', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile(path.join(__dirname, '../sample_data/pool-scaling-options.json'), function(err, data) {
				if (!err) {
					console.log('Got offline data for /api/config/pool-scaling-options');
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.post('/api/config/cloud-account', cors(), function(req, res) {
		if (online) {

		} else {
			// eat it
			console.log('Got post data from UI', req.body);
			res.send({
				status: 'OK'
			});
		}
	});

	app.post('/api/config/cloud-account/:id', cors(), function(req, res) {
		if (online) {

		} else {
			// eat it
			console.log('Got post data from UI', req.body, 'for id', req.params.id);
			res.send({
				status: 'OK'
			});
		}
	});

	app.get('/api/config/cloud-account', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/cloud-account.json', function(err, data) {
				if (!err) {
					console.log('Got offline data for /api/config/cloud-account');
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.get('/api/config/cloud-account/:id', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/cloud-account.json', function(err, data) {
				if (!err) {
					console.log('Got offline data for /api/config/cloud-account/:id', req.params.id);
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.delete('/api/config/cloud-account/:id', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Removing cloud-account id', req.params.id);
			res.send({
				status: 'OK'
			})
		}
	});



	app.post('/api/config/vm-pool', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Adding vm-pool');
			res.send({
				status: 'OK'
			})
		}
	});

	app.put('/api/config/vm-pool/:id', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Updating vm-pool id', req.params.id);
			res.send({
				status: 'OK'
			})
		}
	});

	app.get('/api/config/vm-pool', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/vm-pool.json', function(err, data) {
				if (!err) {
					console.log('Getting vm-pool');
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.get('/api/config/vm-pool/:id', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/vm-pool.json', function(err, data) {
				if (!err) {
					console.log('Getting vm-pool id', req.params.id);
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.delete('/api/config/vm-pool/:id', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Removing vm-pool id', req.params.id);
			res.send({
				status: 'OK'
			})
		}
	});



	app.post('/api/config/management-domain', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Creating management-domain');
			res.send({
				status: 'OK'
			})
		}
	});

	app.put('/api/config/management-domain/:id', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Updating management-domain id', req.params.id);
			res.send({
				status: 'OK'
			})
		}
	});

	app.get('/api/config/management-domain', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/management-domain.json', function(err, data) {
				if (!err) {
					console.log('Getting management-domain');
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.get('/api/config/vm-pool/:id', cors(), function(req, res) {
		if (online) {

		} else {
			fs.readFile('../sample_data/management-domain.json', function(err, data) {
				if (!err) {
					console.log('Getting management-domain id', req.params.id);
					res.send(JSON.parse(data));
				} else {
					res.send(err);
				}
			});
		}
	});

	app.delete('/api/config/management-domain/:id', cors(), function(req, res) {
		if (online) {

		} else {
			console.log('Removing management-domain id', req.params.id);
			res.send({
				status: 'OK'
			})
		}
	});

};

module.exports = routes
