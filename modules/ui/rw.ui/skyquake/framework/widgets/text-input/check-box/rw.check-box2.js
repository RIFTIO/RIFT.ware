
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var React = require('react');
var ButtonEventListenerMixin = require('../mixins/ButtonEventListener.js');


/**
 *  A check-box component.
 *  It's props values and a brief description below
 *
 *  label:        The label for the checkbox group.
 *  checkboxes:   The object that creates each checkbox.  Each object has a property "label" and "checked".
 *    label:        The label for the individual checkbox.
 *    checked:      If set to true, the individual checkbox is initialized with a check.
 *  requiredText: The text content of the "if required" message.
 *  errorText:    The text content of the error message.
 *  ClassName:    Css Classes applied to the element.
 *  size:         The size of the element.
 *  isRequired:   A boolean indicating whether or not the input is required.
 *  isDisabled:   A boolean indicating the disabled state of the element.
 *  isReadOnly:   A boolean indicating whether or not the input is read only.
 *  instructions: The text content of the instructions
 **/
module.exports = React.createClass({
  displayName: "CheckBox",
  mixins:[ButtonEventListenerMixin],
  propTypes: {
    children:        React.PropTypes.arrayOf(React.PropTypes.object),
    label:           React.PropTypes.string,
    checked:         React.PropTypes.bool,
    indeterminate:   React.PropTypes.bool,
    requiredText:    React.PropTypes.string,
    instructionText: React.PropTypes.string,
    errorText:       React.PropTypes.string,
    className:       React.PropTypes.string,
    size:            React.PropTypes.string,
    isRequired:      React.PropTypes.bool,
    isDisabled:      React.PropTypes.bool,
    isReadOnly:      React.PropTypes.bool
  },


  /**
   * Sets the default input state.
   * If there is no description for the variable, assume it's the same as it's props counterpart.
   *
   * isActive: Boolean to indicate if input is active.
   * isHovered: Boolean to indicate if the input is being hovered over.
   * isFocused: Boolean to indicate if the input has been focused.
   * isDisabled: Boolean to indicate if input has been disabled.
   *
   * @returns {{sizeOfButton: (*|string), isActive: boolean, isHovered: boolean, isFocused: boolean, isDisabled: (*|boolean),
   * isReadOnly: (*|.textInput.isReadOnly|.exports.propTypes.isReadOnly|.exports.getInitialState.isReadOnly|boolean),
   * isRequired: (*|.textInput.isRequired|.exports.propTypes.isRequired|.exports.getInitialState.isRequired|isRequired|null),
   * isValid: null, isSuccess: null}}
   */
  getInitialState: function() {

    return {
      //mounted:              false,
      label:                this.props.label || "",
      requiredText:         this.props.requiredText || "Required",
      instructionsText:     this.props.instructions || "",
      errorText:            this.props.errorText || "",
      size:                 this.props.size || '',
      isActive:             false,
      isHovered:            false,
      isFocused:            false,
      isDisabled:           this.props.disabled || false,
      isReadOnly:           this.props.isReadOnly || false,
      isRequired:           this.props.isRequired || null,
      isValid:              null,      // Three way bool. Valid: true.   Invalid: false. Not acted on: null.
      isSuccess:            null       // Three way bool. Success: true. Error: false.   Not acted on: null.
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
    var currentStateString = this.state.isReadOnly + this.state.isDisabled + this.state.isActive + this.state.isFocused +
      this.state.isHovered + this.state.isValid + this.state.isSuccess;
    var nextStateString = nextState.isReadOnly + nextState.isDisabled + nextState.isActive + nextState.isFocused +
      nextState.isHovered + nextState.isValid + nextState.isSuccess;
    if (currentStateString == nextStateString) {
      return false;
    }
    return true;
  },

  componentDidMount: function() {
    this.state.mounted = true;
    this.render();
  },

  /**
   * Returns a string reflecting the current state of the input.
   * If the component state "isDisabled" is true, returns a string "disabled".
   * If the component state "isReadOnly" is true, returns a string "readonly".
   * Otherwise returns a string containing a word for each state that has been set to true.
   * (ie "active focused" if the states active and focused are true, but hovered is false).
   * @returns {string}
   */
  setComponentState: function() {

    var ret = "";
    if (this.state.isDisabled) {
      return "disabled";
    }
    if (this.state.isReadOnly) {
      return "readonly";
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

  localOnClick: function(e) {
    this.onClick(e);
  },

  /**
   * Renders the checkbox component.
   *
   **/
  render: function() {
    var input = [];
    var checkbox = [];
    var label = [];
    var input_style = {};
    var input_state = this.setComponentState();

    // The group label element
    label = React.createElement("label", {className: "rw-form__label"}, this.props.label);

    // Creates each individual checkbox element and the label for each.
    for (var i = 0; i < this.props.checkboxes.length; i++) {
      checkbox[i] = React.createElement("input",{
        key:               i,
        defaultChecked:    this.props.checkboxes[i].checked,
        //indeterminate: true,
        type:              "checkbox",
        readOnly:          this.props.readonly,
        disabled:          this.props.disabled,
        onClick:           this.localOnClick,
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
      }, null);
      if (this.state.mounted) {
        this.getDOMNode().children[i + 1].children[0].indeterminate = this.props.checkboxes[i].indeterminate;
      }


      input[i] = React.createElement("label", {className: "rw-form__label-checkBox", key:i, readOnly:this.props.readonly, disabled:this.props.disabled}, checkbox[i], this.props.checkboxes[i].label);

    }

    // The "if required" element. It displays a label if the element is required.
    if(this.props.isRequired == true){
      var requiredEle = React.createElement("small", {className: "rw-form__required-label"}, this.state.requiredText);
    }

    // The "error" element.  It pops up as a message if there is an error with the input.
    if(this.state.errorText != "") {
      var error = React.createElement("p", {className: "rw-form__message-error"}, this.state.errorText);
    }

    // The "instruction" element.
    if(this.state.instructionsText != ""){
      var instructions = React.createElement("p", {className: "rw-form-instructions"}, this.state.instructionsText)
    }

    // The parent element for all.
    var ret = React.createElement("div", {
      "data-state":      input_state,
      required:          this.state.isRequired,
      disabled:          this.state.isDisabled,
      readOnly:          this.state.isReadOnly,
      className:         "rw-form"

    }, requiredEle, label, input, error, instructions);

    return ret;
  }
});
