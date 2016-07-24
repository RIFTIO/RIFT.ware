/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

'use strict';

import changeCase from 'change-case';

export default {
	addAuthorizationStub(xhr) {
		xhr.setRequestHeader('Authorization', 'Basic YWRtaW46YWRtaW4=');
	},
	getSearchParams(url) {
		var a = document.createElement('a');
		a.href = url;
		var params = {};
		var items = a.search.replace('?', '').split('&');
		for (var i = 0; i < items.length; i++) {
			if (items[i].length > 0) {
				var key_value = items[i].split('=');
				params[key_value[0]] = decodeURIComponent(key_value[1]);
			}
		}
		return params;
	},
	parseJSONIgnoreErrors(txt) {
		try {
			return JSON.parse(txt);
		} catch (ignore) {
			return {};
		}
	},
	resolvePath(obj, path) {
		// supports a.b, a[1] and foo[bar], etc.
		// where obj is ['nope', 'yes', {a: {b: 1}, foo: 2}]
		// then [1] returns 'yes'; [2].a.b returns 1; [2].a[foo] returns 2;
		path = path.split(/[\.\[\]]/).filter(d => d);
		return path.reduce((r, p) => {
			if (r) {
				return r[p];
			}
		}, obj);
	},
	assignPathValue(obj, path, value) {
		path = path.split(/[\.\[\]]/).filter(d => d);
		// enable look-ahead to determine if type is array or object
		const pathCopy = path.slice();
		// last item in path used to assign value on the resolved object
		const name = path.pop();
		const resolvedObj = path.reduce((r, p, i) => {
			if (typeof(r[p]) !== 'object') {
				// look-ahead to see if next path item is a number
				const isArray = !isNaN(parseInt(pathCopy[i + 1], 10));
				r[p] = isArray ? [] : {}
			}
			return r[p];
		}, obj);
		resolvedObj[name] = value;
	},
	updatePathValue(obj, path, value) {
		// todo: replace implementation of assignPathValue with this impl and
		// remove updatePathValue (only need one function, not both)
		// same as assignPathValue except removes property if value is undefined
		path = path.split(/[\.\[\]]/).filter(d => d);
		const isRemove = typeof value === 'undefined';
		// enable look-ahead to determine if type is array or object
		const pathCopy = path.slice();
		// last item in path used to assign value on the resolved object
		const name = path.pop();
		const resolvedObj = path.reduce((r, p, i) => {
			// look-ahead to see if next path item is a number
			const index = parseInt(pathCopy[i + 1], 10);
			const isArray = !isNaN(index);
			if (typeof(r[p]) !== 'object') {
				r[p] = isArray ? [] : {}
			}
			if (isRemove && ((i + 1) === path.length)) {
				if (isArray) {
					r[p] = r[p].filter((d, i) => i !== index);
				} else {
					delete r[p][name];
				}
			}
			return r[p];
		}, obj);
		if (!isRemove) {
			resolvedObj[name] = value;
		}
	},
	removePathValue(obj, path) {
		// note updatePathValue removes value if third argument is undefined
		return this.updatePathValue(obj, path);
	},

	suffixAsInteger: (field) => {
		return (obj) =>{
			const str = String(obj[field]);
			const value = str.replace(str.replace(/[\d]+$/, ''), '');
			return 1 + parseInt(value, 10) || 0;
		};
	},

	toBiggestValue: (maxIndex, curIndex) => Math.max(maxIndex, curIndex)

}
