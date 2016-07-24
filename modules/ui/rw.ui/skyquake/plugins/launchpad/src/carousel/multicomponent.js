
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require('react');
var mixin = require('./ButtonEventListener.js')
/**
 *  Contains a set of components.  Takes a list of components and renders them in lists.
 *  It's props values and a brief description below
 *  component_list: Takes a list of React components.
 */
module.exports = React.createClass({
  displayName: 'Multicomponent',
  mixins:mixin.MIXINS,
  propTypes: {
    component_list:           React.PropTypes.array.isRequired
  },

  /**
   * Defines default state.
   *  component_list: Takes a list of React components.
   */
  getInitialState: function() {
    return {
      component_list: this.props.component_list

    }
  },


  /**
   * Renders the multicomponent Component
   * Returns a list React components
   * @returns {*}
   */
  render: function() {
    var components = [];
    for (var i = 0; i < this.props.component_list.length; i++) {
      components.push(this.props.component_list[i].component);
    }

    var componentDOM = React.createElement("div", {className:this.props.className}, 
      components
    )
    return componentDOM;
  }
});