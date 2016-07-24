
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require('react');
var Slider = require('react-slick');
//import SkyquakeCarousel from './skyquakeCarousel.jsx';
//This componenet should not be coupled with launchpad

import button from 'widgets/button/rw.button.js'
var LaunchpadFleetStore = require('../launchpadFleetStore.js');
var LaunchpadFleetActions = require('../launchpadFleetActions.js');

require('./carousel.css');
var SimpleSlider = React.createClass({
  propTypes: {
    component_list:           React.PropTypes.array.isRequired,
    slideno:                  React.PropTypes.number,
    externalChangeSlide:      React.PropTypes.bool
  },
  handleClick: function() {
    this.setState({});
  },
  getDefaultProps: function() {
    this.externalChangeSlide = false;
    return {
      externalChangeSlide: false
    }
  },
  getInitialState: function() {
    return {
      }

  },
  changeSlide: function(data) {
    if (data.slideChange > 0) {
      var setSlide = -1;
      for (var i = 0; i < this.props.component_list.length; i++) {
        var component = this.props.component_list[i].component;
        if (Object.prototype.toString.call(component) === "[object Array]") {
          for (var j = 0; j < component.length; j++) {
            var subcomponent = component[j];
            if (subcomponent.props.mp === data.dropdownSlide[0]) {
              setSlide = i;
            }
          }
        } else {
            if (component.props.mp === data.dropdownSlide[0]) {
              setSlide = i;
            }
          }
      }
      if (setSlide < 0) {
        return;
      }
      this.externalChangeSlide = true;
      this.setState({slideno: parseFloat(setSlide)});
    }
  },
  componentDidMount: function() {
    // LaunchpadFleetStore.listen(this.changeSlide);
  },
  componentWillUnmount: function() {
  },
  shouldComponentUpdate: function(nextProps) {

    return true;
    // This prevents things in the carousel from updating which makes no sense because we're displaying metrics that need to update
    // if (nextProps.slideno != this.props.slideno) {
    //   return true;
    // }
    // return false;
  },
  componentDidUpdate: function(prevProps, prevState) {
    if (this.externalChangeSlide > 0) {
      this.externalChangeSlide = false;
      // LaunchpadFleetActions.slideNoStateChangeSuccess();
    }
  },
  handleResize: function(e) {

  },
  render: function () {
    // var settings = {
    //   dots: true,
    //   infinite: false,
    //   speed: 500,
    //   slidesToShow: 1,
    //   slidesToScroll: 1,
    //   centerMode: true,
    //   initialSlide: this.props.slideno || 2
    // };
    var settings = {
        dots: true,
        infinite: false,
        speed: 500,
        slidesToShow: 1,
        slidesToScroll: 1,
        centerMode: true,
        changeSlide: this.externalChangeSlide,
        initialSlide: this.state.slideno || 0
    }
    if (this.externalChangeSlide) {
      settings.initialSlide = this.state.slideno;
    }
    setTimeout(function() {
      window.dispatchEvent(new Event('resize'));
    }, 0)
    var list = [];
    if (this.props.component_list !== undefined) {
      for (var i = 0; i < this.props.component_list.length; i++) {
        let title = this.props.component_list[i].title;
        let displayTitle = title ? 'inherit' : 'none';
        list.push(<div key={i}  className={"component"}><h2 style={{flex: '0 1 auto', justifyContent: 'center', alignSelf: 'center', display: displayTitle, padding: '0.5rem'}}>{title}</h2><div className="componentWrapper">{this.props.component_list[i].component}</div></div>);
      }
    }

    return (
      <div className={list.length > 1 ? '' : 'hideButtons'}>
        <Slider {...settings}>
          {list}
        </Slider>
      </div>
    );
  }
});
module.exports = SimpleSlider;
