
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var React = require('react');
var Slider = require('react-slick');
// require('../../components/gauge/gauge.js');
// require('../../components/text-area/rw.text-area.js');
// require('../../components/test/multicomponent.js');
import button from '../../components/components.js'

require('./carousel.css');
var SimpleSlider = React.createClass({
  propTypes: {
    component_list:           React.PropTypes.array.isRequired,
    slideno:                  React.PropTypes.number
  },
  handleClick: function() {
    this.setState({});
  },
  getInitialState: function() {
    return {
      }
    
  },
  shouldComponentUpdate: function(nextProps) {

    if (nextProps.slideno != this.props.slideno) {
      return true;
    }
    return false;
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
        dots: false,
        infinite: false,
        speed: 500,
        slidesToShow: 1,
        slidesToScroll: 1,
        centerMode: true,
        initialSlide: this.props.slideno || 0
    }
    setTimeout(function() {
      window.dispatchEvent(new Event('resize'));
    }, 1000)
    var list = [];
    if (this.props.component_list !== undefined) {
      for (var i = 0; i < this.props.component_list.length; i++) {
        list.push(<div key={i}  className={"component"}>{this.props.component_list[i]}</div>);
      }
    }
    return (
      <div>
      <Slider {...settings}>
        {list}
      </Slider>
      </div>o
    );
  }
});
module.exports = SimpleSlider;
