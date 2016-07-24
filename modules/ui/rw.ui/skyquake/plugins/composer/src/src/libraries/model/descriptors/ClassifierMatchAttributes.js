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
import RspConnectionPointRef from './RspConnectionPointRef'
import VnfdConnectionPointRef from './VnfdConnectionPointRef'

/**
 * A VirtualNetworkFunctionConnectionPoint is always a child of a VNFD. We use it to build VnfdConnectionPointRef instances. So convenience
 * methods are add to access the fields needed to do that.
 */
export default class ClassifierMatchAttributes extends DescriptorModel {

	static get type() {
		return 'match-attributes';
	}

	static get className() {
		return 'ClassifierMatchAttributes';
	}

	static get qualifiedType() {
		return 'nsd.vnffgd.classifier.' + ClassifierMatchAttributes.type;
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = ClassifierMatchAttributes.type;
		this.uiState['qualified-type'] = ClassifierMatchAttributes.qualifiedType;
		this.className = ClassifierMatchAttributes.className;
		this.position = new Position();
	}

	getFieldValue(name) {
		return this.model[name];
	}

	setFieldValue(name, value) {
		this.model[name] = value;
	}

	remove() {
		return this.parent.removeMatchAttributes(this);
	}

}
