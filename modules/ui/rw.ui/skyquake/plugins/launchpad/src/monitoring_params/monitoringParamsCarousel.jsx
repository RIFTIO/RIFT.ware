
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import Carousel from '../carousel/carousel-react.jsx';
import SkyquakeCarousel from '../carousel/skyquakeCarousel.jsx';
import './monitoring_params.scss';
import ParseMP from './monitoringParamComponents.js';
//TODO
// This Carousel needs to be refactored so that we can sanely add a unique key to each element.
// The entire component needs some serious refactoring.

export default class launchpadCarousel extends React.Component {
    constructor(props){
        super(props);
    }
    componentWillMount(){
        // window.addEventListener('resize', resizeComponents.bind(this))
    }
    componentWillUnmount(){
        // window.removeEventListener('resize', resizeComponents.bind(this))
    }
  render() {
    var slideno = this.props.slideno;
    if(this.props.component_list) {
        var components = ParseMP.call(this, this.props.component_list);
       return <Carousel slideno={this.props.slideno} component_list={components} />
       // return <SkyquakeCarousel slideno={this.props.slideno} slides={components} />
    } else {
        return <div className="empty"> MONITORING PARAMETERS NOT LOADED</div>
    }
  }
}


