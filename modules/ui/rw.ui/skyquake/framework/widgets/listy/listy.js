/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import _ from 'lodash';

/**
 *
 *
 */
export default class Listy extends React.Component {
	constructor(props) {
 		super(props);
 	}

 	/**

 	 */
 	createList(data, iter=0) {
 		var groupTag = this.props.groupTag.tag;
 		var groupClass = this.props.groupTag.className;
 		var itemTag = this.props.itemTag.tag;
 		var itemClass = this.props.itemTag.className;
 		var listHeaderTag = this.props.listHeaderTag.tag;
 		var listHeaderClass = this.props.listHeaderTag.className;

 		var listNode = null;
 		var self = this;
 		if (_.isArrayLike(data) && _.isObjectLike(data)) {
 			var children = [];
			data.forEach(function(element, index, array) {
				if ( _.isArrayLike(element) || _.isObjectLike(element)) {
					children.push(self.createList(element, iter+1));
				} else {
					children.push(React.createElement(itemTag, {
						key: index,
						className: itemClass
					}, element));
				}
			});

			listNode = React.createElement(groupTag, {
				key: iter,
				className: groupClass }, children);
 		}
 		else if (_.isObjectLike(data)) {
 			var children = [];
 			for (var key in data) {
 				if ( _.isArrayLike(data[key]) || _.isObjectLike(data[key])) {
 					children.push(
 						React.createElement(listHeaderTag, {
 							key: key + '_header',
 							className: listHeaderClass }, key + ":")
 					);
 					children.push(
 						React.createElement(groupTag, {
 							key: key + '_list',
 							className: groupClass },
 							[this.createList(data[key], iter + 1)])
 					);

 				} else {
 					// TODO: Add span to line-wrap the data part (hanging)
 					children.push(React.createElement(itemTag, {
 						key: key,
 						className: itemClass},
 						key + ": " + data[key]));
 				}
 			}
 			listNode = React.createElement(groupTag, {
 				key: iter,
 				className: groupClass }, children);
 		} else {
 			listNode = React.createElement(itemTag, {
 				className: itemClass}, data);
 		}

 		return listNode;
 	}

 	noDataMessage() {
 		return React.createElement("div", {
 			className: this.props.noDataMessageClass},
 			this.props.noDataMessage);
 	}

 	render () {
 		var data = this.props.data;

 		return React.createElement("div", {
 			className: "listy" },
 			_.isEmpty(data) ? 
 			this.noDataMessage() : 
 			this.createList(data)
 		)
 	}
}

Listy.validateTagDefinition = function(props, propName, componentName) {
	let obj = props[propName];
	let fullAttr = componentName + "." + propName;

	if (!obj)
		return new Error('Validation failed. "%" is undefined', fullAttr);
	if (!obj.hasOwnProperty("tag") || _.isEmpty(obj.tag))
		return new Error('Validation failed. "%s" missing attribute "tag"', fullAttr);
	if (!obj.hasOwnProperty("className") || obj.className == undefined)
		return new Error('Validation failed. "%s" missing attribute "className"', fullAttr);
}

Listy.propTypes = {
	data: React.PropTypes.object,
	groupTag: Listy.validateTagDefinition,
	itemTag: Listy.validateTagDefinition,
	listHeaderTag: Listy.validateTagDefinition,	
	debugMode: React.PropTypes.bool
}

Listy.defaultProps = {
	data: {},

	// Visual Rules
	groupTag: {
		tag: "ul",
		className: "listyGroup"
	},
	itemTag: {
		tag: "li",
		className: "listyItem"
	},
	listHeaderTag: {
		tag: "h2",
		className: "listyGroupHeader"
	},
	noDataMessage: "No data",
	noDataMessageClass: "listyNoDataMessage",
	debugMode: false
}
