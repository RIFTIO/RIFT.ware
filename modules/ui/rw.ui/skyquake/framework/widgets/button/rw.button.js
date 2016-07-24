
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import './button.scss';
import Loader from '../loading-indicator/loadingIndicator.jsx';
var React = require('react');
var ButtonEventListenerMixin = require('../mixins/ButtonEventListener.js');


/**
 *  A generic button component.
 *  It's props values and a brief description below
 *
 *  Label: The label of the button. What it displays at any given time.
 *  Icon: A url for an icon that will be displayed on the button.  Leave blank if no icon required.
 *  Class: Css Classes applied to the element.
 *  sizeOfButton: The preset sizes for the button (small, default, large, xlarge, expand).
 *  minWidth: Minimum Width of the button.
 *  maxWidth: Maximum Width of the button.
 **/
module.exports = React.createClass({
  displayName: "Button",
  mixins:[ButtonEventListenerMixin],
  propTypes: {
    label:           React.PropTypes.string,
    icon:            React.PropTypes.array,
    className:       React.PropTypes.string,
    //sizeOfButton:    React.PropTypes.string,
    minWidth:        React.PropTypes.string,
    maxWidth:        React.PropTypes.string,
    //isActive:        React.PropTypes.bool,
    //isFocused:       React.PropTypes.bool,
    //isHovered:       React.PropTypes.bool,
    isDisabled:      React.PropTypes.bool
  },


  /**
   * Defines default state.
   * sizeOfButton: See Prop type definitions.
   * class: See Prop type definitions.
   * label: See Prop type definitions.
   * isActive: Boolean to indicate if button is active.
   * isHovered: Boolean to indicate if the button is being hovered over.
   * isFocused: Boolean to indicate if the button has been focused.
   * isDisabled: Boolean to indicate if button has been disabled.
   * @returns {{sizeOfButton: (*|string), class: *, isActive: boolean, isHovered: boolean,
   *            isFocused: boolean, isDisabled: (*|boolean), label: *}}
   */
  getInitialState: function() {
    return {
      //sizeOfButton:   this.props.size || '',  //There is no Medium value in CSS, default size is the absence of a value
      className:      this.props.className || 'rw-button-primary', //Default value is 'rw-button-primary' which is the primary one
      label:          this.props.label,
      isActive:       false,
      isHovered:      false,
      isFocused:      false,
      isLoading:      this.props.isLoading || false,
      isDisabled:     this.props.isDisabled || false
    }
  },


  /**
   * If any of the state variables have changed, the component should update.
   * "nextProps" and "nextState" hold the state and property variables as they will be after the update.
   * "this.props" and "this.state" hold the state and property variables as they were before the update.
   * returns true if the state have changed. Otherwise returns false.
   * @param nextProps
   * @param nextState
   * @returns {boolean}
   */
  shouldComponentUpdate: function(nextProps, nextState) {
    var currentStateString = this.state.label + this.state.isDisabled + this.state.isActive + this.state.isFocused +
      this.state.isHovered + this.props.isLoading;
    var nextStateString = nextState.label + nextState.isDisabled + nextState.isActive + nextState.isFocused +
      nextState.isHovered + nextProps.isLoading;

    if (currentStateString == nextStateString) {
      return false;
    }
    return true;
  },


  /**
   * Returns a string reflecting the current state of the button.
   * If the button state "isDisabled" is true, returns a string "disabled".
   * Otherwise returns a string containing a word for each state that has been set to true.
   * (ie "active focused" if the states active and focused are true, but hovered is false).
   * @returns {string}
   */
  setButtonState: function() {
    var ret = "";
    if (this.state.isDisabled) {
      return "disabled";
    }
    if (this.state.isActive) {
      ret += "active ";
    }
    if (this.state.isHovered) {
      ret += "hovered ";
    }
    if (this.state.isFocused) {
      ret += "focused ";
    }
    return ret;
  },



  /**
   * Track the width if set and write into markup using Style attribute
   * Returns the minWidth and maxWidth prop in a string
   * @returns {{}}
   */
  setButtonWidth:function(){ 
    var width = {};  

    if (this.props.minWidth) { 
      width.minWidth = String(this.props.minWidth); 
    } 
    if (this.props.maxWidth) { 
      width.maxWidth = String(this.props.maxWidth); 
    } 

    return width; 
  },



  /**
   * Apply the size of the button to the icon directly
   * Returns a string indicating the icon size.
   * @returns {string}
   */
  /*
  setIconSize:function(){

    var iconClass = "rw-icon";

    if(this.props.size){
      iconClass += "-" + this.props.size;
    }
    return iconClass;
  },
  */



  /**
   * Builds the list of classes.
   * Returns a string holding each class seperated by a space.
   * @returns {string}
   */
  /*
  setButtonClass:function() {
    var buttonClass = "";
    var buttonClassType = "rw-button-primary";
    // If the size is declared, add it in
    if (this.state.sizeOfButton) {
      buttonClass += this.state.sizeOfButton;
    }
    //
    if (typeof(this.props.className) != "undefined") {
      this.props.className.push("rw-button-secondary");

      // Run through the array and check all the values
      for (var i = 0; i < this.props.className.length; i++) {

        if (this.props.className[i].indexOf("secondary") > -1) {
          buttonClassType = "rw-button-secondary";  // If the value of the array is equal to the string "secondary", add a predefined string
        } else {
          buttonClass += " " + this.props.className[i];  // Else just write the value of the array
        }
      }
    }
    buttonClass += " " + buttonClassType;  //Add the button style type either primary or secondary
    return buttonClass;
  },
  */

  /**
   * Builds an array of html elements for the icons and returns them.
   * @returns {Array}
   */
  setIconElement: function() {
    var button_icon = [];

    if (typeof(this.props.icon) != "undefined") {
      for (var i = 0; i < this.props.icon.length; i++) {
        button_icon.push(React.createElement('svg', {
          className: "rw-button__icon",
          key: i,
          dangerouslySetInnerHTML: {__html: '<use xlink:href="#' + this.props.icon[i] + '" />'}  //Using a React method to drop in a hack since React does not support xlink:href yet
        }));
      }
    }
    return button_icon;
  },

  /**
   * Renders the Button Component
   * Returns a react component that constructs the html that houses the button component.
   * @returns {*}
   */
  render: function() {
    var button = null;
    var button_style = this.setButtonWidth();
    var button_state = this.setButtonState();
    var button_class = this.state.className;
    var button_icon = this.setIconElement();
    var display = this.state.label;
    if (this.props.isLoading) {
      display = React.createElement(Loader, {show: true, size: '1rem', color: this.props.loadingColor});
    }

    button = React.createElement("button", {
        className:         button_class,
        "data-state":      button_state,
        style:             button_style,

        // onClick:           this.onClick,
        onClick:           this.props.onClick,
        onMouseUp:         this.onMouseUp,
        onMouseDown:       this.onMouseDown,
        onMouseOver:       this.onMouseOver,
        onMouseEnter:      this.onMouseEnter,
        onMouseLeave:      this.onMouseLeave,
        onMouseOut:        this.onMouseOut,
        onTouchCancel:     this.onTouchCancel,
        onTouchEnd:        this.onTouchEnd,
        onTouchMove:       this.onTouchMove,
        onTouchStart:      this.onTouchStart,
        onKeyDown:         this.onKeyDown,
        onKeyPress:        this.onKeyPress,
        onKeyUp:           this.onKeyUp,
        onFocus:           this.onFocus,
        onBlur:            this.onBlur
      },
      button_icon,
      React.createElement("span", {className: "rw-button__label"}, display)
    );
    return button;
  }
});
