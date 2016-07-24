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
import InternalConnectionPoint from './InternalConnectionPoint'
import RspConnectionPointRef from './RspConnectionPointRef'
import VnfdConnectionPointRef from './VnfdConnectionPointRef'
import DescriptorModelFactory from '../DescriptorModelFactory'

/**
 * A VirtualNetworkFunctionConnectionPoint is always a child of a VNFD. We use it to build VnfdConnectionPointRef instances. So convenience
 * methods are add to access the fields needed to do that.
 */
export default class VirtualDeploymentUnitInternalConnectionPoint extends InternalConnectionPoint {

	static get type() {
		return 'internal-connection-point';
	}

	static get className() {
		return 'VirtualDeploymentUnitInternalConnectionPoint';
	}

	static get qualifiedType() {
		return 'vnfd.vdu.' + VirtualDeploymentUnitInternalConnectionPoint.type;
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = VirtualDeploymentUnitInternalConnectionPoint.type;
		this.uiState['qualified-type'] = VirtualDeploymentUnitInternalConnectionPoint.qualifiedType;
		this.className = 'VirtualNetworkFunctionConnectionPoint';
	}

	remove() {
		return this.parent.removeInternalConnectionPoint(this);
	}

}
