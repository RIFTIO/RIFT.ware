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

export default class InternalConnectionPoint extends DescriptorModel {

	static get type() {
		return 'internal-connection-point';
	}

	static get className() {
		return 'InternalConnectionPoint';
	}

	static get qualifiedType() {
		return 'vnfd.vdu.' + InternalConnectionPoint.type;
	}

	constructor(model, parent) {
		super(model, parent);
		// evil type collides with the YANG property 'type' for this object
		this.type = 'internal-connection-point';
		this.uiState['qualified-type'] = InternalConnectionPoint.qualifiedType;
		this.className = 'InternalConnectionPoint';
		this.location = 'bottom-left';
		this.position = new Position(parent.position.value());
		this.position.top = parent.position.bottom;
		this.position.width = 14;
		this.position.height = 14;
	}

	get key() {
		return this.id;
	}

	toInternalConnectionPointRef() {
		return DescriptorModelFactory.newInternalConnectionPointRef(this.id);
	}

	canConnectTo(obj) {
		return DescriptorModelFactory.isInternalVirtualLink(obj) || (DescriptorModelFactory.isInternalConnectionPoint(obj) && obj.parent !== this.parent);
	}

	remove() {
		return this.parent.removeInternalConnectionPoint(this);
	}

}
