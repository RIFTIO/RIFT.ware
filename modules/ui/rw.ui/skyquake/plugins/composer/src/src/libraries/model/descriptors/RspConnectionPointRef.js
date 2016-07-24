/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import VnfdConnectionPointRef from './VnfdConnectionPointRef'

export default class RspConnectionPointRef extends VnfdConnectionPointRef {

	static get type() {
		return 'rsp-connection-point-ref';
	}

	static get className() {
		return 'RspConnectionPointRef';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'rsp-connection-point-ref';
		this.uiState['qualified-type'] = 'nsd.vnffgd.rsp.vnfd-connection-point-ref';
		this.className = 'RspConnectionPointRef';
	}

	remove() {
		return this.parent.removeRspConnectionPointRef(this);
	}

}
