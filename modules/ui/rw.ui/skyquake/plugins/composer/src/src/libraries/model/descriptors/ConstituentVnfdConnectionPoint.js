/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import ConnectionPoint from './ConnectionPoint'

export default class ConstituentVnfdConnectionPoint extends ConnectionPoint {

	constructor(model, parent) {
		super(model, parent);
		this.uid = parent.uid + this.key;
	}

}
