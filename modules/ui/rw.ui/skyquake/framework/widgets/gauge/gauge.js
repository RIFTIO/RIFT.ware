
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require("react");
var ReactDOM = require('react-dom');
var MIXINS = require("../mixins/ButtonEventListener.js");
var Gauge = require("../../js/gauge-modified.js");
var GUID = require("utils/guid");
import _ from 'underscore'




/**
 *  Gauge Component
 *  It's props values and a brief description below
 *
 *  min:         minimum value expected
 *  max:         maximum value expected
 *  width:       width of gauge in px
 *  height:      height of gauge in px
 *  value:       the number displayed on the gauge
 *  resize:      should the gauge resize with container
 *  unit:        the units displayed on the gauge
 *  valueFormat: An object with an 'int' and 'dec' property. The 'int' is the min number of integer digits displayed
 *                 and the 'dec' object is the min number of fractional digits displayed.
 *
 *
 **/
module.exports = React.createClass({
  displayName: 'Gauge',
  mixins:MIXINS,
  propTypes: {
    min:           React.PropTypes.number,
    max:           React.PropTypes.number,
    width:         React.PropTypes.number,
    height:        React.PropTypes.string,
    value:         React.PropTypes.number,
    resize:        React.PropTypes.bool,
    isAggregate:   React.PropTypes.bool,
    units:         React.PropTypes.string,
    valueFormat:   React.PropTypes.shape({
      'int':         React.PropTypes.number,
      'dec':         React.PropTypes.number
    })
  },
  clone: function(obj) {
      if (null == obj || "object" != typeof obj) return obj;
      var copy = obj.constructor();
      for (var attr in obj) {
          if (obj.hasOwnProperty(attr)) copy[attr] = obj[attr];
      }
      return copy;
  },

  /**
   * Defines default state.
   *
   *  min:         minimum value expected
   *  max:         maximum value expected
   *  nSteps:      fixed number for now. The number of ticks in the gauge.
   *  width:       width of gauge in px
   *  height:      height of gauge in px
   *  value:       the number displayed on the gauge
   *  resize:      should the gauge resize with container
   *  unit:        the units displayed on the gauge
   *  valueFormat: An object with an 'int' and 'dec' property. The 'int' is the min number of integer digits displayed
   *                 and the 'dec' object is the min number of fractional digits displayed.
   */
  getInitialState: function() {
    var valueFormatState = null
    this.gauge = null;
    this.gaugeID = GUID();
    if (!this.props.valueFormat) {
        if ((this.props.max && this.props.max > 1000) || this.props.value) {
            valueFormatState = {
                "int": 1,
                "dec": 0
            };
        } else {
            valueFormatState = {
                "int": 1,
                "dec": 2
            };
        }
    } else {
      valueFormatState = this.props.valueFormat;
    }
    return {
      //sizeOfButton:   this.props.size || '',  //There is no Medium value in CSS, default size is the absence of a value
      min: this.props.min || 0,
      max: this.props.max || 0,
      nSteps: 14,
      height: this.props.height || 200,
      width: this.props.width || 200,
      color: this.props.color || 'hsla(212, 57%, 50%, 1)',
      value: this.props.value || 0,
      valueFormat: valueFormatState,
      isAggregate: this.props.isAggregate || false,
      units: this.props.units || '',
      resize:this.props.resize || false

    }
  },


  /**
   *  Called when props are changed.  Syncs props with state.
   */
  componentWillReceiveProps: function(nextProps) {
    this.setState({
      max:nextProps.max || this.state.max,
      value:nextProps.value || 0,
      valueFormat:nextProps.valueFormat || this.state.valueFormat
    });
  },

  /**
   * Calls the render on the gauge object once the component first mounts
   */
  componentDidMount: function() {
    this.canvasRender(this.state);
  },

  /**
   * If any of the state variables have changed, the component should update.
   * Note, this is where the render step occures for the gauge object.
   */
  shouldComponentUpdate: function(nextProps, nextState) {
    var currentStateString = String(this.state.max) + String(this.state.valueFormat.int) + String(this.state.valueFormat.dec) + String(this.state.value);
    var nextStateString = String(nextState.max) + String(nextState.valueFormat.int) + String(nextState.valueFormat.dec) + String(nextState.value);
    if (currentStateString == nextStateString) {
      return false;
    }
    this.state.valueFormat = this.determineValueFormat(nextState.value);
    this.canvasRender(nextState);
    return true;
  },

  /**
   * Default value format based on units.
   */
  determineValueFormat: function(value) {
          if (value > 999 || this.state.units == "%") {
              return {
                  "int": 1,
                  "dec": 0
              }
          }

          return {
              "int": 1,
              "dec": 2
          }
      },


  /**
   * Render step for the gauge object. Sets some defaults, passes some of the component's state down.
   */
  canvasRender: function(state) {
    if (state.max == state.min) {
        state.max = 14;
    }
    var range = state.max - state.min;
    var step = Math.round(range / state.nSteps);
    var majorTicks = [];
    for (var i = 0; i <= state.nSteps; i++) {
        majorTicks.push(state.min + (i * step));
    }
    var redLine = state.min + (range * 0.9);
    var config = {
        isAggregate: state.isAggregate,
        renderTo: ReactDOM.findDOMNode(document.getElementById(this.gaugeID)),
        width: state.width,
        height: state.height,
        glow: false,
        units: state.units,
        title: false,
        minValue: state.min,
        maxValue: state.max,
        majorTicks: majorTicks,
        valueFormat: this.determineValueFormat(self.value),
        minorTicks: 0,
        strokeTicks: false,
        highlights: [],
        colors: {
            plate: 'rgba(0,0,0,0)',
            majorTicks: 'rgba(15, 123, 182, .84)',
            minorTicks: '#ccc',
            title: 'rgba(50,50,50,100)',
            units: 'rgba(50,50,50,100)',
            numbers: '#fff',
            needle: {
                start: 'rgba(255, 255, 255, 1)',
                end: 'rgba(255, 255, 255, 1)'
            }
        }
    };

    var min = config.minValue;
    var max = config.maxValue;
    var N = 1000;
    var increment = (max - min) / N;
    for (i = 0; i < N; i++) {
        var temp_color = 'rgb(0, 172, 238)';
        if (i > 0.5714 * N && i <= 0.6428 * N) {
            temp_color = 'rgb(0,157,217)';
        } else if (i >= 0.6428 * N && i < 0.7142 * N) {
            temp_color = 'rgb(0,142,196)';
        } else if (i >= 0.7142 * N && i < 0.7857 * N) {
            temp_color = 'rgb(0,126,175)';
        } else if (i >= 0.7857 * N && i < 0.8571 * N) {
            temp_color = 'rgb(0,122,154)';
        } else if (i >= 0.8571 * N && i < 0.9285 * N) {
            temp_color = 'rgb(0,96,133)';
        } else if (i >= 0.9285 * N) {
            temp_color = 'rgb(0,80,112)';
        }
        config.highlights.push({
            from: i * increment,
            to: increment * (i + 2),
            color: temp_color
        })
    }
    var updateSize = _.debounce(function() {
        config.maxValue = state.max;
    }, 500);
    if (state.resize) $(window).resize(updateSize)

    if (this.gauge) {
        this.gauge.setValue(Math.ceil(state.value* 100) / 100)
        this.gauge.updateConfig(config);
    } else {
        this.gauge = new Gauge(config);
        this.gauge.setValue(Math.ceil(state.value* 100) / 100)
        this.gauge.draw();
    }
  },

  /**
   * Renders the Gauge Component
   * Returns the canvas element the gauge will be housed in.
   * @returns {*}
   */
  render: function() {
    var gaugeDOM = React.createElement("div", null,
      React.createElement("canvas",
        {className: "rwgauge", style:
          {width:'100%','maxWidth':this.state.width + 'px','maxHeight':this.state.width},
          id:this.gaugeID
        }
      )
    )



    return gaugeDOM;
  }
});
