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

export default class InternalConnectionPointRef extends DescriptorModel {

	static get type() {
		return 'internal-connection-point-ref';
	}

	static get className() {
		return 'InternalConnectionPointRef';
	}

	static get qualifiedType() {
		return 'vnfd.internal-vld.' + InternalConnectionPointRef.type;
	}

	constructor(m, parent) {
		super(!m || typeof m === 'string' ? {id: m, isLeaf: true} : m, parent);
		this.uid = this.id;
		this.type = InternalConnectionPointRef.type;
		this.uiState['qualified-type'] = InternalConnectionPointRef.qualifiedType;
		this.className = InternalConnectionPointRef.className;
	}

	toString() {
		return this.valueOf();
	}

	remove() {
		return this.parent.removeInternalConnectionPointRefForId(this.id);
	}

	valueOf() {
		return this.id;
	}

}
