// import SkyquakeRouter from './../../framework/widgets/skyquake_container/skyquakeRouter.jsx';

define([
        'intern!object',
        'intern/chai!assert',
        'require',
        './../support/babel!../../framework/widgets/skyquake_container/skyquakeRouter.jsx'
       ],

    function (registerSuite, assert, require, skyquakeRouter) {
        console.log(skyquakeRouter)
    registerSuite({
        name: 'plugin_discoverer',

        init: function () {
            // var res = plugin_discoverer.init();
            // assert.isUndefined(res, 'return value is undefined');
        }
    });
});
