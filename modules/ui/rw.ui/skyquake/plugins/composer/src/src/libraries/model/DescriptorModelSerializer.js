
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/20/15.
 */

import _ from 'lodash'
import utils from './../utils'
import DescriptorModelFields from './DescriptorModelFields'
import DescriptorModelMetaFactory from './DescriptorModelMetaFactory'

const nsdFields = DescriptorModelMetaFactory.getModelFieldNamesForType('nsd').concat('uiState');
const vldFields = DescriptorModelMetaFactory.getModelFieldNamesForType('nsd.vld');
const vnfdFields = DescriptorModelMetaFactory.getModelFieldNamesForType('vnfd').concat('uiState');
const cvnfdFields = DescriptorModelMetaFactory.getModelFieldNamesForType('nsd.constituent-vnfd');

/**
 * Serialize DescriptorModel JSON into CONFD JSON. Also, cleans up the data as needed.
 *
 * @type {{serialize: (function(*=)), ':clean': (function(*=)), nsd: {serialize: (function(*=))}, vld: {serialize: (function(*=))}, vnfd-connection-point-ref: {serialize: (function(*=))}, constituent-vnfd: {serialize: (function(*=))}, vnfd: {serialize: (function(*=))}, vdu: {serialize: (function(*=))}}}
 */
const DescriptorModelSerializer = {
	serialize(model) {
		const type = model.uiState && model.uiState.type;
		const serializer = this[type];
		if (serializer) {
			model = serializer.serialize(model);
			this[':clean'](model);
			return model;
		}
		return false;
	},
	':clean'(model) {
		// remove uiState from all elements accept nsd and vnfd
		// remove empty / blank value fields
		function clean(m) {
			Object.keys(m).forEach(k => {
				const isEmptyObject = typeof m[k] === 'object' && _.isEmpty(m[k]);
				if (typeof m[k] === 'undefined' || isEmptyObject || m[k] === '') {
					delete m[k];
				}
				const isMetaAllowed = /^nsd|vnfd$/.test(m.uiState && m.uiState.type);
				if (k === 'uiState') {
					if (isMetaAllowed) {
						// remove any transient ui state properties
						const uiState = _.pick(m.uiState, DescriptorModelFields.meta);
						if (!_.isEmpty(uiState)) {
							// uiState field must be a string
							m['meta'] = JSON.stringify(uiState);
						}
					}
					delete m[k];
				}
				if (typeof m[k] === 'object') {
					clean(m[k]);
				}
			});
		}
		clean(model);
		return model;
	},
	nsd: {
		serialize(nsdModel) {

			const confd = _.pick(nsdModel, nsdFields);

			// vnfd is defined in the ETSI etsi_gs reference manual but RIFT does not use it
			delete confd.vnfd;

			// map the vnfd instances into the CONFD constituent-vnfd ref instances
			confd['constituent-vnfd'] = confd['constituent-vnfd'].map((d, index) => {

				const constituentVNFD = {
					'member-vnf-index': d['member-vnf-index'],
					'vnfd-id-ref': d['vnfd-id-ref']
				};

				if (d['vnf-configuration']) {
					const vnfConfig = _.cloneDeep(d['vnf-configuration']);
					const configType = vnfConfig['config-type'] || 'none';
					// make sure we send the correct values based on config type
					if (configType === 'none') {
						constituentVNFD['vnf-configuration'] = {'config-type': 'none'};
						const configPriority = utils.resolvePath(vnfConfig, 'input-params.config-priority');
						const configPriorityValue = _.isNumber(configPriority) ? configPriority : d.uiState['member-vnf-index'];
						utils.assignPathValue(constituentVNFD['vnf-configuration'], 'input-params.config-priority', configPriorityValue);
					} else {
						// remove any unused configuration options
						['netconf', 'rest', 'script', 'juju'].forEach(type => {
							if (configType !== type) {
								delete vnfConfig[type];
							}
						});
						constituentVNFD['vnf-configuration'] = vnfConfig;
					}
				}

				if (d.hasOwnProperty('start-by-default')) {
					constituentVNFD['start-by-default'] = d['start-by-default'];
				}

				return constituentVNFD;

			});

			// serialize the VLD instances
			confd.vld = confd.vld.map(d => {
				return DescriptorModelSerializer.serialize(d);
			});

			return confd;

		}
	},
	vld: {
		serialize(vldModel) {
			const confd = _.pick(vldModel, vldFields);
			const property = 'vnfd-connection-point-ref';

			// TODO: There is a bug in RIFT-REST that is not accepting empty
			// strings for string properties.
			// once that is fixed, remove this piece of code.
			// fix-start
			for (var key in confd) {
				if (confd.hasOwnProperty(key) && confd[key] === '') {
					delete confd[key];
				}
			}

			const deepProperty = 'provider-network';
			for (var key in confd[deepProperty]) {
				if (confd[deepProperty].hasOwnProperty(key) && confd[deepProperty][key] === '') {
					delete confd[deepProperty][key];
				}
			}
			// fix-end


			confd[property] = confd[property].map(d => DescriptorModelSerializer[property].serialize(d));
			return confd;
		}
	},
	'vnfd-connection-point-ref': {
		serialize(ref) {
			return _.pick(ref, ['member-vnf-index-ref', 'vnfd-id-ref', 'vnfd-connection-point-ref']);
		}
	},
	'constituent-vnfd': {
		serialize(cvnfdModel) {
			return _.pick(cvnfdModel, cvnfdFields);
		}
	},
	vnfd: {
		serialize(vnfdModel) {
			const confd = _.pick(vnfdModel, vnfdFields);
			confd.vdu = confd.vdu.map(d => DescriptorModelSerializer.serialize(d));
			return confd;
		}
	},
	vdu: {
		serialize(vduModel) {
			const confd = _.omit(vduModel, ['uiState']);
			return confd;
		}
	}
};

export default DescriptorModelSerializer;
