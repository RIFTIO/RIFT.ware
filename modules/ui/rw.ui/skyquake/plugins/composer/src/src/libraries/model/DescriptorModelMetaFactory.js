/**
 * Created by onvelocity on 1/27/16.
 *
 * This class provides methods to get the metadata about descriptor models.
 */

'use strict';

import _ from 'lodash'
import utils from './../utils'
import DescriptorModelMetaJSON from './DescriptorModelMeta.json'
import DescriptorModelMetaProperty from './DescriptorModelMetaProperty'

const assign = Object.assign;

const exportInnerTypesMap = {
	'constituent-vnfd': 'nsd.constituent-vnfd',
	'vdu': 'vnfd.vdu'
};

function getPathForType(type) {
	if (exportInnerTypesMap[type]) {
		return exportInnerTypesMap[type];
	}
	return type;
}

const modelMetaByPropertyNameMap = Object.keys(DescriptorModelMetaJSON).reduce((map, key) => {
	function mapProperties(parentMap, parentObj) {
		parentMap[':meta'] = parentObj;
		const properties = parentObj && parentObj.properties ? parentObj.properties : [];
		properties.forEach(p => {
			parentMap[p.name] = mapProperties({}, assign(p, {':qualified-type': parentObj[':qualified-type'] + '.' + p.name}));
			return map;
		}, parentMap);
		return parentMap;
	}
	map[key] = mapProperties({}, assign(DescriptorModelMetaJSON[key], {':qualified-type': key}));
	return map;
}, {});

(() => {
	// initialize the UI centric properties that CONFD could care less about
	utils.assignPathValue(modelMetaByPropertyNameMap, 'nsd.meta.:meta.preserve-line-breaks', true);
	utils.assignPathValue(modelMetaByPropertyNameMap, 'vnfd.meta.:meta.preserve-line-breaks', true);
	utils.assignPathValue(modelMetaByPropertyNameMap, 'vnfd.vdu.cloud-init.:meta.preserve-line-breaks', true);
	utils.assignPathValue(modelMetaByPropertyNameMap, 'nsd.constituent-vnfd.vnf-configuration.config-template.:meta.preserve-line-breaks', true);
})();

export default {
	createModelInstanceForType(typeOrPath) {
		const modelMeta = this.getModelMetaForType(typeOrPath);
		return DescriptorModelMetaProperty.createModelInstance(modelMeta);
	},
	getModelMetaForType(typeOrPath, filterProperties = () => true) {
		// resolve paths like 'nsd' or 'vnfd.vdu' or 'nsd.constituent-vnfd'
		const found = utils.resolvePath(modelMetaByPropertyNameMap, getPathForType(typeOrPath));
		if (found) {
			const uiState = _.cloneDeep(found[':meta']);
			uiState.properties = uiState.properties.filter(filterProperties);
			return uiState;
		}
		console.warn('no model uiState found for type', typeOrPath);
	},
	getModelFieldNamesForType(typeOrPath) {
		// resolve paths like 'nsd' or 'vnfd.vdu' or 'nsd.constituent-vnfd'
		const found = utils.resolvePath(modelMetaByPropertyNameMap, getPathForType(typeOrPath));
		if (found) {
			return found[':meta'].properties.map(p => p.name);
		}
		console.warn('no model uiState found for type', typeOrPath);
	}
}
