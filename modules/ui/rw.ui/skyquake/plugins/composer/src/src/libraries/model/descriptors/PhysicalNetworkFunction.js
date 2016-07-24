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

export default class PhysicalNetworkFunction extends DescriptorModel {

	static get type() {
		return 'pnfd';
	}

	static get className() {
		return 'PhysicalNetworkFunction';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'pnfd';
		this.className = 'PhysicalNetworkFunction';
	}

	get connectionPoint() {
		if (this.model && this.model['connection-points']) {
			return this.model['connection-points'];
		}
		return [];
	}

}
