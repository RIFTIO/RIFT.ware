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
import ConnectionPoint from './ConnectionPoint'
import RspConnectionPointRef from './RspConnectionPointRef'
import VnfdConnectionPointRef from './VnfdConnectionPointRef'
import DescriptorModelFactory from '../DescriptorModelFactory'

/**
 * A VirtualNetworkFunctionConnectionPoint is always a child of a VNFD. We use it to build VnfdConnectionPointRef instances. So convenience
 * methods are add to access the fields needed to do that.
 */
export default class VirtualNetworkFunctionConnectionPoint extends ConnectionPoint {

	static get type() {
		return 'connection-point';
	}

	static get className() {
		return 'VirtualNetworkFunctionConnectionPoint';
	}

	static get qualifiedType() {
		return 'vnfd.' + VirtualNetworkFunctionConnectionPoint.type;
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = VirtualNetworkFunctionConnectionPoint.type;
		this.uiState['qualified-type'] = VirtualNetworkFunctionConnectionPoint.qualifiedType;
		this.className = 'VirtualNetworkFunctionConnectionPoint';
	}

}
