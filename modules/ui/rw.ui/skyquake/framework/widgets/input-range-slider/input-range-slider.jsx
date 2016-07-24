
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
// import Slider from 'react-rangeslider';
// import Slider from './react-rangeslider.jsx';
import './input-range-slider.scss';


class RWslider extends React.Component {
  constructor(props, context) {
    super(props, context);
    this.state = {...props}
  }

  handleChange = (value) => {
    this.props.handleInputUpdate(value);
    this.setState({
      value: value
    });
  };

  render() {
    let state = this.state;
      var className = "input-range-slider_" + this.props.orientation;
    return (
      <div className={className}>
      <div className={className + '-info'}>
      {state["min-value"]}
      </div>
      <Slider
        displayValue={true}
        value={state.value}
        max={state["max-value"]}
        min={state["min-value"]}
        step={state["step-value"]}
        onChange={this.handleChange}
        className={className + '-info'} />
      <div className={className + '-info'}>
      {state["max-value"]}
      </div>
      </div>
    );
  }
}

RWslider.defaultProps = {
    value: 10,
    "min-value": 0,
    "max-value":100,
    "step-value":1,
    "units": "%",
    orientation: "horizontal"
}

export default RWslider

