/*global SVGSVGElement, SVGPathElement, SVGGElement*/
/**
 * Created by onvelocity on 2/6/16.
 */

'use strict';

import _ from 'lodash'
import d3 from 'd3'
import UID from './UniqueId'
import React from 'react'
import PathBuilder from './graph/PathBuilder'

const SELECTIONS = '::private::selections';

/**
 * SelectionManager provides two features:
 * 	1) given DescriptorModel instances mark them as 'selected'
 * 	2) given DOM elements draw an outline around them
 *
 * Note that the consumer must call addSelection(descriptor) and outlineElement(dom) separately. This allows for complex
 * selections to be separate from the outline indicator. It also allows for selection sets that do not have a visual
 * outline associated with them.
 */
class SelectionManager {

	static disableOutlineChanges() {
		SelectionManager._disableOutlineChanges = true;
	}

	static enableOutlineChanges() {
		SelectionManager._disableOutlineChanges = false;
	}

	static select(obj) {
		const isSelected = SelectionManager.isSelected(obj);
		if (!isSelected) {
			SelectionManager.clearSelectionAndRemoveOutline();
			SelectionManager.addSelection(obj);
			return true;
		}
	}

	static isSelected(obj) {
		const uid = UID.from(obj);
		if (uid) {
			return SelectionManager[SELECTIONS].has(uid);
		}
	}

	static getSelections() {
		return Array.from(SelectionManager[SELECTIONS]).filter(d => d);
	}

	static addSelection(obj) {
		const uid = UID.from(obj);
		if (uid) {
			SelectionManager[SELECTIONS].add(uid);
		}
	}

	static updateSelection(container, addOrRemove = true) {
		if (addOrRemove) {
			SelectionManager.addSelection(container);
		} else {
			SelectionManager.removeSelection(container);
		}
	}

	static removeSelection(obj) {
		const uid = UID.from(obj);
		if (uid) {
			SelectionManager[SELECTIONS].delete(uid);
		}
	}

	static get onClearSelection() {
		return this._onClearSelection;
	}

	static set onClearSelection(callback) {
		if (!this._onClearSelection) {
			this._onClearSelection = new Set();
		}
		if (typeof callback !== 'function') {
			throw new TypeError('onClearSelection only takes functions');
		}
		this._onClearSelection.add(callback);
	}

	static clearSelectionAndRemoveOutline() {
		SelectionManager.removeOutline();
		const unselected = SelectionManager.getSelections();
		if (unselected.length) {
			SelectionManager[SELECTIONS].clear();
			if (SelectionManager.onClearSelection) {
				SelectionManager.onClearSelection.forEach(callback => callback(unselected));
			}
		}
		return unselected;
	}

	static removeOutline() {
		Array.from(document.querySelectorAll('[data-outline-indicator-outline]')).forEach(n => d3.select(n).remove());
	}

	static refreshOutline() {
		clearTimeout(SelectionManager.timeoutId);
		SelectionManager.moveOutline();
		SelectionManager.timeoutId = setTimeout(() => {
			// note the timeout is to wait for the react digest to complete
			// in the case this method is called from an Alt action....
			SelectionManager.moveOutline();
		}, 100);
	}

	static moveOutline() {
		SelectionManager.getSelections().forEach(uid => {
			Array.from(document.body.querySelectorAll(`[data-uid="${uid}"]`)).forEach(SelectionManager.outline.bind(this));
		});
	}

	static outline(dom) {

		const elements = Array.isArray(dom) ? dom : [dom];

		elements.map(SelectionManager.getClosestElementWithUID).filter(d => d).forEach(element => {

			if (element instanceof SVGElement) {
				SelectionManager.outlineSvg(element);
			} else {
				SelectionManager.outlineDom(element);
			}

		});

	}

