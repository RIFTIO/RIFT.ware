/**
 * Created by onvelocity on 1/28/16.
 *
 * This function expects a React SyntheticEvent e.g. onClick and toggles the data-toggle element and adds the class '-is-toggled'.
 *
 * <h1 data-toggle="false" onClick={toggle}>Basic</h1>
 + <div className="toggleable">more info here</div>
 */

'use strict';

import getEventPath from './getEventPath'

import '../styles/ToggleElement.scss'


export default function toggle(event) {
	if (event.defaultPrevented) return;
	const target = getEventPath(event).reduce((r, node) => {
		if (r) {
			return r;
		}
		if (node && node.dataset && node.dataset.hasOwnProperty('toggle')) {
			return node;
		}
	}, false);
	if (target) {
		const value = String(target.dataset.toggle) !== 'true';
		target.dataset.toggle = value;
		target.classList.toggle('-is-toggled', value);
	}
}
