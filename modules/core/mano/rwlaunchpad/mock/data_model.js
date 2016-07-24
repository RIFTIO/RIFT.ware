/*
 *  This module provides the data model layer for the Launchpad Mocklet
 */

var util = require('util');
var uuid = require('node-uuid');
var _ = require('lodash');

// Our modules
var simmp_module = require('./simmp.js');

// Data packages
// TODO: Make these parameters to pass to the data model
// instead of hardcoding them as requires here
var simmp_rules = require('./data/simmp-rules.json');
var nsr_templates = require('./data/nsr-templates.json');
var vnfr_templates = require('./data/vnfr-templates.json');

/*
 * Generic  to throw on data model exceptions
 */
function DataModelException(message) {
    this.message = message;
    this.name = "DataModelException";
}

/*
 * This 
 * This function is temporary until all needed features are implemented in this mocklet
 */
function NotImplementedException(message) {
    this.message = "You have fallen off the edge of the world: "+message;
    this.name = 'NotImplementedException';
}


/*
 * Class to handle simulating events over time for monitoring params
 */
MonitoringParam = function(values, time_function) {
    this.values = values;
    this.timeFunc = time_function;
}

MonitoringParam.prototype.timeStep = function(elapsed_seconds) {
    this.values.current_value = this.timeFunc(this.values.current_value,
            elapsed_seconds);
    return this.values.current_value;
};

/*
 * DataModel constructor
 *
 * Arguments
 *   restconf_host - Host name and port. eg: 'localhost:8008'
 */
DataModel = function (restconf_host) {
    this.restconf_host = restconf_host ? restconf_host : "localhost:8008";

    this.simmp = new simmp_module.SimMp(simmp_rules);
    if (!this.simmp) {
        throw "simmp failed to initialize";
    }
    // Time data for event simulation (monitoring params)
    this.start_time = Date.now();
    this.previous_time =this.start_time;

    // Store descriptors
    this.descriptors = { nsd: {}, vnfd: {}, vld: {} };

    // Store instance config data. Currently only NS Yang implements config data
    this.config_records = { nsr: {}, vnfr: {}, vlr: {} };

    // Stores Virtual Network Function instance records
    this.vnfr_records = { };

    // Stores Network Service instance operational records
    this.ns_opdata_records = { };

    // Manage which mock data to use next
    this.vnfr_template_index = 0;
    this.nsr_template_index = 0;

    // Operational (running) state for opdata records
    // 'on', 'off'
    // TBD: do restarting
    this.opstate = { nsr: {}, vnfr: {} };

    // Store MonitoringParam objects
    this.monitoring_params = {nsr: {}, vnfr: {} };
}


/*
 * creates a descriptor name from the record name
 */
DataModel.prototype.rec2desc = function (record_type) {
    if (record_type.charAt(record_type.lenth-1) == 'r') {
        return record_type.slice(0, -1)+'d';
    } else if (["ns","vnf","vl"].indexOf(record_type_) != -1) {
        return record_type + 'd';
    } else {
        throw new DataModelException('"%s" is not a supported record type', record_type);
    }
};

DataModel.prototype.setDescriptor = function(descriptor_type, descriptor) {
        if (!this.descriptors.hasOwnProperty(descriptor_type)) {
            throw new DataModelException('"%s" is not a supported descriptor type', descriptor_type);
        }

        this.descriptors[descriptor_type][descriptor.id] = descriptor;
};

DataModel.prototype.setConfigRecord = function(record_type, record) {
         if (!this.config_records.hasOwnProperty(record_type)) {
            throw new DataModelException('"%s" is not a supported record type', record_type);
        }

        this.config_records[record_type][record.id] = record;
};

DataModel.prototype.findConfigRecord = function(record_type, record_id) {
        if (this.config_records.hasOwnProperty(record_type)) {
            return this.config_records[record_type][record_id];
        } else {
            return null;
        }
};

/*
 *
 */
DataModel.prototype.updateControlParam = function(record_type, record_id,
        control_id, value) {
    if (record_type == 'vnfr') {
        var record = this.vnfr_records[record_id];
    } else {
        var record = this.ns_opdata_records[record_id];
    }
    // find the control param
    if ('control_param' in record) {
        for (var i=0; i < record.control_param.length; i++) {
            if (control_id == record.control_param[i].id) {
                // Make sure value is within min and max values
                if (value >= record.control_param[i].min_value &&
                    value <= record.control_param[i].max_value) {

                    record.control_param[i].current_value = value;
                    return 'SUCCESS';
                } else {
                    var errmsg = 'value "'+value+'" out of range. '+
                        'Needs to be within '+ record_control_param[i].min_value +
                        ' and ' + record_control_param[i].max_value;
                    throw new DataModelException(errmsg);
                }
            }
        }
    } else {
        var errmsg = 'Record type "' + record_type + '" with id "'+
            record_id + '" does not have any control params';
        throw new DataModelException(errmsg);
    }
};

