/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
//Alt Instance.
import Alt from 'alt';
var path = require('path');
if (!window.A) {
    window.A = new Alt();
    Alt.debug('alt', window.A);
    console.log('new alt', path.resolve(__dirname));
}

module.exports = window.A;
