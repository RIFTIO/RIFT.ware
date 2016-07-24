/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import Position from '../../graph/Position'
import DescriptorModel from '../DescriptorModel'
import DescriptorModelFactory from '../DescriptorModelFactory'
import DescriptorModelMetaFactory from '../DescriptorModelMetaFactory'
import ClassifierConnectionPointRef from './ClassifierConnectionPointRef'

/**
 * A VirtualNetworkFunctionConnectionPoint is always a child of a VNFD. We use it to build VnfdConnectionPointRef instances. So convenience
 * methods are add to access the fields needed to do that.
 */
export default class Classifier extends DescriptorModel {

	static get type() {
		return 'classifier';
	}

	static get className() {
		return 'Classifier';
	}

	static get qualifiedType() {
		return 'nsd.vnffgd.' + Classifier.type;
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = Classifier.type;
		this.uiState['qualified-type'] = Classifier.qualifiedType;
		this.className = Classifier.className;
		this.position = new Position();
		this._vnfdConnectionPointRef = new ClassifierConnectionPointRef(this);
	}

	get matchAttributes() {
		if (!this.model['match-attributes']) {
			this.model['match-attributes'] = {};
		}
		return this.model['match-attributes'].map(d => DescriptorModelFactory.newClassifierMatchAttributes(d, this));
	}

	set matchAttributes(obj) {
		return this.updateModelList('match-attributes', obj, DescriptorModelFactory.ClassifierMatchAttributes);
	}

	createMatchAttributes() {
		const model = DescriptorModelMetaFactory.createModelInstanceForType(DescriptorModelFactory.ClassifierMatchAttributes.qualifiedType);
		return this.matchAttributes = DescriptorModelFactory.newClassifierMatchAttributes(model, this);
	}

	removeMatchAttributes(matchAttributes) {
		return this.removeModelListItem('matchAttributes', matchAttributes);
	}

	get vnfdConnectionPointRef() {
		return this._vnfdConnectionPointRef || {};
	}

	// vnfdConnectionPointRef is read only by design

	addVnfdConnectionPoint(cp) {
		this.vnfdConnectionPointRef.vnfdRef = cp.parent.vnfdRef;
		this.vnfdConnectionPointRef.vnfdId = cp.vnfdId;
		this.vnfdConnectionPointRef.vnfdIndex = cp.vnfdIndex;
		this.vnfdConnectionPointRef.vnfdConnectionPointName = cp.name;
		this.vnfdConnectionPointRef.cpNumber = cp.cpNumber;
	}

	remove() {
		return this.parent.removeClassifier(this);
	}

}
