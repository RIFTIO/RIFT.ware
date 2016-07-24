
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/9/15.
 */
'use strict';

import getZoomFactor from './zoomFactor'
import '../styles/ResizableManager.scss'

const sideComplement = {
	left: 'right',
	right: 'left',
	top: 'bottom',
	bottom: 'top'
};

function zoneName(sides) {
	return sides.sort((a, b) => {
		if (!a) {
			return 1;
		}
		if (!b) {
			return 1;
		}
		if (a.side === 'top' || a.side === 'bottom') {
			return -1;
		}
		return 1;
	}).join('-');
}

const resizeDragZones = {
	top (position, x, y, resizeDragZoneWidth) {
		const adj = y > 0 && y < document.body.clientHeight;
		const pos = position.top - (adj ? resizeDragZoneWidth / 2 : 0);
		if (Math.abs(y - pos) < resizeDragZoneWidth) {
			return 'top';
		}
	},
	bottom (position, x, y, resizeDragZoneWidth) {
		const adj = y > 0 && y < document.body.clientHeight;
		const pos = position.bottom - (adj ? resizeDragZoneWidth / 2 : 0);
		if (Math.abs(y - pos) < resizeDragZoneWidth) {
			return 'bottom';
		}
	},
	right (position, x, y, resizeDragZoneWidth) {
		const adj = x > 0 && x < document.body.clientWidth;
		const pos = position.right - (adj ? resizeDragZoneWidth / 2 : 0);
		if (Math.abs(x - pos) < resizeDragZoneWidth) {
			return 'right';
		}
	},
	left (position, x, y, resizeDragZoneWidth) {
		const adj = x > 0 && x < document.body.clientWidth;
		const pos = position.left - (adj ? resizeDragZoneWidth / 2 : 0);
		if (Math.abs(x - pos) < resizeDragZoneWidth) {
			return 'left';
		}
	},
	outside (position, x, y, resizeDragZoneWidth) {
		if (x < (position.left - resizeDragZoneWidth) || x > (position.right + resizeDragZoneWidth) || y < (position.top - resizeDragZoneWidth) || y > (position.bottom + resizeDragZoneWidth)) {
			return 'outside';
		}
	}
};

const resizeDragLimitForSide = {
	top (position, x, y, resizeDragZoneWidth, resizing) {
		// y must be inside the position coordinates
		const adj = y > 0 && y < document.body.clientHeight;
		const top = position.top - (adj ? resizeDragZoneWidth / 2 : 0);
		const bottom = position.bottom + (adj ? resizeDragZoneWidth / 2 : 0);
		return y > top && y < bottom;
	},
	bottom (position, x, y, resizeDragZoneWidth) {
		return this.top(position, x, y, resizeDragZoneWidth);
	},
	right (position, x, y, resizeDragZoneWidth) {
		const adj = x > 0 && x < document.body.clientWidth;
		const left = position.left - (adj ? resizeDragZoneWidth / 2 : 0);
		const right = position.right + (adj ? resizeDragZoneWidth / 2 : 0);
		return x > left && x < right;
	},
	left (position, x, y, resizeDragZoneWidth) {
		return this.right(position, x, y, resizeDragZoneWidth);
	},
	limit_top (position, x, y, resizeDragZoneWidth, resizing) {
		// must be outside the position coordinates
		const limit = {
			top: 0,
			right: position.right,
			bottom: position.bottom,
			left: position.left
		};
		return this.top(limit, x, y, resizeDragZoneWidth, resizing);
	},
	limit_bottom (position, x, y, resizeDragZoneWidth, resizing) {
		const limit = {
			top: position.top,
			right: position.right,
			bottom:0,
			left: position.left
		};
		return this.bottom(limit, x, y, resizeDragZoneWidth, resizing);
	},
	limit_right (position, x, y, resizeDragZoneWidth, resizing) {
		const limit = {
			top: position.top,
			right: 0,
			bottom: position.bottom,
			left: position.left
		};
		return this.right(position, x, y, resizeDragZoneWidth, resizing);
	},
	limit_left (position, x, y, resizeDragZoneWidth, resizing) {
		const limit = {
			top: position.top,
			right: position.right,
			bottom: position.bottom,
			left: 0
		};
		return this.left(position, x, y, resizeDragZoneWidth, resizing);
	}
};

