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

export default class ConstituentVnfd extends DescriptorModel {

	static get type() {
		return 'constituent-vnfd';
	}

	static get className() {
		return 'ConstituentVnfd';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'constituent-vnfd';
		this.uiState['qualified-type'] = 'nsd.constituent-vnfd';
		this.className = 'ConstituentVnfd';
		this.addProp('vnfdRef', DescriptorModelFactory.newVirtualNetworkFunctionReadOnlyWrapper({}, this));
	}

	get id() {
		return this.vnfdIndex;
	}

	get vld() {
		if (this.vnfdRef) {
			return this.vnfdRef.vld;
		}
		return [];
	}

	get connectionPoint() {
		if (this.vnfdRef) {
			return this.vnfdRef.connectionPoint;
		}
		return [];
	}

	get connectors() {
		return this.connectionPoint
	}

	get vnfdRef() {
		return this.getProp('vnfdRef');
	}

	set vnfdRef(ref) {
		this.addProp('vnfdRef', DescriptorModelFactory.newVirtualNetworkFunctionReadOnlyWrapper(ref, this));
		// since this vnfd reference is not part of
		// the YANG, we need to provide info needed
		// for the details editor to manipulate it.
		this.uiState.vnfdIndex = this.vnfdIndex;
		this.uiState.displayName = this.title;
	}

	get vnfdIndex() {
		return this.model['member-vnf-index'];
	}

	set vnfdIndex(index) {
		this.model['member-vnf-index'] = index;
	}

	get vnfdId() {
		return this.model['vnfd-id-ref'];
	}

	set vnfdId(id) {
		this.model['vnfd-id-ref'] = id;
	}

	get name() {
		return this.vnfdRef.name;
	}

	get 'short-name'() {
		return this.vnfdRef['short-name'];
	}

	get title() {
		return (this['short-name'] || this.name) + `/${this.vnfdIndex}`;
	}

	get vnfdServiceFunctionChain() {
		return this.vnfdRef.model['service-function-chain'] || 'UNAWARE';
	}

	remove() {
		return this.parent.removeConstituentVnfd(this);
	}

}
