AUTOBAHN_DEBUG = true;
var autobahn = require('autobahn');
var uuid = require('node-uuid');
var _ = require('lodash');

// Our modules
var dm = require('./data_model.js');


var DUMP_RESULTS = false;

// TODO: make the url be configurable via command line arg
var connection = new autobahn.Connection({
    url: 'ws://localhost:8090/ws',
    realm: 'dts_mock'
});

// Instance of our data model/data store
var dataModel = new dm.DataModel();

var descriptor_module = (function () {

    my = {};

    /*
     * This function sets descriptors in the dataModel
     */
    function on_config_descriptor_catalog(args) {
        try {
            var xpath = args[0];
            var msg = args[1];

            console.log("\n\n*** Got on_config_descriptor_catalog:\n    (xpath: %s)(msg: %j)", xpath, msg);

            var descriptor_type = xpath.match(new RegExp(/(nsd|vnfd|vld)-catalog/))[1];

            if (descriptor_type in msg) {
                msg[descriptor_type].forEach(function(entry) {
                    console.log('Assigning descriptor "%s" with id %s',
                        descriptor_type, entry.id);
                    if (descriptor_type == 'vnfd') {
                        console.log('-- Adding VNFR data');
                        dataModel.addVnfData(entry);
                    } else {
                        // Simply assign
                        dataModel.setDescriptor(descriptor_type, entry);
                    }
                });
            }
        } catch(e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    my.register = function (session) {
        console.log('Registering for descriptor handling');
        session.subscribe('dts.config.nsd-catalog', on_config_descriptor_catalog);
        session.subscribe('dts.config.vnfd-catalog', on_config_descriptor_catalog);
        session.subscribe('dts.config.vld-catalog', on_config_descriptor_catalog);
    };

    return my;
}());


var instance_module = (function () {
    my = {};

   function on_config_config(args) {
        try {
            var xpath = args[0];
            var msg = args[1];

            console.log("\n\n*** Got on_config_config:\n    (xpath: %s)(msg: %j)", xpath, msg);

            var record_type = xpath.match(new RegExp(/(ns|vnf|vl)-instance-config/))[1];
            record_type += 'r';

            console.log('record_type = %s', record_type);

            if (record_type in msg) {
                msg[record_type].forEach(function(entry) {
                    console.log('Assigning record (%s) id=%s, descriptor: id=%s',
                       record_type, entry.id, entry.nsd_ref);
                    if (record_type == 'nsr') {
                        dataModel.setNsInstanceConfig(entry);
                    } else {
                        // vnfd, vld, which don't have instance_config records yet
                        dataModel.setConfigRecord(record_type, entry);
                    }
                });
            }

        } catch (e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    /*
     * Get all nsr opdata records:
     *   xpath: D,/nsr:ns-instance-opdata/nsr:nsr
     *   msg: {"nsr":[{"ns_instance_config_ref":""}]}
     *
     * Get Ping Pong nsr opdata record:
     *   xpath: D,/nsr:ns-instance-opdata/nsr:nsr[nsr:ns-instance-config-ref='f5f41f36-78f6-11e5-b9ba-6cb3113b406f']
     *   msg: {"nsr":[{"ns_instance_config_ref":"f5f41f36-78f6-11e5-b9ba-6cb3113b406f"}]}
     *
     * Get monitoring param for nsr instance opdata record:
     *   xpath: D,/nsr:ns-instance-opdata/nsr:nsr[nsr:ns-instance-config-ref='f5f41f36-78f6-11e5-b9ba-6cb3113b406f']
     *   msg: {
     *          "nsr":[{
     *              "monitoring_param":[{"id":""}],
     *              "ns_instance_config_ref":"f5f41f36-78f6-11e5-b9ba-6cb3113b406f"
     *          }]}
     *
     * Note that the xpath arg is identical in getting the entire NSR and getting sub-elements in the NSR
     * The message tells what values to get
     */
    function on_get_opdata(args) {
        try {
            var xpath = args[0];
            var msg = args[1];
            //console.log("\n\n*** Got on_get_opdata:\n   (xpath: %s)(msg: %j)", xpath, msg);
            console.log("*** Got on_get_opdata:\n   (xpath: %s)(msg: %j)", xpath, msg);

            var record_type = xpath.match(new RegExp(/(ns|vnf|vl)-instance-opdata/))[1];
            record_type += 'r';

            var gi_type_map = {
                "nsr": "RwNsrYang.YangData_Nsr_NsInstanceOpdata",
                "vnfr": "VnfrYang.YangData_Vnfr_VnfInstanceOpdata_Vnfr",
                "vlr": "VlrYang.YangData_Vlr_VlInstanceOpdata_Vlr"
            };

            if (record_type == 'nsr') {
                //console.log("###################\n   data model:\n\n");
                //dataModel.prettyPrint();
                var response = {
                    'nsr': dataModel.getNsInstanceOpdata()
                };
                var respond_xpath = 'D,/nsr:ns-instance-opdata';
            } else {
                throw new dm.NotImplementedException(
                        "record_type '%s' is not yet supported.", record_type);
            }

            var result = new autobahn.Result([
                'RwNsrYang.YangData_Nsr_NsInstanceOpdata',
                response
            ], {"xpath": respond_xpath});

            if (DUMP_RESULTS)
                console.log("result=\n%s", JSON.stringify(result) );

            return result;
        } catch(e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    function on_get_vnfr_catalog(args) {
        try {
            var xpath = args[0];
            var msg = args[1];
            console.log("*** Got on_vnfr_catalog:\n   (xpath: %s)(msg: %j)", xpath, msg);

            var response = {
                'vnfr': dataModel.getVnfrs()
            };
            var respond_xpath = 'D,/vnfr:vnfr-catalog';

            var result = new autobahn.Result([
                'RwVnfrYang.YangData_Vnfr_VnfrCatalog',
                response
            ], {"xpath": respond_xpath});

            if (DUMP_RESULTS)
                console.log("result=\n%s", JSON.stringify(result) );

            return result;
        } catch(e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    my.register = function (session) {
        console.log('Registering for record handling');
        session.register('dts.data.ns-instance-opdata', on_get_opdata);
        session.register('dts.data.vnfr-catalog', on_get_vnfr_catalog);
        session.subscribe('dts.config.ns-instance-config', on_config_config);
    }

    return my;
}());


var action_module = (function() {
    my = {};

    /*
     * Set the specified VNFR operating state
     *
     * (xpath: I,/lpmocklet:start-vnfr)
     * (msg: {"id":"f26b90b0-8184-11e5-bc47-2b429643382b"})
     */
    function on_set_opstate(args) {
        try {
            var xpath = args[0];
            var msg = args[1];

            console.log("\n\n*** Got on_start_vnfr:\n    (xpath: %s)(msg: %j)",
                xpath, msg);
            var action_match = xpath.match(new RegExp(/lpmocklet:(\w+)-(\w+)/));
            var action = action_match[1];
            var obj_type = action_match[2];

            var record_id = msg['id'];
            console.log('action="%s", obj_type="%s", record_id="%s"',
                    action, obj_type, record_id);

            if (obj_type == 'vnfr') {
                if (action == 'start') {
                    dataModel.startVnfr(record_id);
                }
                else if (action == 'stop') {
                    dataModel.stopVnfr(record_id);
                }
                else {
                    console.error('Unsupported opstate action "%s"', action);
                }
            } else {
                console.error('Unsupported opstate action object: "%s"',
                        obj_type);
            }

            console.log('\n\nBuilding response....');

            var response = {
                id: uuid.v1(),
                object_type: obj_type,
                action: action,
                status: 'SUCCESS' 
            };
            var respond_xpath = 'D,/lpmocklet:lpmocklet-action-status';
            var result = new autobahn.Result([
                    'LpmockletYang.YangData_Lpmocklet_LpmockletActionStatus',
                    response
                    ], {"xpath": respond_xpath});

            console.log('Done running on_set_opdata');
            return result;

        } catch (e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    function on_set_control_param(args) {
        try {
            var xpath = args[0];
            var msg = args[1];

            console.log("\n\n*** Got on_set_control_param:\n    (xpath: %s)(msg: %j)",
                xpath, msg);

            // We can ignore xpath. We expect: "I,/lpmocklet:set-control-param"
// msg: {"set":{"id":"f8d63b30-84b3-11e5-891c-61c6a71edd3c","obj_code":"VNFR","control_id":"ping-packet-size-1","value":10}}

            var response_class = 'LpmockletYang.YangData_Lpmocklet_LpmockletActionStatus';
            var status = dataModel.updateControlParam(
                    msg.obj_code.toLowerCase(),
                    msg.id,
                    msg.control_id,
                    msg.value);

            var response = {
                id: uuid.v1(),
                object_type: msg.obj_code,
                action: msg.control_id,
                status: status
            };

            var respond_xpath = 'D,/lpmocklet:lpmocklet-action-status';
            var result = new autobahn.Result([
                    'LpmockletYang.YangData_Lpmocklet_LpmockletActionStatus',
                    response
                    ], {"xpath": respond_xpath});

            console.log('Done running on_set_opdata');
            return result;
        } catch (e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    my.register = function(session) {
        console.log('Registering for action handling');
        session.register('dts.rpc.start-vnfr', on_set_opstate);
        session.register('dts.rpc.stop-vnfr', on_set_opstate);
        session.register('dts.rpc.set-control-param', on_set_control_param);
    }

    return my;

}());


connection.onopen = function (session) {
    console.log('Connection to wamp server established!');
    descriptor_module.register(session);
    instance_module.register(session);
    action_module.register(session);
}

console.log('Opening autobahn connection');
connection.open();

