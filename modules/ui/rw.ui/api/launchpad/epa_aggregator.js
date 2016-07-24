/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var EpaSchema = {
  "vswitch-epa": {
    "objType": "container",
    "ovs-offload": {
      "objType": "leaf"
    },
    "ovs-acceleration": {
      "objType": "leaf"
    }
  },
  "hypervisor-epa": {
    "objType": "container",
    "type": {
      "objType": "leaf"
    },
    "version": {
      "objType": "leaf"
    }
  },
  "host-epa": {
    "objType": "container",
    "cpu-model": {
      "objType": "leaf"
    },
    "cpu-arch": {
      "objType": "leaf"
    },
    "cpu-vendor": {
      "objType": "leaf"
    },
    "cpu-socket-count": {
      "objType": "leaf"
    },
    "cpu-core-count": {
      "objType": "leaf"
    },
    "cpu-feature": {
      "objType": "leaf-list"
    }
  },
  "guest-epa": {
    "objType": "container",
    "mempage-size": {
      "objType": "leaf"
    },
    "trusted-execution": {
      "objType": "leaf"
    },
    "cpu-pinning-policy": {
      "objType": "leaf"
    },
    "cpu-thread-pinning-policy": {
      "objType": "leaf"
    },
    "pcie-device": {
      "objType": "list"
    },
    "numa-node-policy": {
      "objType": "container",
      "node": {
        "objType": "list"
      }
    }
  }
}

module.exports = function aggregateEPA(vdur, data) {
  var data = data || {};
  var schema = EpaSchema;
  var findLeaf = function(obj, schema, data) {
    for (k in obj) {
      var epaSchema = schema[k];
      //if key exists in schema
      if (epaSchema) {
        if (!data[k]) {
          data[k] = {};
        }
        switch (epaSchema.objType) {
          case 'container':
            findLeaf(obj[k], epaSchema, data[k]);
            break;
          case 'leaf':
            aggregateValue(obj[k].toString(), data[k]);
            break;
          case 'leaf-list':
            aggregateLeafListValue(obj[k], data[k]);
            break;
          case 'list':
            aggregateListValue(k, obj[k], data[k]);
            break;
          case 'choice':
            aggregateChoiceValue(obj[k], data[k]);
            break;
        }
      }
    }
    return data;
  }
  try {
    vdur.map(function(vm) {
      findLeaf(vm, schema, data)
    })
  }
  catch(e) {

  }

  return data;

  function aggregateChoiceValue(obj, data) {
    for (k in obj) {
      aggregateValue(k, data)
    }
  }

  function aggregateValue(value, data) {
    value = value.toString();
    if (!data[value]) data[value] = {};
    var total = data[value];
    total.total = (!total.total) ? 1 : total.total + 1;
  }

  function aggregateListValue(k, value, data) {
    if (value.length > 0) {
      aggregateValue(k, data);
    }
  }

  function aggregateLeafListValue(obj, data) {
    if (obj.constructor.name == "Array") {
      obj.map(function(p) {
        aggregateValue(p, data);
      })
    } else {
      console.log("Oops, something went wrong here. An object was passed that wasn't an Array. Are you sure it was leaf-list?")
    }
  }
}