function createEvent(e, eventName = 'resize') {
	const lastX = this.resizing.lastX || 0;
	const lastY = this.resizing.lastY || 0;
	const zoomFactor = getZoomFactor();
	this.resizing.lastX = (e.clientX / zoomFactor);
	this.resizing.lastY = (e.clientY / zoomFactor);
	const data = {
		bubbles: true,
		cancelable: true,
		detail: {
			x: e.clientX / zoomFactor,
			y: e.clientY / zoomFactor,
			side: this.resizing.side,
			start: {x: this.resizing.startX, y: this.resizing.startY},
			moved: {
				x: lastX - (e.clientX / zoomFactor),
				y: lastY - (e.clientY / zoomFactor)
			},
			target: this.resizing.target,
			originalEvent: e
		}
	};
	if (window.CustomEvent.prototype.initCustomEvent) {
		// support IE
		var evt = document.createEvent('CustomEvent');
		evt.initCustomEvent(eventName, data.bubbles, data.cancelable, data.detail);
		return evt;
	}
	return new CustomEvent(eventName, data);
}

const defaultHandleOffset = [0, 0, 0, 0]; // top, right, bottom, left

class ResizableManager {

	constructor(target = document, dragZones = resizeDragZones) {
		this.target = target;
		this.resizing = null;
		this.lastResizable = null;
		this.activeResizable = null;
		this.resizeDragZones = dragZones;
		this.defaultDragZoneWidth = 5;
		this.isPaused = false;
		this.addAllEventListeners();
	}

	pause() {
		this.isPaused = true;
		this.removeAllEventListeners();
	}

	resume() {
		if (this.isPaused) {
			this.addAllEventListeners();
			this.isPaused = false;
		}
	}

	static isResizing() {
		return document.body.classList.contains('resizing');
	}

	addAllEventListeners() {
		this.removeAllEventListeners();
		this.mouseout = this.mouseout.bind(this);
		this.mousedown = this.mousedown.bind(this);
		this.dispatchResizeStop = this.dispatchResizeStop.bind(this);
		this.dispatchResize = this.dispatchResize.bind(this);
		this.updateResizeCursor = this.updateResizeCursor.bind(this);
		this.target.addEventListener('mousemove', this.updateResizeCursor, true);
		this.target.addEventListener('mousedown', this.mousedown, true);
		this.target.addEventListener('mousemove', this.dispatchResize, true);
		this.target.addEventListener('mouseup', this.dispatchResizeStop, true);
		this.target.addEventListener('mouseout', this.mouseout, true);
	}

	removeAllEventListeners() {
		this.target.removeEventListener('mousemove', this.updateResizeCursor, true);
		this.target.removeEventListener('mousedown', this.mousedown, true);
		this.target.removeEventListener('mousemove', this.dispatchResize, true);
		this.target.removeEventListener('mouseup', this.dispatchResizeStop, true);
		this.target.removeEventListener('mouseout', this.mouseout, true);
	}

	makeResizableActive(d) {
		this.activeResizable = d;
	}

	clearActiveResizable() {
		this.activeResizable = null;
	}

	mouseout(e) {
		if (this.resizing && (e.clientX > window.innerWidth || e.clientX < 0 || e.clientY < 0 || e.clientY > window.innerHeight)) {
			this.dispatchResizeStop(e);
		}
	}

	mousedown(e) {
		if (!this.resizing && this.activeResizable) {
			this.resizing = this.activeResizable;
			const zoomFactor = getZoomFactor();
			this.resizing.startX = e.clientX / zoomFactor;
			this.resizing.startY = e.clientY / zoomFactor;
			e.preventDefault();
			this.dispatchResizeStart(e);
		}
	}

	dispatchResize(e) {
		if (this.resizing && !this.resizeLimitReached) {
			e.preventDefault();
			const resizeEvent = createEvent.call(this, e, 'resize');
			this.resizing.target.dispatchEvent(resizeEvent)
		}
	}

	dispatchResizeStart(e) {
		if (this.resizing) {
			document.body.classList.add('resizing');
			e.preventDefault();
			const resizeEvent = createEvent.call(this, e, 'resize-start');
			this.resizing.target.dispatchEvent(resizeEvent)
		}
	}

	dispatchResizeStop(e) {
		if (this.resizing) {
			document.body.classList.remove('resizing');
			e.preventDefault();
			const resizeEvent = createEvent.call(this, e, 'resize-stop');
			this.resizing.target.dispatchEvent(resizeEvent);
			this.lastResizable = this.resizing;
		} else {
			this.lastResizable = this.activeResizable;
		}
		this.clearActiveResizable();
		this.resizing = null;
	}

	isResizing() {
		return this.activeResizable && this.resizing;
	}

