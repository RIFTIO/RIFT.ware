/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import _ from 'lodash'
import DescriptorModel from '../DescriptorModel'
import RspConnectionPointRef from './RspConnectionPointRef'
import DescriptorModelFactory from '../DescriptorModelFactory'
import DescriptorModelMetaFactory from '../DescriptorModelMetaFactory'

export default class RecordServicePath extends DescriptorModel {

	static get type() {
		return 'rsp';
	}

	static get className() {
		return 'RecordServicePath';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'rsp';
		this.uiState['qualified-type'] = 'nsd.vnffgd.rsp';
		this.className = 'RecordServicePath';
	}

	get vnfdConnectionPointRef() {
		if (!this.model['vnfd-connection-point-ref']) {
			this.model['vnfd-connection-point-ref'] = [];
		}
		return this.model['vnfd-connection-point-ref'].map(d => DescriptorModelFactory.newRspConnectionPointRef(d, this));
	}

	set vnfdConnectionPointRef(obj) {
		this.updateModelList('vnfd-connection-point-ref', obj, RspConnectionPointRef);
	}

	createVnfdConnectionPointRef(connectionPoint) {
		let ref;
		if (DescriptorModelFactory.isConnectionPoint(connectionPoint)) {
			ref = connectionPoint.toRspConnectionPointRef();
		} else {
			const model = DescriptorModelMetaFactory.createModelInstanceForType('nsd.vnffgd.rsp.vnfd-connection-point-ref');
			ref = new RspConnectionPointRef(model, this);
		}
		this.vnfdConnectionPointRef = ref;
		return ref;
	}

	get connectionPoints() {
		return this.vnfdConnectionPointRef;
	}
	remove() {
		return this.parent.removeRecordServicePath(this);
	}

	/**
	 * Remove all connection points refs associate with this vnfd.
	 *
	 * @param vnfdIndex
	 */
	removeVnfdConnectionPointRefForVnfdIndex(vnfdIndex) {
		if (typeof vnfdIndex !== 'undefined') {
			const len = this.vnfdConnectionPointRef.length;
			this.vnfdConnectionPointRef = this.vnfdConnectionPointRef.filter(d => d.vnfdIndex !== vnfdIndex);
			return len !== this.vnfdConnectionPointRef.length;
		}
	}

	removeRspConnectionPointRef(child) {
		return this.removeModelListItem('vnfdConnectionPointRef', child);
	}

	createClassifier() {
		this.parent.createClassifierForRecordServicePath(this);
	}

}
