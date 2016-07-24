/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

export default class ClassifierConnectionPointRef {

	constructor(parent) {
		this.parent = parent;
	}

	get key() {
		return this.parent.model['member-vnf-index-ref'] + '/' + this.parent.model['vnfd-connection-point-ref'];
	}

	get title() {
		return this.vnfdConnectionPointName;
	}

	get vnfdId() {
		return this.parent.model['vnfd-id-ref'];
	}

	set vnfdId(id) {
		return this.parent.model['vnfd-id-ref'] = id;
	}

	get vnfdIndex() {
		return this.parent.model['member-vnf-index-ref'];
	}

	set vnfdIndex(index) {
		return this.parent.model['member-vnf-index-ref'] = index;
	}

	get vnfdConnectionPointName() {
		return this.parent.model['vnfd-connection-point-ref'];
	}

	set vnfdConnectionPointName(name) {
		return this.parent.model['vnfd-connection-point-ref'] = name;
	}

	get cpNumber() {
		return this.parent.uiState.cpNumber;
	}

	set cpNumber(n) {
		this.parent.uiState.cpNumber = n;
	}

	get vnfdRef() {
		return this.parent.uiState.vnfdRef;
	}

	set vnfdRef(n) {
		this.parent.uiState.vnfdRef = n;
	}

	get order() {
		return this.model['order'];
	}

	set order(n) {
		this.model['order'] = n;
	}

	remove() {
		return this.parent.removeVnfdConnectionPointRef();
	}

}
