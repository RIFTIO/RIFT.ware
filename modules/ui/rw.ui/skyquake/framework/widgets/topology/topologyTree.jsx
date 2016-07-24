
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import ReactDOM from 'react-dom';
import d3 from 'd3';
import DashboardCard from '../dashboard_card/dashboard_card.jsx';
import _ from 'lodash';
import $ from 'jquery';
import './topologyTree.scss';

export default class TopologyTree extends React.Component {
    constructor(props) {
        super(props);
        this.data = props.data;
        this.selectedID = 0;
        this.nodeCount = 0;
        this.size = this.wrapperSize();
        this.tree = d3.layout.tree()
            .size([this.props.treeHeight, this.props.treeWidth]);
        this.diagonal = d3.svg.diagonal()
            .projection(function(d) { return [d.y, d.x]; });
        this.svg = null;
    }
    componentWillReceiveProps(props) {
        let self = this;
        if(!this.svg) {
            let zoom = d3.behavior.zoom()
                .translate([this.props.maxLabel, 0])
                .scaleExtent([this.props.minScale, this.props.maxScale])
                .on("zoom", self.zoom);
            let svg = this.selectParent().append("svg")
                    .attr("width", this.size.width)
                    .attr("height", this.size.height)
                .append("g")
                    .call(zoom)
                .append("g")
                    .attr("transform", "translate(" + this.props.maxLabel + ",0)");

            svg.append("rect")
                .attr("class", "overlay")
                .attr("width", this.size.width)
                .attr("height", this.size.height);
            // this.svg = d3.select()
            this.svg = svg;
            this.props.selectNode(props.data);
        }
        if(props.data.hasOwnProperty('type') && !this.props.hasSelected) {
            this.selectedID = props.data.id;
            //Commenting out to prevent transmitter push error
            //this.props.selectNode(props.data);
        }
        if(this.svg) {
          this.update(_.cloneDeep(props.data));
          // this.selectedID = props.data.id;
        }
    }

    wrapperSize() {
        if (this.props.useDynamicWrapperSize) {
            try {
                let wrapper = $(".topologyTreeGraph-body");

                return {
                    width: wrapper.width(),
                    height: wrapper.height()
                }
            } catch (e) {
                console.log("ERROR: cannot get width and/or height from element."+
                    " Using props for width and height. e=", e);
                return {
                    width: this.props.width,
                    height: this.props.height
                }
            }
        } else {
            return {
                width: this.props.width,
                height: this.props.height
            }
        }
    }
    selectParent() {
        return d3.select(document.querySelector('#topology'));
    }
    computeRadius(d) {
        // if(d.parameters && d.parameters.vcpu) {
        //     return this.props.radius + d.parameters.vcpu.total;
        // } else {
            return this.props.radius;
        // }
    }
    click = (d) => {
        this.props.selectNode(d);
        this.selectedID = d.id;
        // if (d.children){
        //     d._children = d.children;
        //     d.children = null;
        // }
        // else{
        //     d.children = d._children;
        //     d._children = null;
        // }
        // this.update(d);
    }
    zoom = () => {
        this.svg.attr("transform", "translate(" + d3.event.translate +
            ")scale(" + d3.event.scale + ")");
    }
    update = (source) => {
        // Compute the new tree layout.
        var svg = this.svg;
        var nodes = this.tree.nodes(source).reverse();
        var links = this.tree.links(nodes);
        var self = this;

        // Normalize for fixed-depth.
        nodes.forEach(function(d) { d.y = d.depth * self.props.maxLabel; });
        // Update the nodes…
        var node = svg.selectAll("g.node")
            .data(nodes, function(d){
                return d.id || (d.id = ++self.nodeCount);
            });
        // Enter any new nodes at the parent's previous position.
        var nodeEnter = node.enter()
            .append("g")
            .attr("class", "node")
            .attr("transform", function(d){
                return "translate(" + source.y0 + "," + source.x0 + ")"; })
            .on("click", this.click);

        nodeEnter.append("circle")
            .attr("r", 0)
            .style("fill", function(d){
                return d._children ? "lightsteelblue" : "white";
            });

        nodeEnter.append("text")
            .attr("x", function(d){
                var spacing = self.computeRadius(d) + 5;
                    return d.children || d._children ? -spacing : spacing; })
            .attr("transform", function(d, i) {
                    return "translate(0," + ((i%2) ? 15 : -15) + ")"; })
            .attr("dy", "3")
            .attr("text-anchor", function(d){
                    return d.children || d._children ? "end" : "start";
                })
            .text(function(d){ return d.name; })
            .style("fill-opacity", 0);

        // Transition nodes to their new position.
        var nodeUpdate = node
            .transition()
            .duration(this.props.duration)
            .attr("transform", function(d) {
                return "translate(" + d.y + "," + d.x + ")"; });

        nodeUpdate.select("circle")
            .attr("r", function(d){ return self.computeRadius(d); })
            .style("fill", function(d) {
                return d.id == self.selectedID ? "green" : "lightsteelblue"; });

        nodeUpdate.select("text")
            .style("fill-opacity", 1)
            .style("font-weight", function(d) {
                return d.id == self.selectedID ? "900" : "inherit";
            })
            .attr("transform", function(d, i) {
                return "translate(0," + ((i%2) ? 15 : -15) + ")" +
                    (d.id == self.selectedID ? "scale(1.125)" : "scale(1)");
            });

        // Transition exiting nodes to the parent's new position.
        var nodeExit = node.exit()
            .transition()
            .duration(this.props.duration)
            .attr("transform", function(d) {
                return "translate(" + source.y + "," + source.x + ")"; })
            .remove();

        nodeExit.select("circle").attr("r", 0);
        nodeExit.select("text").style("fill-opacity", 0);

        // Update the links…
        var link = svg.selectAll("path.link")
            .data(links, function(d){ return d.target.id; });

        // Enter any new links at the parent's previous position.
        link.enter().insert("path", "g")
            .attr("class", "link")
            .attr("d", function(d){
                var o = {x: source.x0, y: source.y0};
                return self.diagonal({source: o, target: o});
            });

        // Transition links to their new position.
        link
            .transition()
            .duration(this.props.duration)
            .attr("d", self.diagonal);

        // Transition exiting nodes to the parent's new position.
        link.exit()
            .transition()
            .duration(this.props.duration)
            .attr("d", function(d){
                var o = {x: source.x, y: source.y};
                return self.diagonal({source: o, target: o});
            })
            .remove();

        // Stash the old positions for transition.
        nodes.forEach(function(d){
            d.x0 = d.x;
            d.y0 = d.y;
        });
    }
    render() {
        let html = (
            <DashboardCard  className="topologyTreeGraph" showHeader={true} title="Topology Tree"
                headerExtras={this.props.headerExtras} >
                <div id="topology"></div>
            </DashboardCard>
        );
        return html;
    }
}

TopologyTree.defaultProps = {
    treeWidth: 800,
    treeHeight: 500,
    width: 800,
    height: 800,
    minScale: 0.5,
    maxScale: 2,
    maxLabel: 150,
    duration: 500,
    radius: 5,
    useDynamicWrapperSize: false,
    data: {}
}
