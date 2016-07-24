
var _ = require('lodash');

/*
 * Args:
 * rules - object with the monitoring param to simulator function mapping
 *         see data/simmp.json
 */
SimMp = function(rules) {
    this.rules = _.clone(rules, true);
};

//SimMp.prototype.map_rule = function(mp_id) {
//    return this.rules['mp-mapper'][mp_id];
//}

// Use the monitoring param id for now
SimMp.prototype.createSimMonitorFunc = function(mp) {

    // Define our core simulation function here
    //
    // min, max inclusive
    var rand_func = function(min, max) {
        return Math.floor(Math.random() * (max-min+1)) + min;
    }

    var funcs = {
        // transmit and receive rate
        tx_rc_rate: function(value, elapsed_seconds) {
            // Ignore elapsed time for first implementation of transmit and
            // receive rate simulation.
            // This is just a quick and dirty and simple implementation to make
            // the monitoring params change, stay within bounds, and not swing
            // wildly.
            var min_val = mp.min_value;
            var max_val = mp.max_value;
            // Set an outer bound of maxmium change from current value
            // Tweak bin_count to set how much the value can swing from the
            // last value
            var bin_count = 10;
            // Set the range we can generate the new value based on a function
            //  of the difference of the max and min values
            var max_delta = (max_val - min_val) / bin_count;
            console.log('Setting max_delta = %s', max_delta);
            var new_val = rand_func(
                    Math.max(min_val, value-max_delta),
                    Math.min(max_val, value+max_delta));
            //console.log("Generated value: %s", new_val);
            return new_val;
        },
        packet_size: function(value, elapsed_seconds) {
            // Stub method just returns value unchanged
            // TODO: Figure out how we want to vary packet sizes
            return value;
        },
        accumulate: function(value, elapsed_seconds) {
            // NOT TESTED. Basic idea. Will want to add variablility
            // how fast we accumulate
            var accumulate_rate = 0.1;
            var new_value = value + (elapsed_seconds * accumulate_rate);
            return new_value;
        }
        // add growth function
    };

    // Declare our monitoring param id to sim function mapping here
    // TODO: Move out to a yaml/json file and make this function belong to
    // a 'Class'
    //var mapper = {
    //    'tx-rate-pp1': funcs['tx_rc_rate'],
    //    'rc-rate-pp1': funcs['tx_rc_rate'] 
    //};

    var sim_func_name = this.rules['mp-mapper'][mp.id];
    if (sim_func_name) {
        return funcs[sim_func_name];
    } else {
        console.log('No time step sim function found for monitoring param with id "%s", using constant value', mp.id); 
        return function(value, elapsed_seconds) {
            return value;
        }
    }
}

module.exports = {
    SimMp: SimMp
};
