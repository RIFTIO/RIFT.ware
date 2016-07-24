/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 1/27/16.
 *
 * This class provides utility methods for interrogating an instance of model uiState object.
 */

'use strict';

import _ from 'lodash'
import guid from './../guid'
import changeCase from 'change-case'
import InstanceCounter from './../InstanceCounter'
import DescriptorModelFields from './DescriptorModelFields'
import DescriptorTemplateFactory from './DescriptorTemplateFactory'

export default {
	isLeaf(property = {}) {
		return /leaf|choice/.test(property.type);
	},
	isList(property = {}) {
		return /list|leaf-list/.test(property.type);
	},
	isLeafList(property = {}) {
		return property.type === 'leaf-list';
	},
	isArray(property = {}) {
		// give '1' or '0..N' or '0..1' or '0..5' determine if represents an array
		// '0..1' is not an array
		// '0..2' is an array
		// '0..N' is an array
		const cardinality = String(property.cardinality).toUpperCase();
		const pos = cardinality.lastIndexOf('.') + 1;
		const val = cardinality.substr(pos);
		return val === 'N' || parseInt(val, 10) > 1;
	},
	isEnumeration(property = {}) {
		const type = property['data-type'] || {};
		return type.hasOwnProperty('enumeration');
	},
	isRequired(property = {}) {
		return /^1/.test(property.cardinality);
	},
	isObject(property = {}) {
		return !/^(leaf|leaf-list)$/.test(property.type);
	},
	isSimpleList(property = {}) {
		return _.contains(DescriptorModelFields.simpleList, property.name);
	},
	isPrimativeDataType(property = {}) {
		const Property = this;
		return /string|int/.test(property['data-type']) || Property.isEnumeration(property) || Property.isGuid(property);
	},
	defaultValue(property = {}) {
		if (property.defaultValue) {
			return property.defaultValue;
		}
		if (this.isObject(property)) {
			return {};
		}
		return '';
	},
	getContainerMethod(property, container, methodName) {
		const name = changeCase.camel(methodName + '-' + property.name);
		if (typeof container[name] === 'function') {
			return container[name].bind(container);
		}
	},
	getContainerCreateMethod(property, container) {
		const name = changeCase.camel('create-' + property.name);
		if (typeof container[name] === 'function') {
			return container[name].bind(container);
		}
	},
	containerHasCreateMethod(container, property = {}) {
		const find = changeCase.camel('create-' + property.name);
		return typeof container[find] === 'function';
	},
	getEnumeration(property = {}, value) {
		const enumeration = property['data-type'].enumeration.enum;
		if (typeof enumeration === 'string') {
			return [{name: enumeration, value: enumeration, isSelected: String(value) === enumeration}];
		}
		return Object.keys(enumeration).map(enumName => {
			let enumValue = enumName;
			// warn we only support named enums and systematically ignore enum values
			//const enumObj = enumeration[enumName];
			//if (enumObj) {
			//	enumValue = enumObj.value || enumName;
			//}
			return {name: enumName, value: enumValue, isSelected: String(enumValue) === String(value)};
		});
	},
	isGuid(property = {}) {
		const type = property['data-type'];
		if (typeof type === 'object' && type.leafref && type.leafref.path) {
			return /\bid$/.test(type.leafref.path);
		}
		return /uuid/.test(property['data-type']);
	},
	createModelInstance(property) {
		const Property = this;
		const defaultValue = Property.defaultValue.bind(this);
		function createModel(uiState, parentMeta) {
			const model = {};
			if (Property.isLeaf(uiState)) {
				if (uiState.name === 'name') {
					return changeCase.param(parentMeta.name) + '-' + InstanceCounter.count(parentMeta[':qualified-type']);
				}
				if (_.isArray(parentMeta.key) && _.contains(parentMeta.key, uiState.name)) {
					if (/uuid/.test(uiState['data-type'])) {
						return guid();
					}
					if (uiState['data-type'] === 'string') {
						const prefix = uiState.name.replace('id', '');
						return  (prefix ? changeCase.param(prefix) + '-' : '') + guid(5);
					}
					if (/int/.test(uiState['data-type'])) {
						return InstanceCounter.count(uiState[':qualified-type']);
					}
				}
				return defaultValue(uiState);
			} else if (Property.isList(uiState)) {
				return [];
			} else {
				uiState.properties.forEach(p => {
					model[p.name] = createModel(p, uiState);
				});
			}
			return model;
		}
		if (property) {
			if (Property.isPrimativeDataType(property)) {
				return defaultValue(property);
			}
			if (property.type === 'leaf') {
				return defaultValue(property);
			}
			if (/list/.test(property.type)) {
				property.type = 'container';
			}
			const modelInstance = createModel(property, property);
			modelInstance.uiState = {type: property.name};
			const modelFragment = DescriptorTemplateFactory.createModelForType(property[':qualified-type'] || property.name) || {};
			Object.assign(modelInstance, modelFragment);
			return modelInstance;
		}
	}
}
