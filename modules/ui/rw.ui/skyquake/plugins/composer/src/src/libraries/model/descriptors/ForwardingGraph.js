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
import Classifier from './Classifier'
import DescriptorModel from '../DescriptorModel'
import RecordServicePath from './RecordServicePath'
import DescriptorModelFactory from '../DescriptorModelFactory'
import DescriptorModelMetaFactory from '../DescriptorModelMetaFactory'

export default class ForwardingGraph extends DescriptorModel {

	static get type() {
		return 'vnffgd';
	}

	static get className() {
		return 'ForwardingGraph';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'vnffgd';
		this.uiState['qualified-type'] = 'nsd.vnffgd';
		this.className = 'ForwardingGraph';
	}

	get rsp() {
		if (!this.model.rsp) {
			this.model.rsp = [];
		}
		return this.model.rsp.map(d => DescriptorModelFactory.newRecordServicePath(d, this));
	}

	set rsp(obj) {
		this.updateModelList('rsp', obj, RecordServicePath);
	}

	createRsp() {
		const model = DescriptorModelMetaFactory.createModelInstanceForType('nsd.vnffgd.rsp');
		return this.rsp = new RecordServicePath(model, this);
	}

	removeRsp(obj) {
		this.children.filter(d => d.id === obj.id).forEach(rsp => rsp.remove());
	}

	get recordServicePaths() {
		return this.rsp;
	}

	removeRecordServicePath(child) {
		this.rsp = this.rsp.filter(rsp => rsp.uid !== child.uid);
		this.removeChild(child);
	}

	get classifier() {
		if (!this.model['classifier']) {
			this.model['classifier'] = [];
		}
		return this.model['classifier'].map(d => DescriptorModelFactory.newClassifier(d, this));
	}

	set classifier(obj) {
		this.updateModelList('classifier', obj, Classifier);
	}

	createClassifier(model) {
		model = model || DescriptorModelMetaFactory.createModelInstanceForType(DescriptorModelFactory.Classifier.qualifiedType);
		return this.classifier = DescriptorModelFactory.newClassifier(model, this);
	}

	removeClassifier(child) {
		return this.removeModelListItem('classifier', child);
	}

	remove() {
		if (this.parent) {
			return this.parent.removeVnffgd(this);
		}
	}

	getColor() {

	}

	createClassifierForRecordServicePath(rsp) {
		const classifier = this.createClassifier();
		classifier.model['rsp-id-ref'] = rsp.id;
	}

	removeVnfdConnectionPointRefForVnfdIndex(vnfdIndex) {
		this.rsp.forEach(rsp => rsp.removeVnfdConnectionPointRefForVnfdIndex(vnfdIndex));
	}

}

