/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import utils from '../../utils'
import DescriptorModel from '../DescriptorModel'
import DescriptorModelFactory from '../DescriptorModelFactory'
import DescriptorModelMetaFactory from '../DescriptorModelMetaFactory'

export default class VirtualNetworkFunction extends DescriptorModel {

	static get type() {
		return 'vnfd';
	}

	static get className() {
		return 'VirtualNetworkFunction';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'vnfd';
		this.className = 'VirtualNetworkFunction';
	}

	get vdu() {
		const list = this.model.vdu || (this.model.vdu = []);
		return list.map(d => DescriptorModelFactory.newVirtualDeploymentUnit(d, this));
	}

	set vdu(obj) {
		this.updateModelList('vdu', obj, DescriptorModelFactory.VirtualDeploymentUnit);
	}

	createVdu() {
		const model = DescriptorModelMetaFactory.createModelInstanceForType('vnfd.vdu');
		return this.vdu = DescriptorModelFactory.newVirtualDeploymentUnit(model, this);
	}

	removeVirtualDeploymentUnit(vdu) {
		vdu.connectors.forEach(cp => this.removeAnyConnectionsForConnector(cp));
		return this.vdu = this.vdu.filter(d => d.id !== vdu.id);
	}

	get vld() {
		const list = this.model['internal-vld'] || (this.model['internal-vld'] = []);
		return list.map(d => DescriptorModelFactory.newInternalVirtualLink(d, this));
	}

	set vld(obj) {
		this.updateModelList('internal-vld', obj, DescriptorModelFactory.InternalVirtualLink);
	}

	createVld() {
		const model = DescriptorModelMetaFactory.createModelInstanceForType('vnfd.internal-vld');
		return this.vld = DescriptorModelFactory.newInternalVirtualLink(model, this);
	}

	/**
	 * @deprecated use `removeInternalVirtualLink()`
	 * @param vld
	 * @returns {*}
	 */
	removeVld(vld) {
		return this.removeModelListItem('vld', vld);
	}

	get connectionPoint() {
		const list = this.model['connection-point'] || (this.model['connection-point'] = []);
		return list.map(d => DescriptorModelFactory.newVirtualNetworkFunctionConnectionPoint(d, this));
	}

	set connectionPoint(obj) {
		return this.updateModelList('connection-point', obj, DescriptorModelFactory.VirtualNetworkFunctionConnectionPoint);
	}

	removeConnectionPoint(cp) {
		return this.removeModelListItem('connectionPoint', cp);
	}

	get connectors() {
		return this.connectionPoint;
	}

	remove() {
		if (this.parent) {
			this.parent.removeConstituentVnfd(this);
			this.connectors.forEach(cpc => this.parent.removeAnyConnectionsForConnector(cpc));
		}
	}

	removeAnyConnectionsForConnector(cpc) {
		this.vld.forEach(vld => vld.removeInternalConnectionPointRefForId(cpc.id));
	}

	removeInternalVirtualLink(ivl) {
		return this.removeModelListItem('vld', ivl);
	}

}

