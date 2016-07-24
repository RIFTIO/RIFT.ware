
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var React = require('react');
var ButtonEventListenerMixin = require('../mixins/ButtonEventListener.js');


/**
 *  A text input component.
 *  It's props values and a brief description below
 *
 *  value: Holds the initial text content of the input.
 *  label: The text content of the label.
 *  requiredText: The text content of the "if required" message.
 *  errorText: The text content of the error message.
 *  placeholder: The initial placeholder text of the input
 *  ClassName: Css Classes applied to the element.
 *  rows: Number of text lines the input element displays at one time.
 *  cols: Number of characters per line.
 *  resizable: If the input element is resizable.
 *  isRequired: A boolean indicating whether or not the input is required.
 *  isDisabled: A boolean indicating the disabled state of the element.
 *  isReadONly: A boolean indicating whether or not the input is read only.
 *  maxLength: The hard limit on how many characters can be in the input.
 **/
module.exports = React.createClass({
  displayName: "TextArea",
  mixins:[ButtonEventListenerMixin],
  propTypes: {
    value:           React.PropTypes.string,
    label:           React.PropTypes.string,
    requiredText:    React.PropTypes.string,
    errorText:       React.PropTypes.string,
    placeholder:     React.PropTypes.string,
    className:       React.PropTypes.string,
    rows:            React.PropTypes.number,
    cols:            React.PropTypes.number,
    resizable:       React.PropTypes.bool,
    isRequired:      React.PropTypes.bool,
    isDisabled:      React.PropTypes.bool,
    isReadOnly:      React.PropTypes.bool,
    maxLength:       React.PropTypes.number,
    instructions:    React.PropTypes.string
  },


  /**
   * Sets the default input state.
   * If there is no description for the variable, assume it's the same as it's props counterpart.
   *
   * value: Holds the current text contents of the input element.
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
      value:            this.props.value || "",
      label:            this.props.label || "",
      requiredText:     this.props.requiredText || "Required",
      errorText:        this.props.errorText || "",
      instructionsText: this.props.instructions || "",
      isActive:         false,
      isHovered:        false,
      isFocused:        false,
      isDisabled:       this.props.isDisabled || false,
      isReadOnly:       this.props.isReadOnly || false,
      isRequired:       this.props.isRequired || null,
      isValid:          null,      // Three way bool. Valid: true.   Invalid: false. Not acted on: null.
      isSuccess:        null     // Three way bool. Success: true. Error: false.   Not acted on: null.

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
      this.state.isHovered + this.state.isValid + this.state.isSuccess + this.state.value;
    var nextStateString = nextState.isReadOnly + nextState.isDisabled + nextState.isActive + nextState.isFocused +
      nextState.isHovered + nextState.isValid + nextState.isSuccess + nextState.value;
    if (currentStateString == nextStateString) {
      return false;
    }
    return true;
  },
  /**
   * Makes sure that when the user types new input, the contents of the input changes accordingly.
   *
   * @param event
   */
  handleChange: function(event) {
    this.setState({value:event.target.value});
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

  /**
   * Renders the Text Input component.
   *
   **/
  render: function() {
    var value = this.state.value;
    var input = null;
    var textarea_style = {};
    var textarea_state = this.setComponentState();

    // creates a style for the "resizable" attribute.
    if (!this.props.resizable) {
      textarea_style.resize = "none";
    }

    // The input element.
    input = React.createElement("textarea", {
        ref:               "inputRef",
        "data-state":      textarea_state,
        value:             value,
        "style":           textarea_style,
        placeholder:       this.props.placeholder,
        rows:              this.props.rows,
        cols:              this.props.cols,
        maxLength:         this.props.maxLength,
        required:          this.state.isRequired,
        disabled:          this.state.isDisabled,
        readOnly:          this.state.isReadOnly,
        onChange:          this.handleChange,
        onClick:           this.onClick,
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
        onBlur:            this.onBlur,
          className:         (this.props.className || "rw-textarea"),
          tabIndex:          0
      },
      null
    );

    // The "if required" element. It displays a label if the element is required.
    if(this.props.isRequired == true){
      var requiredEle = React.createElement("small", {className: "rw-form-requiredLabel"}, this.state.requiredText);
    }

    // The label element associated with the input.
    var label = React.createElement("label", null, this.state.label, requiredEle, input);

    // The "error" element.  It pops up as a message if there is an error with the input.
    if(this.state.errorText != "") {
      var error = React.createElement("p", {className: "rw-form-errorMsg"}, this.state.errorText);
    }

    //
    if(this.state.instructionsText != ""){
      var instructions = React.createElement("p", {className: "rw-form-instructions"}, this.state.instructionsText)
    }

    // The parent element for all.
    var ret = React.createElement("div", {
      "data-state":      textarea_state,
      required:          this.state.isRequired,
      disabled:          this.state.isDisabled,
      readOnly:          this.state.isReadOnly
    }, label, error, instructions);

    return ret;
  }
});
