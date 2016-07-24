
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import ReactDOM from 'react-dom';
import d3 from 'd3';
import './metricBarGroup.scss';

class MetricBarGroup extends React.Component {
  constructor(props) {
    super(props);

  }
  componentWillMount() {
    const {...props} = this.props;
    this.margin = {top: 20, right: 50, bottom: 700, left: 100};
    this.width = 1220 - this.margin.left - this.margin.right;
    this.height = 1220 - this.margin.top - this.margin.bottom;
    // this.width = 800;
    // this.height = 600;
    this.x = d3.scale.ordinal()
      .rangeRoundBands([0, this.width], .1);

    this.y = d3.scale.linear()
      .range([this.height, 0]);

    this.xAxis = d3.svg.axis()
      .scale(this.x)
      .orient("bottom");

    this.yAxis = d3.svg.axis()
        .scale(this.y)
        .orient("left")
        .ticks(props.ticks.number, props.ticks.type);
  }
  componentDidMount() {
    let el = document.querySelector('#' + this.props.title + this.props.lp_id);
    this.svg = d3.select(el)
      .append('svg');
    let self = this;

    let totalWidth = this.width + this.margin.left + this.margin.right;
    let totalHeight = this.height + this.margin.top + this.margin.bottom;
    this.svg = this.svg
      .attr("viewBox", "-10 0 " + totalWidth + " " + totalHeight + "")
      .attr("preserveAspectRatio", "xMidYMid meet")
      .style("overflow","visible")
      .append("g")
      .attr("transform", "translate(" + this.margin.left + "," + this.margin.top + ")");

    this.svg.append("g")
        .attr("class", "y axis")
        .call(this.yAxis);
    this.svg.append("g")
        .attr("class", "x axis")
        .attr("transform", "translate(-1," + this.height + ")")
        .call(this.xAxis)
    this.drawBars(this.props);


  }
  componentWillReceiveProps(props) {
    this.drawBars(props);
  }
  drawBars = (props) => {
    let DATA = props.data.sort(function(a,b){
      return (a.id > b.id) ? -1 : 1;
    });
    this.x.domain(DATA.map(function(d, i) { return d.id }));
    let self = this;
    let barGroup = this.svg.selectAll(".barGroup").data(DATA, function(d, i) { return d.id});;
    let barGroupExit = barGroup.exit().remove();
    let barGroupEnter =  barGroup.enter().append('g')
      .attr('class', 'barGroup');
    barGroupEnter
      .append("rect")
      .attr("class", "bar")
      .attr("x", function(d) { return self.x(d.id); })
      .attr("width", self.x.rangeBand())
      .attr("y", function(d) { return self.y(d[props.valueName]); })
      .attr("height", function(d) {return self.height - self.y(d[props.valueName]); });
    barGroupEnter.append("g")
      .attr("transform", function(d){
        return "translate("+ (parseInt(self.x(d.id)) + (self.x.rangeBand() / 2) + 10) +"," + (parseInt(self.height) + 10) + ")"
       })
      .append('text')
      .classed('metriclabel', true)
      .style("text-anchor", "end")
      .attr('transform', 'rotate(-75)')
      .text(function(d) { return d.name;} )

    let barGroupUpdate = barGroup.transition();
    barGroupUpdate.select('rect')
      .attr("class", "bar")
      .attr("x", function(d) { return self.x(d.id) })
      .attr("width", self.x.rangeBand())
      .attr("y", function(d) { return self.y(d[props.valueName]); })
      .attr("height", function(d) {return self.height - self.y(d[props.valueName]); });
    barGroupUpdate
      .select('g')
      .attr("transform", function(d){
        return "translate("+ (parseInt(self.x(d.id)) + (self.x.rangeBand() / 2) + 10) +"," + (parseInt(self.height) + 10) + ")"
       })
      .select('text')
      .style("text-anchor", "end")
      .attr('transform', 'rotate(-75)')
      .text(function(d) { return d.name;} )
  }
  render() {
    let html = <div></div>;
    let self = this;
    return <div className="metricBarGroup"><h3>{this.props.title}</h3><div id={this.props.title + this.props.lp_id}></div></div>;
  }
}
MetricBarGroup.defaultProps = {
  ticks: {
    number: 2,
    type: '%'
  },
  valueName: 'utilization',
  title: '',
  data: []
}


export default MetricBarGroup;