/*
 * NS functions
 *
 * General comments on NS instance config/opdata:
 *  For each ns-instance-config, the descriptor needs to be added first
 */

// TODO: Consolidate the template handling functions
DataModel.prototype.nextNsrTemplate = function() {
    var nsr_template = _.clone(nsr_templates[this.nsr_template_index], true);
    this.nsr_template_index += 1;
    if (this.nsr_template_index >= nsr_templates.length) {
        this.nsr_template_index = 0;
    }
    return nsr_template;
};

DataModel.prototype.getNsdConnectionPoints = function(nsd_id) {
    var nsd =  this.descriptors['nsd'][nsd_id];
    if (!nsd) {
        throw new DataModelException("NSD ID '%s' does not exist", nsd_id);
    }
    // console.log("\n\nnsd = %s", JSON.stringify(nsd));
    return nsd['connection_point'];
};


DataModel.prototype.createNsrControlParams = function(ns_instance_config_id) {
    // TODO: find all VNFDs associated with this NS instance
    // then either call this.createVnfrControlParams if you want to talk
    // VNFR specific control params or we can generalize 'createVnfrControlParams'
    // to pass in 'record_id' instead of vnfr_id.
    //
    var control_params = [];

    return control_params;
};

/*
 * Sets an ns-instance-config object record and creates an
 * ns-instance-opdata record.
 *
 * If the NS instance opdata record matching the id of the ns-instance-config
 * already exists, then remove the ns-instance-opdate record and reconstruct.
 */
DataModel.prototype.setNsInstanceConfig = function(ns_instance_config) {
    // we are updating an existing ns-instance record set
    // There is an issue that subsequent 'PUT' actions do not transfer
    // the whole data to the mocklet. So we need to retrieve the existingt
    // ns-instance-config to get the nsd-ref

    // TODO: Consider creating a 'set_or_update' method for ns-instance-config
    var ns_config = this.findConfigRecord('nsr', ns_instance_config.id);
    if (ns_config) {
        ns_config.admin_status = ns_instance_config.admin_status;
    } else {
        this.setConfigRecord('nsr', ns_instance_config);
        ns_config = ns_instance_config;
    }
    if (ns_config.id in this.ns_opdata_records) {
        delete this.ns_opdata_records[ns_config.id];
    }
    // if ns-instance-config is 'ENABLED', then create an ns-instance-opdata
    if (ns_config.admin_status == 'ENABLED') {
        ns_opdata = this.generateNsInstanceOpdata(ns_config);
        // set the ns instance opdata. Doesn't matter if it already exists
        this.ns_opdata_records[ns_opdata.ns_instance_config_ref] = ns_opdata;
    }
};

DataModel.prototype.generateMonitoringParams = function(descriptor_type, descriptor_id) {
    console.log('Called generateMonitoringParams');
    if (!(descriptor_type in this.descriptors)) {
        throw DataModelException('descriptor type "%s" not found');
    }
    var descriptor = this.descriptors[descriptor_type][descriptor_id];
    var a_simmp = this.simmp;
    if (descriptor) {
        if ('monitoring_param' in descriptor) {
            return descriptor['monitoring_param'].map(function(obj) {
                var simFunc = a_simmp.createSimMonitorFunc(obj);
                return new MonitoringParam(_.clone(obj, true), simFunc);
            });
        } else {
            console.log('Descriptor(type=%s) with (id=%s) does not have ' +
               'monitoring params', descriptor_type, descriptor_id);
            return [];
        }
    } else {
        throw new DataModelException("Cannot find descriptor %s with id '%s'",
                descriptor_type, descriptor_id);
    }
};

DataModel.prototype.updateMonitoringParams = function(instance_type, instance_id) {
    var sim_mp = this.monitoring_params[instance_type][instance_id];
    if (sim_mp) {
        var time_now = Date.now();
        var elapsed_seconds = (time_now - this.previous_time) / 1000;
        var monitoring_params = sim_mp.map(function(obj) {
            obj.timeStep(elapsed_seconds);
            return obj.values;
        });
        this.previous_time = time_now;
        return monitoring_params;
    } else {
        // TODO: Figure out hosw we want to handle this case
        return [];
    }
};

/*
 * Creates an ns-instance-opdata object, but does not add it to the data
 * store.
 */
