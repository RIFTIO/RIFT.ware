define(function (require) {
    var registerSuite = require('intern!object');
    var assert = require('intern/chai!assert');
    var plugin_discoverer = require('intern/dojo/node!../../framework/core/plugin_discoverer');
    registerSuite({
        name: 'plugin_discoverer',

        init: function () {
            var res = plugin_discoverer.init();
            assert.isUndefined(res, 'return value is undefined');
        }
    });
});
