
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/25/15.
 */

export default class Position {

	constructor(pos = {}) {
		this._position = Object.assign({}, Position.defaults(), pos);
		// if consumer specified width or height in the
		// constructor then set right and bottom....
		if (!isNaN(parseFloat(pos.width))) {
			// ensure right is set
			this.width = pos.width;
		}
		if (!isNaN(parseFloat(pos.height))) {
			// ensure bottom is set
			this.height = pos.height;
		}
	}

	static defaults() {
		return {
			top: 0,
			left: 0,
			bottom: 0,
			right: 0,
			width: 0,
			height: 0
		};
	}

	get position() {
		return Object.assign({}, this._position);
	}

	get top() {
		return this._position.top;
	}

	set top(value) {
		this._position.top = parseFloat(value) || 0;
		return this;
	}

	get left() {
		return this._position.left;
	}

	set left(value) {
		this._position.left = parseFloat(value) || 0;
		return this;
	}

	get bottom() {
		return this._position.bottom;
	}

	set bottom(value) {
		this._position.bottom = parseFloat(value) || 0;
		return this;
	}

	get right() {
		return this._position.right;
	}

	set right(value) {
		this._position.right = parseFloat(value) || 0;
		return this;
	}

	get width() {
		return Math.abs(this.right - this.left);
	}

	set width(width) {
		const value = parseFloat(width);
		if (!isNaN(value)) {
			this.right = value + this.left;
			this._position.width = this.width;
		}
	}

	get height() {
		return Math.abs(this.bottom - this.top);
	}

	set height(height) {
		const value = parseFloat(height);
		if (!isNaN(value)) {
			this.bottom = value + this.top;
			this._position.height = this.height;
		}
	}

	toString() {
		return JSON.stringify(this._position);
	}

	value() {
		return {
			top: this.top,
			left: this.left,
			right: this.right,
			bottom: this.bottom,
			width: this.width,
			height: this.height
		};
	}

	centerPoint() {
		return {
			x: this.left + (this.width / 2),
			y: this.top + (this.height / 2)
		};
	}

	move(left, top) {
		this.moveTop(top);
		this.moveLeft(left);
	}

	moveLeft(left) {
		const width = this.width;
		const value = parseFloat(left);
		if (!isNaN(value)) {
			this._position.left = value;
			this._position.right = value + width;
		}
	}

	moveRight(right) {
		const width = this.width;
		const value = parseFloat(right);
		if (!isNaN(value)) {
			this._position.left = value - width;
			this._position.right = value;
		}
	}

	moveTop(top) {
		const height = this.height;
		const value = parseFloat(top);
		if (!isNaN(value)) {
			this._position.top = value;
			this._position.bottom = value + height;
		}
	}

	moveBottom(bottom) {
		const height = this.height;
		const value = parseFloat(bottom);
		if (!isNaN(value)) {
			this._position.top = value - height;
			this._position.bottom = value;
		}
	}

}
