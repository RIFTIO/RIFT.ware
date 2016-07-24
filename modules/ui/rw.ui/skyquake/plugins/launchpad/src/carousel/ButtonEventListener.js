
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require('react');


/**
 * Event listener Mixins. A vast majority of components are going to expose these events to the user so we're making
 * a central location to house all of them.
 */
var MIXINS = {
  propTypes: {
    onClick:       React.PropTypes.func,
    onMouseUp:     React.PropTypes.func,
    onMouseDown:   React.PropTypes.func,
    onMouseOver:   React.PropTypes.func,
    onMouseEnter:  React.PropTypes.func,
    onMouseLeave:  React.PropTypes.func,
    onMouseOut:    React.PropTypes.func,

    onTouchCancel: React.PropTypes.func,
    onTouchEnd:    React.PropTypes.func,
    onTouchMove:   React.PropTypes.func,
    onTouchStart:  React.PropTypes.func,

    onKeyDown:     React.PropTypes.func,
    onKeyPress:    React.PropTypes.func,
    onKeyUp:       React.PropTypes.func,

    onFocus:       React.PropTypes.func,
    onBlur:        React.PropTypes.func
  },

  /**
   * A vast majority of these functions just check to see if the event function is defined and then passes the function
   * both the event and the local props.
   * @param e
   */
  onClick: function(e) {
    if (Boolean(this.props.onClick) && !this.state.isDisabled && !this.state.isReadOnly) {
      //this.props.isActive = true;
      this.props.onClick(e, this);
    } else {
      e.preventDefault();
    }
  },
  onMouseUp: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isActive:false});
      if (Boolean(this.props.onMouseUp)) {
        this.props.onMouseUp(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onMouseDown: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isActive:true});
      if (Boolean(this.props.onMouseDown)) {
        this.props.onMouseDown(e, this);
      }
    }
  },
  onMouseOver: function(e) {
    if (Boolean(this.props.onMouseOver) && !this.state.isDisabled && !this.state.isReadOnly) {
      this.props.onMouseOver(e, this);
    } else {
      e.preventDefault();
    }
  },
  onMouseEnter: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isHovered:true});
      if (this.props.onMouseEnter) {
        this.props.onMouseEnter(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onMouseLeave: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isHovered:false, isActive:false});
      if (Boolean(this.props.onMouseLeave)) {
        this.props.onMouseLeave(e, this);
      }
    }
  },
  onMouseOut: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      if (Boolean(this.props.onMouseOut)) {
        this.props.onMouseOut(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onTouchCancel: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isActive: false});
      if (Boolean(this.props.onTouchCancel)) {
        this.props.onTouchCancel(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onTouchEnd: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isActive: false});
      if (Boolean(this.props.onTouchEnd)) {
        this.props.onTouchEnd(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onTouchMove: function(e) {
    if (Boolean(this.props.onTouchMove) && !this.state.isDisabled && !this.state.isReadOnly) {
      this.props.onTouchMove(e, this);
    } else {
      e.preventDefault();
    }
  },
  onTouchStart: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isActive: true});
      if (Boolean(this.props.onTouchStart)) {
        this.props.onTouchStart(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onKeyDown: function(e) {
    if (Boolean(this.props.onKeyDown) && !this.state.isDisabled && !this.state.isReadOnly) {
      this.props.onKeyDown(e, this);
    } else {
      e.preventDefault();
    }
  },
  onKeyPress: function(e) {
    if (Boolean(this.props.onKeyPress) && !this.state.isDisabled && !this.state.isReadOnly) {
      this.props.onKeyPress(e, this);
    } else {
      e.preventDefault();
    }
  },
  onKeyUp: function(e) {
    if (Boolean(this.props.onKeyUp) && !this.state.isDisabled && !this.state.isReadOnly) {
      this.props.onKeyUp(e, this);
    } else {
      e.preventDefault();
    }
  },
  onFocus: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isFocused: true});
      if (Boolean(this.props.onFocus)) {
        this.props.onFocus(e, this);
      }
    } else {
      e.preventDefault();
    }
  },
  onBlur: function(e) {
    if (!this.state.isDisabled && !this.state.isReadOnly) {
      this.setState({isFocused: false});
      if (Boolean(this.props.onBlur)) {
        this.props.onBlur(e, this);
      }
    } else {
      e.preventDefault();
    }
  },

  /**
   * Generic clone function that takes an object and returns an independent clone of it.
   * Needed to give the user a clone of the props instead of the props themselves to prevent direct access to the props.
   * @param obj
   * @returns {*}
   **/
  clone: function(obj) {
    if (null == obj || "object" != typeof obj) return obj;
    var copy = obj.constructor();
    for (var attr in obj) {
      if (obj.hasOwnProperty(attr)) copy[attr] = obj[attr];
    }
    return copy;
  }
};