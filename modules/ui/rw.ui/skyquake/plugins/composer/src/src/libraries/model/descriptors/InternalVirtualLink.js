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

export default class InternalVirtualLink extends DescriptorModel {

	static get type() {
		return 'internal-vld';
	}

	static get className() {
		return 'InternalVirtualLink';
	}

	static get qualifiedType() {
		return 'vnfd.' + InternalVirtualLink.type;
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = InternalVirtualLink.type;
		this.uiState['qualified-type'] = InternalVirtualLink.qualifiedType;
		this.className = InternalVirtualLink.className;
	}

	get title() {
		return super.title || (this.type + '/' + this.id);
	}

	get connection() {
		const list = this.model['internal-connection-point-ref'] || (this.model['internal-connection-point-ref'] = []);
		return list.map(d => DescriptorModelFactory.newInternalConnectionPointRef(d, this));
	}

	set connection(connections) {
		return this.updateModelList('internal-connection-point-ref', connections, DescriptorModelFactory.InternalConnectionPointRef);
	}

	addConnectionPoint(icp) {
		icp.model['internal-vld-ref'] = this.id;
		this.parent.removeAnyConnectionsForConnector(icp);
		this.connection = icp.toInternalConnectionPointRef();
	}

	removeInternalConnectionPointRefForId(id) {
		return this.connection = this.connection.filter(d => d.id !== id).map(d => d.id);
	}

	remove() {
		return this.parent.removeInternalVirtualLink(this);
	}

}
