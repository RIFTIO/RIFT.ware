/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

'use strict';

// the models to be transformed into the output DSL JSON meta file
var yang = [require('./json-nsd.json'), require('./json-vnfd.json')];

var _ = require('lodash');
var inet = require('./ietf-inet-types.yang.json');

var utils = {
	resolvePath(obj, path) {
		// supports a.b, a[1] and foo[bar], etc.
		// where obj is ['nope', 'yes', {a: {b: 1}, foo: 2}]
		// then [1] returns 'yes'; [2].a.b returns 1; [2].a[foo] returns 2;
		path = path.split(/[\.\[\]]/).filter(d => d);
		return path.reduce((r, p) => {
			if (r) {
				return r[p];
			}
		}, obj);
	},
	assignPathValue(obj, path, value) {
		path = path.split(/[\.\[\]]/).filter(d => d);
		// enable look-ahead to determine if type is array or object
		const pathCopy = path.slice();
		// last item in path used to assign value on the resolved object
		const name = path.pop();
		const resolvedObj = path.reduce((r, p, i) => {
			if (typeof(r[p]) !== 'object') {
				// look-ahead to see if next path item is a number
				const isArray = !isNaN(parseInt(pathCopy[i + 1], 10));
				r[p] = isArray ? [] : {}
			}
			return r[p];
		}, obj);
		resolvedObj[name] = value;
	}
};

var isType = d => /^(leaf|leaf-list|list|container|choice|case|uses)$/.test(d);

function deriveCardinalityFromProperty(property, typeName) {
	if (String(property.mandatory) === 'true') {
		return '1';
	}
	let min = 0, max = Infinity;
	if (property.hasOwnProperty('min-elements')) {
		min = parseInt(property['min-elements'], 10) || 0;
	}
	if (property.hasOwnProperty('max-elements')) {
		max = parseInt(property['max-elements'], 10) || Infinity;
	} else {
		if (!/^(list|leaf-list)$/.test(typeName)) {
			max = '1';
		}
	}
	if (min > max) {
		return String(min);
	}
	if (min === max) {
		return String(min);
	}
	return String(min) + '..' + (max === Infinity ? 'N' : max);
}

function cleanWhitespace(text) {
	if (typeof text === 'string') {
		return text.replace(/\s+/g, ' ');
	}
	return text;
}

function buildProperties(typeData, typeName) {
	var properties = [];
	Object.keys(typeData).forEach(name => {
		var property = typeData[name];
		var listKey = typeName === 'list' ? String(property.key).split(/\s/).filter(k => k && k !== 'undefined') : false;
		var meta = {
			name: name,
			type: typeName,
			description: cleanWhitespace(property.description),
			cardinality: deriveCardinalityFromProperty(property, typeName),
			'data-type': property.type,
			properties: Object.keys(property).filter(isType).reduce((r, childType) => {
				return r.concat(buildProperties(property[childType], childType));
			}, [])
		};
		if (listKey) {
			meta.key = listKey;
		}
		properties.push(meta);
	});
	return properties;
}

function lookupUses(uses, yang) {
	function doLookup(lookupTypeName) {
		var key;
		// warn: hardcoded prefix support for mano-types - other prefixes will be ignored
		if (/^manotypes:/.test(lookupTypeName)) {
			var moduleName = lookupTypeName.split(':')[1];
			key = ['dependencies.mano-types.module.mano-types.grouping', moduleName].join('.');
		} else {
			var name = yang.name.replace(/^rw-/, '');
			key = ['dependencies', name, 'module', name, 'grouping', lookupTypeName].join('.');
		}
		return utils.resolvePath(yang, key);
	}
	if (typeof uses === 'object') {
		return Object.keys(uses).reduce((result, key) => {
			var found = doLookup(key);
			Object.keys(found).filter(isType).forEach(type => {
				var property = result[type] || (result[type] = {});
				Object.assign(property, found[type]);
			});
			return result;
		}, {});
	} else if (typeof uses === 'string') {
		return doLookup(uses);
	}
	return {};
}

function lookupTypedef(property, yang) {
	var key;
	var lookupTypeName = property.type;
	// warn: hardcoded prefix support - other prefixes will be ignored
	if (/^manotypes:/.test(lookupTypeName)) {
		var lookupName = lookupTypeName.split(':')[1];
		key = ['dependencies.mano-types.module.mano-types.typedef', lookupName].join('.');
	} else if (/^inet:/.test(lookupTypeName)) {
		var lookupName = lookupTypeName.split(':')[1];
		yang = inet;
		key = ['schema.module.ietf-inet-types.typedef', lookupName].join('.');
	}
	if (key) {
		return utils.resolvePath(yang, key);
	}
}

function resolveUses(property, yang) {
	var childData = property.uses;
	var resolved = lookupUses(childData, yang);
	//console.log('uses', childData, 'found', resolved);
	Object.keys(resolved).forEach(type => {
		var parentTypes = property[type] || (property[type] = {});
		// copy types into the parent types bucket
		Object.assign(parentTypes, resolveReferences(yang, resolved[type]));
	});
	delete property.uses;
}

function resolveTypedef(property, yang) {
	if (/:/.test(property.type)) {
		var found = lookupTypedef(property, yang);
		if (found) {
			Object.assign(property, found);
		}
	}
}

function resolveReferences(yang, data) {
	var dataClone = _.cloneDeep(data);
	function doResolve(typeData) {
		Object.keys(typeData).forEach(name => {
			var property = typeData[name];
			resolveTypedef(property, yang);
			Object.keys(property).filter(isType).forEach(childType => {
				if (childType === 'uses') {
					resolveUses(property, yang);
				} else {
					doResolve(property[childType]);
				}
			});
		});
	}
	doResolve(dataClone);
	return dataClone;
}

function module(yang) {
	let module;
	var name = yang.name.replace(/^rw-/, '');
	if (!name) {
		throw 'no name given in json yang';
	}
	const path = ['container', name + '-catalog'].join('.');
	module = utils.resolvePath(yang, path);

	if (!module) {
		module = utils.resolvePath(yang, ['schema', 'module', name, path].join('.'));
	}
	if (!module) {
		module = utils.resolvePath(yang, ['dependencies', name, 'module', name, path].join('.'));
	}
	if (!module) {
		throw 'cannot find the module' + name;
	}

	// module/agument/nsd:nsd-catalog/nsd:nsd/meta
	const augLeafPath = ['schema.module', 'rw-' + name, 'augment', '/' + name + ':' + name + '-catalog/' + name + ':' + name, 'leaf'];
	const meta = utils.resolvePath(yang, augLeafPath.concat('meta').join('.'));

	const putLeafPath = ['dependencies', name, 'module', name, path, 'list', name, 'leaf'];

	if (meta) {
		utils.assignPathValue(yang, putLeafPath.concat(['meta']).join('.'), meta);
	}

	// module/agument/nsd:nsd-catalog/nsd:nsd/logo
	const logo = utils.resolvePath(yang, augLeafPath.concat('logo').join('.'));
	if (logo) {
		utils.assignPathValue(yang, putLeafPath.concat(['logo']).join('.'), logo);
	}
	var data = module.list;

	return {name: name, data: resolveReferences(yang, data)};

}

function reduceModule(result, module) {
	result[module.name] = buildProperties(module.data, 'list')[0];
	return result;
}

var result = yang.map(module).reduce(reduceModule, {});

console.log(JSON.stringify(result, null, 5));
