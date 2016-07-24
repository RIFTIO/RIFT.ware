
/**
 * Created by onvelocity on 2/10/16.
 */

export default class PathBuilder {

	constructor() {
		this.path = [];
	}

	static archFlags(override) {
		return Object.assign({xAxisRotation: 0, largeArcFlag: 0, sweepFlag: 0}, override);
	}

	M(x, y, info = {}) {
		this.path.push({x: x, y: y, cmd: 'M', info: info});
		return this;
	}

	m(dx, dy, info = {}) {
		this.path.push({dx: dx, dy: dy, cmd: 'm', info: info});
		return this;
	}

	L(x, y, info = {}) {
		this.path.push({x: x, y: y, cmd: 'L', info: info});
		return this;
	}

	l(dx, dy, info = {}) {
		this.path.push({dx: dx, dy: dy, cmd: 'l', info: info});
		return this;
	}

	H(x, info = {}) {
		this.path.push({x: x, cmd: 'H', info: info});
		return this;
	}

	h(dx, info = {}) {
		this.path.push({dx: dx, cmd: 'h', info: info});
		return this;
	}

	V(y, info = {}) {
		this.path.push({y: y, cmd: 'V', info: info});
		return this;
	}

	v(dy, info = {}) {
		this.path.push({dy: dy, cmd: 'v', info: info});
		return this;
	}

	C(x1, y1, x2, y2, x, y, info = {}) {
		this.path.push({x1: x1, y1: y1, x2: x2, y2: y2, x: x, y: y, cmd: 'C', info: info});
		return this;
	}

	c(dx1, dy1, dx2, dy2, dx, dy, info = {}) {
		this.path.push({dx1: dx1, dy1: dy1, dx2: dx2, dy2: dy2, dx: dx, dy: dy, cmd: 'c', info: info});
		return this;
	}

	Q(x1, y1, x, y, info = {}) {
		this.path.push({x1: x1, y1: y1, x: x, y: y, cmd: 'Q', info: info});
		return this;
	}

	q(x1, y1, x, y, info = {}) {
		this.path.push({x1: x1, y1: y1, x: x, y: y, cmd: 'q', info: info});
		return this;
	}

	T(x, y, info = {}) {
		this.path.push({x: x, y: y, cmd: 'T', info: info});
		return this;
	}

	A (rx, ry, x, y, info = {xAxisRotation: 0, largeArcFlag: 0, sweepFlag: 0}) {
		info = PathBuilder.archFlags(info);
		this.path.push({rx: rx, ry: ry, 'x-axis-rotation': info.xAxisRotation,  'large-arc-flag': info.largeArcFlag, 'sweep-flag': info.sweepFlag, x: x, y: y, cmd: 'A', info: info});
		return this;
	}

	S(x2, y2, x, y, info = {}) {
		this.path.push({x2: x2, y2: y2, x: x, y: y, cmd: 'S', info: info});
		return this;
	}

	H(x, info = {}) {
		this.path.push({x: x, info: info});
		return this;
	}

	h(dx, info = {}) {
		this.path.push({dx: dx, info: info});
		return this;
	}

	V(y, info = {}) {
		this.path.push({y: y, info: info});
		return this;
	}

	v(dy, info = {}) {
		this.path.push({dy: dy, info: info});
		return this;
	}

	get Z() {
		this.path.push({cmd: 'Z'});
		return this;
	}

	last() {
		return this.path[this.path.length - 1];
	}

	findPrevious(filter) {
		return this.path.splice().reverse().reduce((r, d) => {
			if (r) {
				return r;
			}
			if (filter(d)) {
				return d
			}
		}, null);
	}

	insertBeforeLast() {
		if (this.path.length < 3) {
			return this;
		}
		const last = this.path.pop();
		this.path.splice(this.length - 2, 0, last);
		return this;
	}

	concat(pathBuilder) {
		this.path = this.path.filter(d => d.cmd !== 'Z').concat(pathBuilder);
		return this;
	}

	toString() {
		return this.path.reduce((r, d) => {
			return r + ' ' + d.cmd + ' ' + Object.keys(d).filter(k => !(k == 'cmd' || k === 'info')).map(k => d[k]).reduce((r, val) => r + ' ' + val, '');
		}, '').replace(/\s+/g, ' ').trim();
	}

	get length() {
		return this.path.length;
	}

	static descriptorPath(borderRadius = 20, height = 55, width = 250) {
		const path = new PathBuilder();
		borderRadius = Math.max(0, borderRadius);
		width = Math.max(borderRadius, width) - borderRadius;
		height = Math.max(borderRadius, height) - (borderRadius);
		const iconWidthHeight = 40;
		return path.M(0, 0)
			// top
			.L(width, 0).C(width, 0, width + borderRadius, 0, width + borderRadius, borderRadius)
			// right
			.L(width + borderRadius, height).C(width + borderRadius, height, width + borderRadius, height + borderRadius, width, height + borderRadius)
			// bottom
			.L(0, height + borderRadius).C(0, height + borderRadius, 0 - borderRadius, height + borderRadius, 0 - borderRadius, height)
			// left
			.L(0 - borderRadius, 0 + borderRadius).C(0 - borderRadius, 0 + borderRadius, 0 - borderRadius, 0, 0, 0)
			// icon separator
			//.L(iconWidthHeight, 0).L(iconWidthHeight, iconWidthHeight + (borderRadius + height - iconWidthHeight))
			// label separator
			//.L(iconWidthHeight, iconWidthHeight).L(0 - borderRadius, iconWidthHeight);
	}

}
