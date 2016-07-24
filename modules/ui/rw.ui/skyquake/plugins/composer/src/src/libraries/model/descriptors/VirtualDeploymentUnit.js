/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import DescriptorModel from '../DescriptorModel'
import DescriptorModelFactory from '../DescriptorModelFactory'

export default class VirtualDeploymentUnit extends DescriptorModel {

	static get type() {
		return 'vdu';
	}

	static get className() {
		return 'VirtualDeploymentUnit';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'vdu';
		this.uiState['qualified-type'] = 'vnfd.vdu';
		this.className = 'VirtualDeploymentUnit';
	}

	get key() {
		return this.parent.key + '/' + super.key;
	}

	get connectionPoint() {
		const list = this.model['internal-connection-point'] || (this.model['internal-connection-point'] = []);
		return list.map(d => DescriptorModelFactory.newVirtualDeploymentUnitInternalConnectionPoint(d, this));
	}

	set connectionPoint(obj) {
		return this.updateModelList('internal-connection-point', obj, DescriptorModelFactory.VirtualDeploymentUnitInternalConnectionPoint);
	}

	//get internalConnectionPoint() {
	//	//https://trello.com/c/ZOyKQd3z/122-hide-lines-representing-interface-connection-point-references-both-internal-and-external-interfaces
	//	const vduc = this;
	//	if (this.model && this.model['internal-interface']) {
	//		const icpMap = this.model['internal-connection-point'].reduce((r, d) => {
	//			r[d.id] = d;
	//			return r;
	//		}, {});
	//		return this.model['internal-interface'].reduce((result, internalIfc) => {
	//			const id = internalIfc['vdu-internal-connection-point-ref'];
	//			const keyPrefix = vduc.parent ? vduc.parent.key + '/' : '';
	//			if (icpMap[id]) {
	//				const icp = Object.assign({}, icpMap[id], {
	//					key: keyPrefix + id,
	//					name: internalIfc.name,
	//					'virtual-interface': internalIfc['virtual-interface']
	//				});
	//				result.push(icp);
	//			}
	//			return result;
	//		}, []);
	//	}
	//	return [];
	//}

	removeInternalConnectionPoint(icp) {
		this.parent.removeAnyConnectionsForConnector(icp);
		return this.removeModelListItem('connectionPoint', icp);
	}

	//get externalConnectionPoint() {
	//	// https://trello.com/c/ZOyKQd3z/122-hide-lines-representing-interface-connection-point-references-both-internal-and-external-interfaces
	//	//const vduc = this;
	//	//if (vduc.model && vduc.model['external-interface']) {
	//	//	return vduc.model['external-interface'].reduce((result, externalIfc) => {
	//	//		const id = externalIfc['vnfd-connection-point-ref'];
	//	//		const keyPrefix = vduc.parent ? vduc.parent.key + '/' : '';
	//	//		result.push({id: id, key: keyPrefix + id});
	//	//		return result;
	//	//	}, []);
	//	//}
	//	return [];
	//}
	//
	get connectors() {
		return this.connectionPoint;
	}

	//get connection() {
	//	return this.externalConnectionPoint;
	//}

	remove() {
		return this.parent.removeVirtualDeploymentUnit(this);
	}

}