DataModel.prototype.generateNsInstanceOpdata = function (ns_config) {
    var nsr_template = this.nextNsrTemplate();

    // HACK: We need to get control and action param from the nsr
    // or have a function that synchronizes the next array element in
    // the templates
    var vnfr_template = this.nextVnfrTemplate();

    var nsd_id = ns_config.nsd_ref;
    var connection_points = this.getNsdConnectionPoints(ns_config.nsd_ref);
    var sim_mp = this.generateMonitoringParams('nsd', nsd_id);
    // save for using in update
    this.monitoring_params['nsr'][ns_config.id] = sim_mp;
    var monitoring_params = sim_mp.map(function(obj) {
        // not time stepping when we create them
        return obj.values;
    });

    return {
        ns_instance_config_ref: ns_config.id,
        'connection_point' : _.clone(connection_points, true),
        epa_param: _.clone(nsr_template['epa_param'], true),
        // NOTE: Remarked out until nfvi metrics figured out
        //nfvi_metric: _.clone(nsr_template['nfvi_metric'], true),
        monitoring_param: monitoring_params,
        //monitoring_param: _.clone(nsr_template['monitoring_param'], true),
        create_time: nsr_template['create_time'],
        action_param: vnfr_template['action_param'],
        // TODO: control_param: this.createNsrControlParams(ns_config.id);
        control_param: vnfr_template['control_param']
    };
};

DataModel.prototype.getNsInstanceOpdata = function() {
    var opdata_records = [];
    var config_records = this.config_records['nsr'];
    for (config_record_id in config_records) {
        if (config_records[config_record_id]['admin_status'] == 'ENABLED') {
            console.log('Is ENABLED: ns-instance-config record with id %s', config_record_id);

            ns_op_rec = this.ns_opdata_records[config_record_id];
            if (ns_op_rec) {
                // TODO: update monitoring params
                ns_op_rec.monitoring_param = this.updateMonitoringParams(
                        'nsr', config_record_id);
                opdata_records.push(ns_op_rec);
            } else {
                console.log('NO RECORD FOUND for ns config id: %s', config_record_id);
            }
        } else {
            console.log('Either no admin status record or not enabled');
        }
    }
    return opdata_records;
};


/* =============
 * VNF functions
 * =============
 */

/*
 * Gets the next VNFR template from the array of VNFR templates and 
 * increments the VNFR template counter. Wraps back to the first VNFR
 * template when the last one is used.
 */
DataModel.prototype.nextVnfrTemplate = function() {
    var vnfr_template = _.clone(vnfr_templates[this.vnfr_template_index], true);
    this.vnfr_template_index += 1;
    if (this.vnfr_template_index >= vnfr_templates.length) {
        this.vnfr_template_index = 0;
    }
    return vnfr_template;
}

/*
 * Arguments
 *  vnfd - VNF Descriptor object
 *  vnfr_id - VNFR unique identifier
 *  host  - host name and port
 */
DataModel.prototype.createVnfrActionParams = function(vnfd, vnfr_id) {
    // Canned start, stop for now
    // TBD: read action params from VNFD and create here
    // Use
    var action_param = [
        {
            id: uuid.v1(),
            name: "Start Me",
            description: "Start this VNFR",
            group_tag: "start-vnfr",
            url: "https://"+this.restconf_host+"/api/operations/start-vnfr",
            operation: "POST",
            payload: '{"start-vnfr": { "id": "'+vnfr_id+'"}}'
        },
        {
            id: uuid.v1(),
            name: "Stop Me",
            description: "Stop this VNFR",
            group_tag: "stop-vnfr",
            url: "https://"+this.restconf_host+"/api/operations/stop-vnfr",
            operation: "POST",
            payload: '{"stop-vnfr": { "id": "'+vnfr_id+'"}}'
        }
    ];
    return action_param;
};

DataModel.prototype.createVnfrControlParams = function(vnfd, vnfr_id,
        vnfr_template) {
    console.log("Called Datamodel.prototype.createVnfrControlParams");
    if (vnfr_template) {
        console.log("returning clone of vnfr_template['control_param']");
        return _.clone(vnfr_template['control_param'], true);
    } else {
        if (vnfd.control_param) {
            console.log("VNFD's control-param="+JSON.stringify(vnfd.control_param));
            var a_restconf_host = this.restconf_host;
            var cp_arry = _.clone(vnfd.control_param, true);
            var control_params = vnfd.control_param.map(function(obj) {
                var cp = _.clone(obj, true);
                cp.url = util.format(cp.url, a_restconf_host);
                console.log("\ncontrol-param payload before:"+ cp.payload);
                cp.payload = util.format(cp.payload, vnfr_id);
                console.log("\ncontrol-param payload after:"+ cp.payload+"\n");
                return cp;
            });
            return control_params;
        } else {
            return [];
        }
        throw new NotImplementedException("createVnfrControlParam: non-template");
    }
}

/*
 * Creates a new VNFR base on the VNFD in the argument.
 * This method is intended to not have side effects, otherwise
 * just put this code in this.addVnfData
 */
