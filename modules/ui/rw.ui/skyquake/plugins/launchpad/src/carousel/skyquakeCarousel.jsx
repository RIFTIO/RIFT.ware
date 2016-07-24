
var defaultSlideData = [{"id":1,"group-tag":"Group1-0","units":"packets","name":"ping-request-rx-count","value-type":"INT","http-endpoint-ref":"api/v1/pong/stats","widget-type":"COUNTER","description":"no of ping requests","value-integer":2538459,"json-query-method":"NAMEKEY","vnfr-id":"39b56173-be82-4f87-872a-eaa761b36281","mp-id":"1-39b56173-be82-4f87-872a-eaa761b36281","vnfr-name":"pong_vnfd"},{"id":2,"group-tag":"Group1-0","units":"packets","name":"ping-response-tx-count","value-type":"INT","http-endpoint-ref":"api/v1/pong/stats","widget-type":"COUNTER","description":"no of ping responses","value-integer":2538459,"json-query-method":"NAMEKEY","vnfr-id":"39b56173-be82-4f87-872a-eaa761b36281","mp-id":"2-39b56173-be82-4f87-872a-eaa761b36281","vnfr-name":"pong_vnfd"},{"id":1,"group-tag":"Group1-1","units":"packets","name":"ping-request-tx-count","value-type":"INT","http-endpoint-ref":"api/v1/ping/stats","widget-type":"COUNTER","description":"no of ping requests","value-integer":2538460,"json-query-method":"NAMEKEY","vnfr-id":"9f0c4f07-f5e2-4189-aa25-acee550eb02b","mp-id":"1-9f0c4f07-f5e2-4189-aa25-acee550eb02b","vnfr-name":"ping_vnfd"},{"id":2,"group-tag":"Group1-1","units":"packets","name":"ping-response-rx-count","value-type":"INT","http-endpoint-ref":"api/v1/ping/stats","widget-type":"COUNTER","description":"no of ping responses","value-integer":2538455,"json-query-method":"NAMEKEY","vnfr-id":"9f0c4f07-f5e2-4189-aa25-acee550eb02b","mp-id":"2-9f0c4f07-f5e2-4189-aa25-acee550eb02b","vnfr-name":"ping_vnfd"}];

import React from 'react';
import ReactDOM from 'react-dom';

require('./skyquakeCarousel.scss');

export default class SkyquakeCarousel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
        this.state.slides = this.constructSlides(props.slides);
        //current slide number. can eventually be used with initial construct slides function to specify initial slide. construct slides currently defaults to slide 1 as initial.
        this.state.currentSlide = props.currentSlide;
        //Ref handles. Index coresponds to slide position. Last slide is overflowed left in dom with an order of 1;
        this.state.refsArray = this.state.slides.map(function(v, i) {
            return v.ref
        });
    }
    componentWillReceiveProps(props) {
        console.log(props);
        let slides = this.constructSlides(props.slides);
        console.log('refs', this.refs);
        this.setState({
            slides: slides
        });

    }
    constructSlides(data){
        let slides = [];
        let len = data.length;
        let classItems = 'slide';
        return data.map(function(v, i) {
            let slideOrder = i + 2;
            let isRef = '';
            let ref;
            if (slideOrder > len) {
                isRef = ' is-ref';
                slideOrder = 1;
            }
            ref = 'slide-' + (i+1);
            // return (<div key={i} ref={ref} className={classItems + isRef} style={{order: slideOrder}}>{v}</div>);
            return (<div key={i} className={classItems + isRef} style={{order: slideOrder}}>{v}</div>);
        });
    }
    reorderSlides(activeSlideNo) {
        let self = this;
        let newSlideRefs = [];
        let newSlideRefsLen;
        let containerNode = ReactDOM.findDOMNode(self.refs['container']);
        newSlideRefs = reorder(this.state.refsArray, activeSlideNo - 1);
        newSlideRefsLen = newSlideRefs.length;
        //updates Dom via ref hanldes to the individual slides.
        newSlideRefs.map(function(v, i) {
            let element = ReactDOM.findDOMNode(containerNode.refs[v]);
            if (newSlideRefsLen > (i + 1)) {
                element.style.order = i + 2;
                //class handles. currently no purpose
                element.classList.remove('is-ref');
            } else {
                element.style.order = 1;
                //class handles. currently no purpose
                element.classList.add('is-ref');
            }
        })

        function reorder(data, index) {
          return data.slice(index).concat(data.slice(0, index))
        };
    }
    changeSlide(dir, e) {
        let self = this;
        let containerNode = ReactDOM.findDOMNode(self.refs['container']);
        let total_len = this.props.slides.length;
        switch(dir) {
            case 'previous': prev.call(this); break;
            case 'next': next.call(this); break;
        }
         containerNode.classList.remove('is-set');
            setTimeout(function() {
                containerNode.classList.add('is-set');
        }, 100)
        function prev() {
            let nextSlide;
            let currentSlide = self.state.currentSlide;
            containerNode.classList.add('is-reversing');
            if(currentSlide == 1) {
               nextSlide = total_len
            } else {
                nextSlide = currentSlide - 1;
            }
            updateSlide(nextSlide);
        }
        function next() {
            let nextSlide;
            let currentSlide = self.state.currentSlide;
            containerNode.classList.remove('is-reversing');
            if(currentSlide == total_len) {
               nextSlide = 1
            } else {
                nextSlide = currentSlide + 1;
            }
            updateSlide(nextSlide);

        }
        function updateSlide(slide) {
            self.reorderSlides(slide)
            self.setState({
                    currentSlide: slide
            });
        }
    }
  render() {
    // console.log('this', this);
    // console.log('refs', this.refs);
    // console.log('props',this.props);
    // console.log('state',this.state);
    return (
      <div className="carousel-component">
        <div>{this.state.currentSlide}</div>
        <button onClick={this.changeSlide.bind(this, 'previous')}>Prev</button>
        <button onClick={this.changeSlide.bind(this, 'next')}>Next</button>
        <div className="carousel-container is-set">
            {this.state.slides}
        </div>
      </div>
    );
  }
}

SkyquakeCarousel.defaultProps = {
    slides: [],
    // slides: [(<div className="slide">slide 1</div>),(<div className="slide">slide 2</div>),(<div className="slide">slide 3</div>),(<div className="slide is-ref" ref="last">slide 4</div>)],
    currentSlide: 1
}

SkyquakeCarousel.displayName = 'SkyquakeCarousel';

/*

<div className="carousel-container is-set" ref="container">
            {this.state.slides}
        </div>

 */

// Uncomment properties you need
// CarouselComponent.propTypes = {};
// CarouselComponent.defaultProps = {};

