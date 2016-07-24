
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require('react');
var mixin = require('../mixins/ButtonEventListener.js')
//TODO: Many values are hard coded, need to make bullet "smarter"
/**
 *  A bullet graph component.
 *  It's props values and a brief description below
 *
 *  vertical: If true, the bar rises and falls vertically
 *  value:    The value displayed
 *  min:      The minimum value expected
 *  max:      The maximum value expected
 *  bulletColor: The fill color of the value section of the graph
 *  radius:   Radius of graph corners.
 *  containerMarginX: Container's margin along x-axis
 *  containerMarginY: Container's margin along y-axis
 *  width:    Width of bullet graph in pixels
 *  height:   Height of bullet graph in pixels
 *  fontSize:  Font size of text in pixels
 *  textMarginX: margin for text on x-axis
 *  textMarginY: Margin for text on y-axis
 *  units:    units displayed. Also changes whether a max value is displayed.
 **/
module.exports = React.createClass({
  displayName: 'Bullet',
  mixins:mixin.MIXINS,
  propTypes: {
    vertical: React.PropTypes.bool,
    value: React.PropTypes.number,
    min: React.PropTypes.number,
    max: React.PropTypes.number,
    bulletColor: React.PropTypes.string,
    radius: React.PropTypes.number,
    containerMarginX: React.PropTypes.number,
    containerMarginY: React.PropTypes.number,
    bulletMargin: React.PropTypes.number,
    width: React.PropTypes.number,
    height: React.PropTypes.number,
    markerX: React.PropTypes.number,
    fontSize: React.PropTypes.number,
    textMarginX: React.PropTypes.number,
    textMarginY: React.PropTypes.number,
    units: React.PropTypes.string
  },

  getDefaultProps: function() {
    return {
        vertical: false,
        value: 0,
        min: 0,
        max: 100,
        bulletColor: "blue",
        radius: 4,
        containerMarginX: 0,
        containerMarginY: 0,
        bulletMargin: 0,
        width: 512,
        height: 64,
        markerX: -100,
        fontSize: 16,
        textMarginX: 5,
        textMarginY: 42,
        units:''
    }
  },

  /**
   * Defines default state.
   * value: The value displayed
   */
  getInitialState: function() {
    return {
        value: this.props.value
    }
  },

  /**
   * Called when the props are updated.
   * Syncs the state value with the prop value.
   * @params nextProps
   */
  componentWillReceiveProps: function(nextProps) {
    this.setState({value:nextProps.value || this.state.value})
  },

  /**
   * Before the component will mount, the width value is recalculed based on browser.
   */
  componentWillMount: function() {
        var isIE = false || !!document.documentMode;
        var range = this.props.max - this.props.min;
        var normalizedValue = (this.state.value - this.props.min) / range;
        this.isPercent = (!this.props.units || this.props.units == '')? true:false
        if (isIE) {
            this.bulletWidth = String(Math.round(100 * normalizedValue)) + "%";
        } else {
            this.bulletWidth = this.props.width - (2 * this.props.containerMarginX);
        }
        this.displayValue = (this.isPercent)? String(Math.round(normalizedValue * 100)) + "%" : this.props.value

  },

  /**
   * When the component mounts, this function sets the animation for smooth transition on value change.  This only
   * happens if the user's browser is not IE.
   */
  componentDidMount: function() {
        var isIE = false || !!document.documentMode;
        var range = this.props.max - this.props.min;
        var normalizedValue = (this.state.value - this.props.min) / range;
        if (!isIE) {
            var transform = "scaleX(" + normalizedValue + ")";
            var bullet = React.findDOMNode(this.refs.bulletRef);
            bullet.style.transform = transform;
            bullet.style["-webkit-transform"] = transform;
        }
  },

  /**
   * On update, reaplies transformation and width changes made in componentWillMount and componentDidMount
   * @param nextProps
   * @param nextState
   * @returns {boolean}
   */
  shouldComponentUpdate: function(nextProps, nextState) {

    if (String(this.state.value) == String(nextState.value)) {
      return false;
    } else {
        var isIE = false || !!document.documentMode;
        var range = this.props.max - this.props.min;
        var normalizedValue = (nextState.value - this.props.min) / range;

        if (isIE) {
            this.bulletWidth = String(Math.round(100 * normalizedValue)) + "%";
        } else {
            this.bulletWidth = this.props.width - (2 * this.props.containerMarginX);
            var transform = "scaleX(" + normalizedValue + ")";
            var bullet = React.findDOMNode(this.refs.bulletRef);
            bullet.style.transform = transform;
            bullet.style["-webkit-transform"] = transform;
        }
        this.displayValue = (this.isPercent)? String(Math.round(normalizedValue * 100)) + "%" : nextState.value

        return true;
    }
  },



  /**
   * Renders the Bullet Component
   * @returns {*}
   */
  render: function() {

    // The text element that displays the difference between the max value and the current value.
    var maxDOMEl = (!this.isPercent)? null : React.createElement("text", {
                //X needs better logic
                // x: this.props.width - this.props.height * 1.25,
                x: this.props.width - (130 - this.props.textMarginX),
                y: this.props.textMarginY,
                fontFamily: "Verdana",
                fontSize: this.props.fontSize,
                fill: "#ffffff"}, String(this.props.max - this.state.value) + "%");


        // The main bullet element.  Made up of a static black rect in the background,
        // a moving colored rect overlayed on top and a text element for the current value.
        var bulletDOM = React.createElement("svg", {
            width: "100%",
            height: "100%",
            viewBox: "0 0 512 " + this.props.height,
            preserveAspectRatio: "none"},
          React.createElement("rect", {className: "bullet-container",
                width: this.props.width - (2 * this.props.containerMarginX),
                height: this.props.height - (2 * this.props.containerMarginY),
                x: this.props.containerMarginX,
                y: this.props.containerMarginY,
                rx: this.props.radius,
                ry: this.props.radius}, null),
          React.createElement("svg", {
              x: this.props.containerMarginX,
              y: this.props.bulletMargin},
            React.createElement("rect", {
                className: "bullet",
                ref:"bulletRef",
                fill: this.props.bulletColor,
                width: this.bulletWidth,
                height: this.props.height - (2 * this.props.bulletMargin),
                rx: this.props.radius,
                ry: this.props.radius
            })
          ),
          React.createElement("line", {className: "bullet-marker",
                x1: this.props.markerX,
                x2: this.props.markerX,
                y1: this.props.markerY1,
                y2: this.props.markerY2}),
            React.createElement("text", {
                x: this.props.textMarginX,
                y: this.props.textMarginY,
                "fontFamily": "Verdana",
              "fontSize": this.props.fontSize,
              fill: "#ffffff"}, this.displayValue
              ),
            maxDOMEl
        );


    return bulletDOM;
  }
});
