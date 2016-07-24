
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';
const TEMPLATE = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx';
export default function guid(len = TEMPLATE.length) {
	// http://byronsalau.com/blog/how-to-create-a-guid-uuid-in-javascript/
	return TEMPLATE.replace(/[xy]/g, function (c) {
		var r = Math.random() * 16 | 0, v = c === 'x' ? r : (r & 0x3 | 0x8);
		return v.toString(16);
	}).substring(0, len);
}
