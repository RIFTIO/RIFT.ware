
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * https://github.com/larsmaultsby/react-rangeslider
 *
 *
 * Forked from: https://github.com/whoisandie/react-rangeslider
 *
 *
    The MIT License (MIT)

    Copyright (c) 2015 Bhargav Anand

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
 *
 */

import React, { PropTypes, Component, findDOMNode } from 'react';
import joinClasses from 'react/lib/joinClasses';

function capitalize(str) {
  return str.charAt(0).toUpperCase() + str.substr(1);
}

function maxmin(pos, min, max) {
  if (pos < min) { return min; }
  if (pos > max) { return max; }
  return pos;
}

const constants = {
  orientation: {
    horizontal: {
      dimension: 'width',
      direction: 'left',
      coordinate: 'x',
    },
    vertical: {
      dimension: 'height',
      direction: 'top',
      coordinate: 'y',
    }
  }
};

class Slider extends Component {
  static propTypes = {
    min: PropTypes.number,
    max: PropTypes.number,
    step: PropTypes.number,
    value: PropTypes.number,
    orientation: PropTypes.string,
    onChange: PropTypes.func,
    className: PropTypes.string,
  }

  static defaultProps = {
    min: 0,
    max: 100,
    step: 1,
    value: 0,
    orientation: 'horizontal',
  }

  state = {
    limit: 0,
    grab: 0
  }

  // Add window resize event listener here
  componentDidMount() {
    this.calculateDimensions();
    window.addEventListener('resize', this.calculateDimensions);
  }

  componentWillUnmount() {
    window.removeEventListener('resize', this.calculateDimensions);
  }

  handleSliderMouseDown = (e) => {
    let value, { onChange } = this.props;
    if (!onChange) return;
    value = this.position(e);
    onChange && onChange(value);
  }

  handleKnobMouseDown = () => {
    document.addEventListener('mousemove', this.handleDrag);
    document.addEventListener('mouseup', this.handleDragEnd);
  }

  handleDrag = (e) => {
    let value, { onChange } = this.props;
    if (!onChange) return;
    value = this.position(e);
    onChange && onChange(value);
  }

  handleDragEnd = () => {
    document.removeEventListener('mousemove', this.handleDrag);
    document.removeEventListener('mouseup', this.handleDragEnd);
  }

  handleNoop = (e) => {
    e.stopPropagation();
    e.preventDefault();
  }

  calculateDimensions = () => {
    let { orientation } = this.props;
    let dimension = capitalize(constants.orientation[orientation].dimension);
    const sliderPos = findDOMNode(this.refs.slider)['offset' + dimension];
    const handlePos = findDOMNode(this.refs.handle)['offset' + dimension]
    this.setState({
      limit: sliderPos - handlePos,
      grab: handlePos / 2,
    });
  }
  getPositionFromValue = (value) => {
    let percentage, pos;
    let { limit } = this.state;
    let { min, max } = this.props;
    percentage = (value - min) / (max - min);
    pos = Math.round(percentage * limit);

    return pos;
  }

  getValueFromPosition = (pos) => {
    let percentage, value;
    let { limit } = this.state;
    let { orientation, min, max, step } = this.props;
    percentage = (maxmin(pos, 0, limit) / (limit || 1));

    if (orientation === 'horizontal') {
      value = step * Math.round(percentage * (max - min) / step) + min;
    } else {
      value = max - (step * Math.round(percentage * (max - min) / step) + min);
    }

    return value;
  }

  position = (e) => {
    let pos, value, { grab } = this.state;
    let { orientation } = this.props;
    const node = findDOMNode(this.refs.slider);
    const coordinateStyle = constants.orientation[orientation].coordinate;
    const directionStyle = constants.orientation[orientation].direction;
    const coordinate = e['client' + capitalize(coordinateStyle)];
    const direction = node.getBoundingClientRect()[directionStyle];

    pos = coordinate - direction - grab;
    value = this.getValueFromPosition(pos);

    return value;
  }

  coordinates = (pos) => {
    let value, fillPos, handlePos;
    let { limit, grab } = this.state;
    let { orientation } = this.props;
    value = this.getValueFromPosition(pos);
    handlePos = this.getPositionFromValue(value);

    if (orientation === 'horizontal') {
      fillPos = handlePos + grab;
    } else {
      fillPos = limit - handlePos + grab;
    }

    return {
      fill: fillPos,
      handle: handlePos,
    };
  }

  render() {
    let dimension, direction, position, coords, fillStyle, handleStyle, displayValue;
    let { value, orientation, className } = this.props;

    dimension = constants.orientation[orientation].dimension;
    direction = constants.orientation[orientation].direction;

    position = this.getPositionFromValue(value);
    coords = this.coordinates(position);

    fillStyle = {[dimension]: `${coords.fill}px`};
    handleStyle = {[direction]: `${coords.handle}px`};

    if(this.props.displayValue) {
      displayValue = <div>{this.props.value}</div>;
    }

    return (
      <div
        ref="slider"
        className={joinClasses('rangeslider ', 'rangeslider-' + orientation, className)}
        onMouseDown={this.handleSliderMouseDown}
        onClick={this.handleNoop}
        style={{display:'flex'}}>
        <div
          ref="fill"
          className="rangeslider__fill"
          style={fillStyle} />
        <div
          ref="handle"
          className="rangeslider__handle"
          onMouseDown={this.handleKnobMouseDown}
          onClick={this.handleNoop}
          style={handleStyle}>
            {displayValue}
          </div>
      </div>
    );
  }
}

export default Slider;
