
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var React = require('react');


var exports = {
  carousel: require('./carousel.js'),
  React: React
};
if (typeof module == 'object') {
  module.exports = exports;
}
if (typeof window.$test == 'object' && typeof window == 'object') {
  $rw.component = exports;
} else {
  window.$test = {
    component: exports
  };
}
