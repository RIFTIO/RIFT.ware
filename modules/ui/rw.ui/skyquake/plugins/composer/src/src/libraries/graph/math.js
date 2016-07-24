
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 9/1/15.
 */

import Position from './Position'

const angleOfLine = function (x1, y1, x2, y2) {

	const y = y2 - y1;
	const x = x2 - x1;

	const rad = Math.atan2(y, x);
	const deg = Math.abs(rad * 180 / Math.PI);

	let addDeg = 0;

	if (rad <= 0) {
		addDeg = 360;
	}

	// - addDeg bc atan2 returns -π to π where -π is the upper side of rectangle
	// - 360 bc our ui grid system has zero in upper left corner not lower left
	return Math.abs(Math.abs(deg - addDeg) - 360);

};

const angleBetweenPositions = function (src, dst) {
	if (!src instanceof Position) {
		throw new ReferenceError('require a position instance');
	}
	if (!dst instanceof Position) {
		throw new ReferenceError('require a position instance');
	}
	const srcCenter = src.centerPoint();
	const dstCenter = dst.centerPoint();
	return angleOfLine(srcCenter.x, srcCenter.y, dstCenter.x, dstCenter.y);
};

const distanceBetweenPositions = function (src, dst) {
	if (!src instanceof Position) {
		throw new ReferenceError('require a position instance');
	}
	if (!dst instanceof Position) {
		throw new ReferenceError('require a position instance');
	}
	const srcCenter = src.centerPoint();
	const dstCenter = dst.centerPoint();
	const x = Math.pow(dstCenter.x - srcCenter.x, 2);
	const y = Math.pow(dstCenter.y - srcCenter.y, 2);
	const distance = Math.sqrt((x) + (y));
	if (isNaN(distance)) {
		console.log('nan distance', srcCenter, dstCenter);
	}
	return distance;
};

const rectangleAnglesFromCenter = function (position) {
	/* given a rectangle defined by a Position, determine the angle of each corner from center.
	 // note: 0˚ is on x-axis; angles are given counterclockwise from x-axis
	 // for example:
	 // the line AB(0, 0, 5, 0) would have angle 0˚
	 // the line AC(0, 0, 0, 5) would have angle 90˚
	 // the line BC(5, 0, 0, 5) would have angle 135˚
	 // the line BA(5, 0, 0, 0) would have angle 180˚
	 */
	if (!position instanceof Position) {
		throw new ReferenceError('require a Position instance');
	}
	const center = position.centerPoint();
	return [
		angleOfLine(center.x, center.y, position.right, position.top),
		angleOfLine(center.x, center.y, position.left, position.top),
		angleOfLine(center.x, center.y, position.left, position.bottom),
		angleOfLine(center.x, center.y, position.right, position.bottom)
	];
};

const edgeLocation = function (rectangleInclinations) {
	/* given the angle of a line, determine which edge of the rectangle the line should attach. */
	let inclinations = rectangleInclinations;
	if (rectangleInclinations instanceof Position) {
		inclinations = rectangleAnglesFromCenter(rectangleInclinations);
	}
	return angle => {
		angle = parseFloat(angle);
		if (angle >= inclinations[3]) {
			return 'right';
		}
		if (angle >= 270) {
			return 'bottom-right';
		}
		if (angle >= inclinations[2]) {
			return 'bottom-left';
		}
		if (angle >= inclinations[1]) {
			return 'left';
		}
		if (angle >= 90) {
			return 'top-left';
		}
		if (angle >= inclinations[0]) {
			return 'top-right';
		}
		return 'right';
	};
};

const upperLowerEdgeLocation = function (angle) {
	angle = parseFloat(angle);
	if (angle > 270) {
		return 'bottom-right';
	}
	if (angle > 180) {
		return 'bottom-left';
	}
	if (angle > 90) {
		return 'top-left';
	}
	return 'top-right';
};

export default {
	angleOfLine: angleOfLine,
	angleBetweenPositions: angleBetweenPositions,
	distanceBetweenPositions: distanceBetweenPositions,
	anglesFromCenter: rectangleAnglesFromCenter,
	edgeLocation: edgeLocation,
	upperLowerEdgeLocation: upperLowerEdgeLocation
}