DataModel.prototype.createVnfr = function(vnfd) {
    //var vnfr_template = this.nextVnfrTemplate();
    var vnfr_id = uuid.v1();

    return {
        id: vnfr_id,
        // Hack: Copy the VNFD values but append '-Record' to end
        name: vnfd.name + ' Record',
        short_name: vnfd.short_name + '_REC',
        vendor: vnfd.vendor,
        description: vnfd.description,
        version: vnfd.version,
        vnfd_ref: vnfd.id,
        internal_vlr: [],
        // Even though this is in the Yang, it doesn't exist in the
        // instantiated model:
        // 'internal_connection_point_ref': [],
        action_param: this.createVnfrActionParams(vnfd, vnfr_id),
        //control_param: _.clone(vnfr_template['control_param'], true)
        control_param: this.createVnfrControlParams(vnfd, vnfr_id)
    };
};


/*
 * Creates and adds a new VNFD and matching VNFR record to our data store
 *
 * TODO: Might need to be updated so we create a VNFR when a start VNFR is called
 *
 */
DataModel.prototype.addVnfData = function(vnfd) {
    // if the vnfd does not already exist:
    if (this.descriptors['vnfd'][vnfd.id] == null) {
        console.log("adding new vnfd with id %s", vnfd.id);
        this.setDescriptor('vnfd', vnfd);
        // create a vnfr record, but without monitoring-param
        var vnfr = this.createVnfr(vnfd);

        var sim_mp = this.generateMonitoringParams('vnfd', vnfd.id);
        // save for using in update
        this.monitoring_params['vnfr'][vnfr.id] = sim_mp;
        vnfr.monitoring_param = sim_mp.map(function(obj) {
            // not time stepping when we create them
            return obj.values;
        });
        this.vnfr_records[vnfr.id] = vnfr;
    } else {
        // do nothing
    }
};


DataModel.prototype.getVnfrs = function () {
    records = [];
    for (vnfr_id in this.vnfr_records) {
        // When admin-status is implemented, then return only those 'ENABLED'
        var vnfr_record = this.vnfr_records[vnfr_id];
        vnfr_record.monitoring_param = this.updateMonitoringParams(
                'vnfr', vnfr_id);
        records.push(vnfr_record);
    }
    return records;
}


// Move the following to a new VnfrManager class

DataModel.prototype.startVnfr = function(vnfr_id) {
    console.log('Calling DataModel.startVnfr with id "%s"', vnfr_id);

    console.log('Here are the VNFR ids we have:');
    for (key in this.vnfr_records) {
        console.log('id:%s"', key);
    }
    //console.log('vnfr_records = %s', JSON.stringify(this.vnfr_records));

    if (!(vnfr_id in this.vnfr_records)) {
        var errmsg = 'Cannot find vnfr record with id "'+vnfr_id+'"';
        console.error('\n\n'+errmsg+'\n\n');
        throw new DataModelException(errmsg);
    }
    // Just add/set it
    this.opstate.vnfr[vnfr_id] = 'ON';
    return this.vnfr_records[vnfr_id];
}

DataModel.prototype.stopVnfr = function(vnfr_id) {
    console.log('Calling DataModel.stopVnfr with id "%s"', vnfr_id);
    if (!(vnfr_id in this.vnfr_records)) {
        var errmsg = 'Cannot find vnfr record with id "'+vnfr_id+'"';
        console.error(errmsg);
        throw new DataModelException(errmsg);
    }
    // Just add/set it
    this.opstate.vnfr[vnfr_id] = 'OFF';
    return this.vnfr_records[vnfr_id];
}

DataModel.prototype.vnfrRunningState = function(vnfr_id) {
    if (!(vnfr_id in this.vnfr_records)) {
        throw new DataModelException(
                'DataModel.stopVnfr: Cannot find VNFR with id "%s"', vnfr_id);
    }
    if (vnfr_id in this.opstate.vnfr) {
        return this.opstate.vnfr[vnfr_data];
    } else {
        // Assume we are 'ON'
        return 'ON';
    }
}


/* ==========================
 * Debug and helper functions
 * ==========================
 */

DataModel.prototype.prettyPrint = function (out) {
    if (out == undefined) {
        out = console.log;
    }
    out('Descriptors:');
    for (descriptor_type in this.descriptors) {
        out("Descriptor type: %s", descriptor_type);
        for (descriptor_id in this.descriptors[descriptor_type]) {
            out("data=%s",descriptor_id,
                    JSON.stringify(this.descriptors[descriptor_type][descriptor_id]));
        };
    };

    out('\nConfigRecords:');
    for (record_type in this.config_records) {
        out("Record type: %s", record_type);
        for (record_id in this.config_records[record_type]) {
            out("data=%s", record_id,
                    JSON.stringify(this.config_records[record_type][record_id]));
        };
    };
};


module.exports = {
    DataModelException: DataModelException,
    NotImplementedException: NotImplementedException,
    MonitoringParam: MonitoringParam,
    DataModel: DataModel
};

