
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';
export default function zoomFactor(element = document.body) {
	let factor;
	const rect = element.getBoundingClientRect();
	const physicalW = rect.right - rect.left;
	const logicalW = element.offsetWidth;
	factor = Math.round((physicalW / logicalW) * 100) / 100;
	// if fullscreen mode offsetWidth is 0
	if (isNaN(factor)) {
		factor = 1;
	}
	return factor;
}
