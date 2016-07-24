AUTOBAHN_DEBUG = true;
var autobahn = require('autobahn');

var connection = new autobahn.Connection({
    url: 'ws://localhost:8090/ws',
    realm: 'dts_mock'
});


var cloud_account_module = (function () {
    var my = {};

    var next_cloud_account_index = 1;

    function gen_resource_list(resname, index, count) {
        resources = [];
        for (var i=1; i < count+1; i++) {
            resources.push(resname+'_'+index+'_'+i);
        }
        return resources;
    }

    function get_new_resources(index, count) {
        return {
            'vms': gen_resource_list('vm', index, count),
            'ports': gen_resource_list('port', index, count),
            'networks': gen_resource_list('network', index, count)
        };
    }

    var vms = {};
    var ports = {};
    var networks = {};
    var used_vms = {};
    var used_networks = {};
    var used_ports = {};
    var cloud_accounts = [];

    /*
     * At this point, this function doesn't do anything useful, but it tracks clould accounts created
     * and provides console loging when a cloud account is configured.
     */
    function on_cloud_account_config(args) {
    try {
            var xpath = args[0];
            var msg = args[1];
            console.log("\n\n*** Got on_cloud_account_config:\n    (xpath: %s)(msg: %j)", xpath, msg);
            var name_match = xpath.match(new RegExp(/rw-mc:account\[.*?name='(.*?)'/));
            var cloud_name = name_match[1];
            console.log('Attempting to create cloud account name = %s', cloud_name);
            console.log('next index = %s', next_cloud_account_index);

            index = next_cloud_account_index;
            if (cloud_accounts.indexOf(cloud_name) == -1) {
                resources = get_new_resources(index, 4);
                console.log('new resources = %s', JSON.stringify(resources));

                vms[cloud_name] = resources['vms'];
                ports[cloud_name] = resources['ports'];
                networks[cloud_name] = resources['networks'];
                used_vms[cloud_name] = [];
                used_ports[cloud_name] = [];
                used_networks[cloud_name] = [];
                cloud_accounts.push(cloud_name);
                next_cloud_account_index++;

                console.log("Created cloud_account '%s'", cloud_name);
            } else {
                console.log("cloud account '%s' already created", cloud_account);
            }
        } catch(e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    /*
     * This iteration of this function uses the same value for id and name for the vm and network
     * resources
     */
    function on_cloud_resources(args) {
        try {
            var xpath = args[0];
            var msg = args[1];
            console.log("\n\n*** Got on_cloud_resources:\n   (xpath: %s)(msg: %j)\n\n", xpath, msg);

            var name_match = xpath.match(new RegExp(/name='(.*?)'/));
            var cloud_name = name_match[1];

            var resources = {"vm": [], "network": [], "port": []};
            var i;
            for (i in vms[cloud_name]) {
                var vm_id = vms[cloud_name][i];
                resources["vm"].push({
                    "id": vm_id,
                    "name": vm_id,
                    "available": used_vms[cloud_name].indexOf(vm_id) === -1
                });
            }
            for (i in ports[cloud_name]) {
                var port_id = ports[cloud_name][i];
                resources["port"].push({
                    "id": port_id,
                    "available": used_ports[cloud_name].indexOf(port_id) === -1
                });
            }
            for (i in networks[cloud_name]) {
                var network_id = networks[cloud_name][i];
                resources["network"].push({
                    "id": network_id,
                    "name": network_id,
                    "available": used_networks[cloud_name].indexOf(network_id) === -1
                });
            }

            // Some times DTS asks us for a path under /resources (like vm)
            // It's easier to just always respond with the upper level
            // then creating a new protobuf message per possible child
            var respond_xpath = xpath.replace(/resources\/.*/, "resources");

            //console.log("   response_xpath='%s'", respond_xpath);
            //console.log("   resources = ");
            //console.log(JSON.stringify(resources));

            var result = new autobahn.Result([
                "RwMcYang.CloudResources",
                resources
            ], {"xpath": respond_xpath});

            return result;

        } catch (e) {
           console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    function on_cloud_pools(args) {
        try {
            var xpath = args[0];
            var msg = args[1];
            console.log("\n\n*** Got on_cloud_pools:\n   (xpath: %s)(msg: %j)\n\n", xpath, msg);

            var name_match = xpath.match(new RegExp(/name='(.*?)'/));
            var cloud_name = name_match[1];

            var assigned_pools = pools_module.get_assigned_pools(cloud_name);
            //console.log("assigned pools=%s", JSON.stringify(assigned_pools));

            // Some times DTS asks us for a path under /resources (like vm)
            // It's easier to just always respond with the upper level
            // then creating a new protobuf message per possible child
            var respond_xpath = xpath.replace(/pools\/.*/, "pools");

            //console.log("   response_xpath='%s'", respond_xpath);
            //console.log("   pools = ");
            //console.log(JSON.stringify(pools));

            var result = new autobahn.Result([
                "RwMcYang.CloudPools",
                assigned_pools
            ], {"xpath": respond_xpath});

            return result;

        } catch (e) {
           console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }


    my.register = function (session) {
        console.log("Registering for cloud account resources");
        session.subscribe("dts.config.cloud/account", on_cloud_account_config);
        session.register("dts.data.cloud/account/pools", on_cloud_pools);
        session.register("dts.data.cloud/account/resources", on_cloud_resources);
    };

    my.reserve_vm = function (cloud_name, vm_id) {
        //console.log("Reserving vm %sd for cloud account %s", vm_id, cloud_name);
        if (vms[cloud_name].indexOf(vm_id) === -1) {
            console.error("Cannot find vm_id %s in cloud %s vms %s", vm_id, cloud_name);
            return;
        }
        used_vms[cloud_name].push(vm_id);
    };

    my.reserve_port = function (cloud_name, port_id) {
        if (ports[cloud_name].indexOf(port_id) === -1) {
            console.error("Cannot find port_id %s in cloud %s vms %s", port_id, cloud_name);
            return;
        }
        used_ports[cloud_name].push(port_id);
    };

    my.reserve_network = function (cloud_name, network_id) {
        if (networks[cloud_name].indexOf(network_id) === -1) {
            console.error("Cannot find network_id %s in cloud %s vms %s", network_id, cloud_name);
            return;
        }
        used_networks[cloud_name].push(network_id);
    };

    my.get_available_vm = function (cloud_name) {
        console.log("\ncalled get_available_vm for cloud_name= %s", cloud_name);
        var available = [];
        vms[cloud_name].forEach(function(id){
            if (used_vms[cloud_name].indexOf(id) == -1) {
                available.push(id);
            }
        });
        return available;
    };

    my.get_assigned_vm = function (cloud_name) {
        console.log('\n called get_assigned_vm for cloud_name= %s', cloud_name);
        var assigned = [];
        vms[cloud_name].forEach(function(id) {
            if (used_vms[cloud_name].indexOf(id) != -1) {
                assigned.push(id);
            }
        });
        return assigned;
    };

    my.get_available_network = function (cloud_name) {
        var available = [];
        networks[cloud_name].forEach(function(id){
            if (used_networks[cloud_name].indexOf(id) == -1) {
                available.push(id);
            }
        });
        return available;
    };

    my.get_assigned_network = function (cloud_name) {
        var assigned = [];
        networks[cloud_name].forEach(function(id){
            if (used_networks[cloud_name].indexOf(id) != -1) {
                assigned.push(id);
            }
        });
        return assigned;
    };

    my.get_available_port = function (cloud_name) {
        var available = [];
        ports[cloud_name].forEach(function(id){
            if (used_ports[cloud_name].indexOf(id) == -1) {
                available.push(id);
            }
        });
        return available;
    };

    my.get_assigned_port = function (cloud_name) {
        var assigned = [];
        ports[cloud_name].forEach(function(id){
            if (used_ports[cloud_name].indexOf(id) != -1) {
                assigned.push(id);
            }
        });
        return assigned;
    };

    return my;
}());

var mgmt_domain_module = (function () {
    var my = {};

    var mgmt_domains = {};

   function on_domain_launchpad(xpath, msg) {
        console.log("Got on_domain_launchpad: (xpath: %s)(msg: %j)",
            xpath, msg);

        return new autobahn.Result([
            "RwMcYang.MgmtDomainLaunchpad",
            {"state": "starting", "ip_address": "1.2.3.4"}
        ]);
    }

    my.register = function (session) {
        session.register("dts.data.mgmt-domain/domain/launchpad", on_domain_launchpad);
    };

    return my;
}());


var pools_module = (function () {
    var my = {};

    pools = {"vm":{}, "network":{}, "port":{}};

    var mgmt_domain_pools = {};

    function on_pool_config(args) {
        try {
            var xpath = args[0];
            var msg = args[1];
            console.log("\n\n*** Got on_pool_config:\n   (xpath: %s)(msg: %j)", xpath, msg);

            var pool_type = xpath.match(new RegExp(/(vm|network|port)-pool/))[1];

            var pool_name = msg.name;
            var cloud_account = msg.cloud_account;

            console.log('   pool_type=%s, pool_name=%s', pool_type, pool_name);

            //console.log("\n   pools(before) = %s", JSON.stringify(pools));

            if (!(pool_name in pools[pool_type])){
                console.log("Creating %s pool: %s", pool_type, pool_name);
                pools[pool_type][pool_name] = {
                    "cloud_account":cloud_account,
                    "assigned":[]
                };
            }
            else{
                // If the pool was already added, then the cloud_account
                // will most likely NOT come in the second time around.
                // So fetch this from the stored pool info
               cloud_account = pools[pool_type][pool_name].cloud_account;
               console.log('Pool already exists. cloud_account=%s', cloud_account);
            }

            if ("assigned" in msg) {
                console.log('--- assigned in msg');

                msg.assigned.forEach(function (entry) {
                    var resource_id = entry.id;
                    console.log("Assigning cloud %s resource id %s to %s pool %s",
                        cloud_account, resource_id, pool_type, pool_name
                    );
                    cloud_account_module["reserve_" + pool_type](cloud_account, resource_id);
                    pools[pool_type][pool_name]["assigned"].push(resource_id);
                });
            }

        } catch (e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }

        //console.log("\n   pools(after) = %s", JSON.stringify(pools));
    }

    function on_pool_show(args){
        try {
            var xpath = args[0];
            var msg = args[1];
            console.log("\n\n*** Got on_pool_show:\n   (xpath: %s)(msg: %j)", xpath, msg);

            var pool_type = xpath.match(new RegExp(/(vm|network|port)-pool/))[1];
            var pool_name = xpath.match(new RegExp(/pool\[.*?name='(.*?)'/))[1];

            console.log('pool_type = %s, pool_name=%s', pool_type, pool_name);
            var pool_gi_type_map = {
                "vm": "RwMcYang.VmPool",
                "port": "RwMcYang.PortPool",
                "network": "RwMcYang.NetworkPool"
            };

            var response = {"name": pool_name, "available": [], "assigned": []};
            var available = cloud_account_module["get_available_"+pool_type](
                pools[pool_type][pool_name].cloud_account
            );

            console.log("available pool resources = %s", JSON.stringify(available));

            var assigned = cloud_account_module['get_assigned_'+pool_type](
                    pools[pool_type][pool_name].cloud_account
            );

            available.forEach(function(id){

                console.log('adding pool resource "%s" to available', id);
                response.available.push({'id': id});
                /*
                if (pool_type == 'port') {
                    console.log('doing port');
                    // if the pool resource does not have a name
                    response.available.push({"id": id });
                } else {
                    console.log('doing other than port');
                    // pool resource has id and name
                    response.available.push({'id': id, 'name': id});
                }*/
            });

            assigned.forEach(function(id){
                console.log('adding pool resource "%s" to assigned', id);
                response.assigned.push({'id': id});
            });

            for (var domain_name in mgmt_domain_pools){
                if (mgmt_domain_pools[domain_name][pool_type].indexOf(pool_name) != -1){
                    console.log("Found pool %s domain name: %s", pool_name, domain_name);
                    response.mgmt_domain = domain_name;
                }
            }

            //console.log("Responding to pool show: %s(%j)", pool_gi_type_map[pool_type], response);

            // Strip off anythign after the pool list element
            // D,/vm-pool/pool[name='asdf']/available -> D,vm-pool/pool[name='asdf']
            var respond_xpath = xpath.replace(/].*/, "]");
            result = autobahn.Result([
                pool_gi_type_map[pool_type],
                response
            ], {"xpath": respond_xpath});

            console.log('Response type=%s\nresponse = %s', pool_gi_type_map[pool_type],
                    JSON.stringify(response));
            return result;

        } catch(e) {
            console.error("Caught exception: %s\n\n%s", e, e.stack);
        }
    }

    my.get_assigned_pools = function(cloud_name) {
        assigned_pools = {};
        for (var pool_type in pools) {
            if (pools.hasOwnProperty(pool_type)) {
                assigned_pools[pool_type] = [];
                pool_type_data = pools[pool_type];
                for (var pool_name in pool_type_data) {
                    if (pool_type_data.hasOwnProperty(pool_name)) {
                        if (pool_type_data[pool_name].cloud_account == cloud_name) {
                            assigned_pools[pool_type].push({'name': pool_name});
                        }
                    }
                }
            }
        }
        return assigned_pools;
    }


    my.register = function (session) {
        session.subscribe("dts.config.vm-pool/pool", on_pool_config);
        session.subscribe("dts.config.port-pool/pool", on_pool_config);
        session.subscribe("dts.config.network-pool/pool", on_pool_config);

        session.register("dts.data.vm-pool/pool", on_pool_show);
        session.register("dts.data.port-pool/pool", on_pool_show);
        session.register("dts.data.network-pool/pool", on_pool_show);
    }

    return my;
}());

connection.onopen = function (session) {
    console.log("Connection to wamp server established!")
    console.log("Session: " + JSON.stringify(session, null, 4))

    mgmt_domain_module.register(session);
    cloud_account_module.register(session);
    pools_module.register(session);
};

console.log("Opening autobahn connection")
connection.open();

