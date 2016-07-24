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

export default class VirtualNetworkFunctionReadOnlyWrapper extends DescriptorModel {

	constructor(model, parent) {
		super(model, parent);
	}

	get vld() {
		if (!this.model['internal−vld']) {
			this.model['internal−vld'] = [];
		}
		return this.model['internal−vld'].map(d => DescriptorModelFactory.newInternalVirtualLink(d, this.parent));
	}

	get connectionPoint() {
		if (!this.model['connection-point']) {
			this.model['connection-point'] = [];
		}
		return this.model['connection-point'].map(d => DescriptorModelFactory.newConstituentVnfdConnectionPoint(d, this.parent));
	}

	get connectors() {
		return this.connectionPoint;
	}

}
