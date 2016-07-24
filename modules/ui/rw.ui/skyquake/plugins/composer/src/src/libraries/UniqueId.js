/**
 * Created by onvelocity on 2/12/16.
 */
'use strict';
import d3 from 'd3'
import guid from './guid'
import DescriptorModel from './model/DescriptorModel'
export default class UID {

	static get propertyName() {
		return ':uid';
	}

	static create() {
		return guid();
	}

	static from(obj) {
		if (!obj || /undefined|null/.test(obj)) {
			return;
		}
		if (obj instanceof DescriptorModel) {
			// a descriptor instance
			return obj.uid;
		}
		if (obj.dataset && obj.dataset.uid) {
			// DOM Element
			obj = obj.dataset.uid;
		} else if (obj.__data__ && obj.__data__.uid) {
			// d3 managed element
			obj = obj.__data__.uid;
		} else if (obj instanceof Element) {
			obj = d3.select(obj).attr('data-uid');
		}
		if (!obj || /undefined|null/.test(obj)) {
			return;
		}
		const uid = typeof obj === 'object' ? obj[UID.propertyName] || (obj.uiState && obj.uiState[UID.propertyName]) : obj;
		if (typeof uid === 'string') {
			return uid;
		}
	}

	static hasUniqueId(obj) {
		return !!UID.from(obj);
	}

	static assignUniqueId(obj) {
		return obj[UID.propertyName] = UID.create();
	}

}