	updateResizeCursor(e) {
		if (e.defaultPrevented) {
			if (this.activeResizable) {
				document.body.style.cursor = '';
				this.clearActiveResizable();
				this.dispatchResizeStop(e);
			}
			return;
		}
		const zoomFactor = getZoomFactor();
		const x = e.clientX / zoomFactor;
		const y = e.clientY / zoomFactor;
		const resizables = Array.from(document.querySelectorAll('[resizable], [data-resizable]'));
		if (this.isResizing()) {
			const others = resizables.filter(d => d !== this.activeResizable.target);
			this.resizeLimitReached = this.checkResizeLimitReached(x, y, others);
			return;
		}
		if (this.lastResizable) {
			resizables.unshift(this.lastResizable.target)
		}
		let resizable = null;
		for (resizable of resizables) {
			const result = this.isCoordinatesOnDragZone(x, y, resizable);
			if (result.side === 'inside' || result.side === 'outside') {
				this.clearActiveResizable();
				// todo IE cursor flickers - now sure why might need to reduce the amount of classList updates
				document.body.classList.remove('-is-show-resize-cursor-col-resize');
				document.body.classList.remove('-is-show-resize-cursor-row-resize');
				document.body.classList.remove('-is-show-resize-cursor-nesw-resize');
				document.body.classList.remove('-is-show-resize-cursor-nwse-resize');
				document.body.style.cursor = '';
				continue
			}
			if (result.side === 'right' || result.side === 'left') {
				if (!document.body.classList.contains('-is-show-resize-cursor-col-resize')) {
					document.body.classList.add('-is-show-resize-cursor-col-resize');
					document.body.style.cursor = 'col-resize';
				}
			} else if (result.side === 'top' || result.side === 'bottom') {
				if (!document.body.classList.contains('-is-show-resize-cursor-row-resize')) {
					document.body.classList.add('-is-show-resize-cursor-row-resize');
					document.body.style.cursor = 'row-resize';
				}
			} else if (result.side === 'top-right' || result.side === 'bottom-left') {
				if (!document.body.classList.contains('-is-show-resize-cursor-nesw-resize')) {
					document.body.classList.add('-is-show-resize-cursor-nesw-resize');
					document.body.style.cursor = 'nesw-resize';
				}
			} else if (result.side === 'top-left' || result.side === 'bottom-right') {
				if (!document.body.classList.contains('-is-show-resize-cursor-nwse-resize')) {
					document.body.classList.add('-is-show-resize-cursor-nwse-resize');
					document.body.style.cursor = 'nwse-resize';
				}
			}
			this.makeResizableActive(result);
			break
		}
	}

	checkResizeLimitReached(x, y, resizables) {
		let hasReachedLimit = false;
		if (this.isResizing()) {
			const sides = this.activeResizable.side.split('-');
			resizables.filter(resizable => {
				const resizableSide = resizable.dataset.resizable;
				return sides.filter(side => resizableSide.indexOf(sideComplement[side]) > -1).length;
			}).forEach(resizable => {
				const position = resizable.getBoundingClientRect();
				const zoneWidth = resizable.dataset.resizableDragZoneWidth || this.defaultDragZoneWidth;
				const resizableSide = resizable.dataset.resizable;
				const isLimit = /^limit/.test(resizableSide);
				sides.forEach(side => {
					const key = (isLimit ? 'limit_' : '') + side;
					if (resizeDragLimitForSide[key]) {
						if (resizeDragLimitForSide[key](position, x, y, zoneWidth, this.resizing)) {
							hasReachedLimit = true;
						}
					}
				});
			});
		}
		return hasReachedLimit;
	}

	isCoordinatesOnDragZone(x, y, element) {
		const result = {
			side: 'inside',
			target: element
		};
		const check = this.resizeDragZones;
		const sides = element.dataset.resizable || 'top,right,bottom,left';
		const zoneWidth = element.dataset.resizableDragZoneWidth || this.defaultDragZoneWidth;
		const zoneOffsets = this.parseHandleOffsets(element.dataset.resizableHandleOffset);
		const rect = element.getBoundingClientRect();
		const position = {
			top: rect.top + zoneOffsets[0],
			right: rect.right + zoneOffsets[1],
			bottom: rect.bottom + zoneOffsets[2],
			left: rect.left + zoneOffsets[3]
		};
		const temp = [];
		if (check.outside && check.outside(position, x, y, zoneWidth)) {
			result.side = 'outside';
			return result;
		}
		sides.split(/\W+|\s*,\s*/).forEach(side => {
			if (check[side]) {
				const r = check[side](position, x, y, zoneWidth);
				if (r) {
					temp.push(r);
				}
			}
		});
		if (temp.length) {
			result.side = zoneName(temp);
			return result;
		}
		return result;
	}

	parseHandleOffsets(str) {
		const val = String(str).trim().split(' ');
		if (val.length === 0) {
			return defaultHandleOffset.splice();
		}
		return defaultHandleOffset.map((defaultOffset, i) => {
			let offset = parseFloat(val[i]);
			if (isNaN(offset)) {
				offset = parseFloat(val[i - 2]);
				if (isNaN(offset)) {
					return defaultHandleOffset[i];
				}
			}
			return offset;
		});
	}

}

export default ResizableManager;
