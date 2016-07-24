/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var router = require('express').Router();

router.get('/api/', function(req, res) {
	res.send({
		'bar': 'foods'
	});
});

router.get('/bar/', function(req, res) {
	res.send({
		'bar': 'foo'
	});
});

module.exports = router;
