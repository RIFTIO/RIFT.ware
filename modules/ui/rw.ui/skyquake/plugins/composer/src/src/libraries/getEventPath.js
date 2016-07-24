/**
 * Created by onvelocity on 2/3/16.
 */
'use strict';
export default function getEventPath(event) {
	if (event.path) {
		return event.path;
	}
	if (event.nativeEvent && event.nativeEvent.path) {
		return event.nativeEvent.path;
	}
	// for browsers, like IE, that don't have event.path
	let node = event.target;
	const path = [];
	// Array.unshift means put the value on the top of the array
	const addToPath = (value) => path.unshift(value);
	// warn: some browsers set the root's parent to itself
	while (node && node !== node.parentNode) {
		addToPath(node);
		node = node.parentNode;
	}
	return path;
}
