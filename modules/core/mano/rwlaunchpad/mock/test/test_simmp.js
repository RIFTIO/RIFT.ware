var assert = require('assert');

var simmp_module = require('../simmp.js');

// This is an example test on SimMp. It is not a very good test, but shows
// how to write a basic test in mocha
describe('SimMp', function() {
    describe('#createSimMonitorFunc()', function () {
        it('should return tx_rc_rate', function () {
            var mp = {
                id: 'tx-rate-pp1',
                min_value: 0,
                max_value: 100,
                current_value: 0
            };
            var simmp = new simmp_module.SimMp({
                "mp-mapper": { "tx-rate-pp1": "tx_rc_rate" }
            });
            assert(simmp != null, 'Could not instantiate simmp');
            var func = simmp.createSimMonitorFunc(mp);
            var value = func(0);
            assert(value >= mp.min_value, 'value less than min value);
            assert(value <= mp.max_value, 'value greater than max value');

       });
    });
});