	static outlineSvg(element) {

		if (SelectionManager._disableOutlineChanges) {
			return
		}

		const svg = element.viewportElement;

		if (!svg) {
			return
		}

		const path = new PathBuilder();

		const dim = element.getBBox();
		const adjustPos = SelectionManager.sumTranslates(element);

		let w = dim.width + 11;
		let h = dim.height + 11;
		let top = adjustPos[1] - 8 + dim.y;
		let left = adjustPos[0] - 8 + dim.x;

		let square = path.M(5, 5).L(w, 5).L(w, h).L(5, h).L(5, 5).Z.toString();

		let border = element;

		// strategy copy the element to use as a foreground mask
		// - this allows the item to appear above other items and
		// - it makes the outline apear outside the element

		const mask = d3.select(element.cloneNode(true));

		if (border instanceof SVGPathElement) {
			const t = d3.transform(d3.select(element).attr('transform')).translate;
			square = d3.select(element).attr('d');
			top = t[1];
			left = t[0];
		} else if (border instanceof SVGGElement) {
			const t = d3.transform(d3.select(element).attr('transform')).translate;
			border = d3.select(element).select('path.border');
			square = border.attr('d');
			top = t[1];
			left = t[0];
		}


		const data = {
			top: top,
			left: left,
			path: square
		};

		const indicator = svg.querySelector(['[data-outline-indicator]']) || svg;

		const outline = d3.select(indicator).selectAll('[data-outline-indicator-outline]').data([data]);

		outline.exit().remove();

		// move to top of z-order
		element.parentNode.appendChild(element);

		outline.enter().append('g').attr({
			'data-outline-indicator-outline': true,
			'class': '-with-transitions'
		}).style({
			'pointer-events': 'none'
		}).append('g');

		outline.attr({
			transform: d => `translate(${d.left}, ${d.top})`
		});

		outline.select('g').append('path').attr({
			'data-outline-indicator-svg-outline-path': true,
			'stroke': 'red',
			'fill': 'transparent',
			'fill-rule': 'evenodd',
			'stroke-width': 5,
			'stroke-linejoin': 'round',
			'stroke-dasharray': '4',
			'opacity': 1,
			d: d => d.path
		}).transition().style({'stroke-width': 15});

		const g = outline.select('g').node();
		const children = Array.from(mask.node().childNodes);
		if (children.length) {

			children.forEach(child => {
				g.appendChild(child);
			});

			// ensure the outline moves with the item during drag operations
			const observer = new MutationObserver(function(mutations) {
				mutations.forEach(function(mutation) {
					const transform = d3.select(mutation.target).style('opacity', 0).attr('transform');
					outline.attr({
						transform: transform
					});
				});
			});

			const config = {attributes: true, attributeOldValue: true, attributeFilter: ['transform']};

			observer.observe(element, config);

		} else {

			if (mask.classed('connection')) {
				element.parentNode.appendChild(outline.node());
				element.parentNode.appendChild(element);
			} else {
				//mask.attr('transform', null);
				element.parentNode.appendChild(outline.node());
				element.parentNode.appendChild(element);
			}

		}

	}

	static outlineDom(element) {

		if (SelectionManager._disableOutlineChanges) {
			return
		}

		element = SelectionManager.getClosestElementWithUID(element);

		const offsetParent = SelectionManager.offsetParent(element);

		const dim = SelectionManager.offsetDimensions(element);
		const w = dim.width + 11;
		const h = dim.height + 11;


		const path = new PathBuilder();
		const square = path.M(5, 5).L(w, 5).L(w, h).L(5, h).L(5, 5).Z.toString();

		const parentDim = SelectionManager.offsetDimensions(offsetParent);

		const top = dim.top - parentDim.top;
		const left = dim.left - parentDim.left;
		const svg = d3.select(offsetParent).selectAll('[data-outline-indicator-outline]').data([{}]);
		svg.enter().append('svg').attr({
			'data-outline-indicator-outline': true,
			width: parentDim.width + 20,
			height: parentDim.height + 23,
			style: `pointer-events: none; position: absolute; z-index: 999; top: ${top - 8}px; left: ${left - 8}px; background: transparent;`
		}).append('g').append('path').attr({
			'data-outline-indicator-dom-outline-path': true,
			'stroke': 'red',
			'fill': 'transparent',
			'stroke-width': '1.5px',
			'stroke-linejoin': 'round',
			'stroke-dasharray': '4',
			d: square
		});

		svg.select('svg').attr({
			width: parentDim.width + 20,
			height: parentDim.height + 23,
		}).select('path').attr({
			d: square
		});

		svg.exit().remove();

	}

	static getClosestElementWithUID(element) {
		let target = element;
		while (target) {
			if (UID.from(target)) {
				return target;
			}
			if (target === target.parentNode) {
				return;
			}
			target = target.parentNode;
		}
	}

	static offsetParent(target) {
		while (target) {
			if ((d3.select(target).attr('data-offset-parent')) || target.nodeName === 'BODY') {
				return target;
			}
			target = target.parentNode;
		}
		return document.body;
	}

	/**
	 * Util for determining the widest child of an offsetParent that is scrolled.
	 *
	 * @param offsetParent
	 * @returns {{top: Number, right: Number, bottom: Number, left: Number, height: Number, width: Number}}
	 */
	static offsetDimensions (offsetParent) {

		const elementDim = offsetParent.getBoundingClientRect();
		const dim = {
			top: elementDim.top,
			right: elementDim.right,
			bottom: elementDim.bottom,
			left: elementDim.left,
			height: elementDim.height,
			width: elementDim.width
		};

		const overrideWidth = Array.from(offsetParent.querySelectorAll('[data-offset-width]'));
		dim.width = overrideWidth.reduce((width, child) => {
			const dim = child.getBoundingClientRect();
			return Math.max(width, dim.width);
		}, elementDim.width);

		return dim;

	}

	static sumTranslates(element) {
		let parent = element;
		const result = [0, 0];
		while (parent) {
			const t = d3.transform(d3.select(parent).attr('transform')).translate;
			result[0] += t[0];
			result[1] += t[1];
			if (!parent.parentNode || /svg/i.test(parent.nodeName) || parent === parent.parentNode) {
				return result;
			}
			parent = parent.parentNode;
		}
		return result;
	}

}

// warn static variable to store all selections across instances
SelectionManager[SELECTIONS] = SelectionManager[SELECTIONS] || new Set();

export default SelectionManager;
