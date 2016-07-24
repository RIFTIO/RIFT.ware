/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 1/19/16.
 *
 * The fields used to populate the 'Basic' details editor panel.
 */
const common = ['logo', 'name', 'short-name', 'description', 'vendor', 'version'];
export default {
	simpleList: ['constituent-vnfd', 'vnffgd', 'vld', 'vdu', 'internal-vld'],
	common: common.concat(),
	nsd: common.concat(['constituent-vnfd', 'vnffgd', 'vld']),
	vld: common.concat([]),
	vnfd: common.concat(['vdu', 'internal-vld']),
	'vnfd.vdu': common.concat(['image', 'external-interface', 'vm-flavor', 'cloud-init']),
	// white-list valid fields to send in the meta field
	meta: ['containerPositionMap']
};
