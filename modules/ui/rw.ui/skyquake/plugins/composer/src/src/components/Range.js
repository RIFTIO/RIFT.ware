/**
 * Created by onvelocity on 2/3/16.
 */
'use strict';

var React = require('react');
var ReactDOM = require('react-dom');
var _extends = Object.assign;

/**
 * WORKAROUND FOR: https://github.com/facebook/react/issues/554
 * COPIED FROM: https://github.com/mapbox/react-range
 */

var Range = React.createClass({
	displayName: 'Range',
	propTypes: {
		onChange: React.PropTypes.func,
		onClick: React.PropTypes.func,
		onKeyDown: React.PropTypes.func,
		onMouseMove: React.PropTypes.func
	},
	getDefaultProps: function() {
		return {
			type: 'range',
			onChange: function(){},
			onClick: function(){},
			onKeyDown: function(){},
			onMouseMove: function(){}
		};
	},
	onRangeChange: function(e) {
		this.props.onMouseMove(e);
		if (e.buttons !== 1 && e.which !== 1) return;
		this.props.onChange(e);
	},
	onRangeClick: function(e) {
		this.props.onClick(e);
		this.props.onChange(e);
	},
	onRangeKeyDown: function(e) {
		this.props.onKeyDown(e);
		this.props.onChange(e);
	},
	componentWillReceiveProps: function(props) {
		ReactDOM.findDOMNode(this).value = props.value;
	},
	render: function() {
		var props = _extends({}, this.props, {
			defaultValue: this.props.value,
			onClick: this.onRangeClick,
			onKeyDown: this.onRangeKeyDown,
			onMouseMove: this.onRangeChange,
			onChange: function() {}
		});
		delete props.value;
		return React.createElement(
			'input',
			props
		);
	}
});

module.exports = Range;