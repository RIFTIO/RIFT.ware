/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 1/27/16.
 *
 * This class creates the initial descriptor model JSON objects.
 */

'use strict';

import _ from 'lodash'
import DescriptorTemplates from './DescriptorTemplates'
import DescriptorModelMetaFactory from './DescriptorModelMetaFactory'

const keys = Object.keys;

function resolveInitHandlers(model) {
	function init(m) {
		keys(m).map(key => {
			const value = m[key];
			if (_.isFunction(value)) {
				m[key] = value(m, key, model);
			}
			if (_.isArray(value)) {
				value.forEach(v => init(v));
			}
			if (_.isObject(value)) {
				init(value);
			}
		});
		return m;
	}
	return init(model);
}

export default {
	createModelForType(type) {
		const template = DescriptorTemplates[type];
		if (template) {
			const model = _.cloneDeep(template);
			return resolveInitHandlers(model);
		}
	}
}
