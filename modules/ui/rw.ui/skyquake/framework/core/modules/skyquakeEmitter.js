/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

/**
 * skyquakeEmitter module. global channel for communication
 * @module framework/core/modules/skyquakeEmitter
 * @author Kiran Kashalkar <kiran.kashalkar@riftio.com>
 */

var events = require('events');
var skyquakeEmitter = new events.EventEmitter();

module.exports = skyquakeEmitter;