/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';

function debugDump(funcName, state, props) {
      console.log("UpTime." + funcName + ": called");
      console.log(" -> props = ", props);
      console.log(" -> state = ", state);
    }

/**
 * Uptime Compoeent
 * Accepts two properties:
 * initialtime {number} number of milliseconds.
 * run {boolean} determines whether the uptime ticker should run
 */
export default class UpTime extends React.Component {

    constructor(props) {
      super(props);
      if (props.debugMode) {
        console.log("UpTime.constructor called");
      }
      let initialTime = Math.floor(props.initialTime);
      this.state = {
        run: props.run,
        initialTime: initialTime,
        time: this.handleConvert(this.calculateDuration({
          intialTime: initialTime
        })),
        noisySeconds: props.noisySeconds,
        debugMode: props.debugMode
      }
      this.tick;
      this.handleConvert = this.handleConvert.bind(this);
      this.updateTime = this.updateTime.bind(this);
      if (props.debugMode) {
        debugDump("constructor", this.state, props);
      }
    }

    componentWillReceiveProps(nextProps) {
      if (this.state.debugMode) {
        debugDump("componentWillReceiveProps", this.state, nextProps);
      }
      let initialTime = Math.floor(nextProps.initialTime);
      if (initialTime > this.state.initialTime) {
        initialTime = this.state.initialTime;
      }
      this.state = {
        initialTime: initialTime,
        time: this.handleConvert(this.calculateDuration({
          intialTime: initialTime
        }))
      }
    }

    calculateDuration(args={}) {
      let initialTime = args.initialTime;
      if (!initialTime) {
        initialTime =  this.state && this.state.initialTime
          ? this.state.initialTime
          : 0;
      }
      let timeNow = args.timeNow ? args.timeNow : Date.now();
      if (initialTime) {
        return Math.floor((timeNow/ 1000)) - initialTime;
      } else {
        return 0;
      }
    }

    handleConvert(input) {
      var ret = {
        days: 0,
        hours: 0,
        minutes: 0,
        seconds: 0
      };
      if (input == "inactive" || typeof(input) === 'undefined') {
        ret.seconds = -1;
      } else if (input !== "" && input != "Expired") {
        input = Math.floor(input);
        ret.seconds = input % 60;
        ret.minutes = Math.floor(input / 60) % 60;
        ret.hours = Math.floor(input / 3600) % 24;
        ret.days = Math.floor(input / 86400);
      }
      return ret;
    }

    toString() {
        var self = this;
        var ret = "";
        var unitsRendered = [];

        if (self.state.time.days > 0) {
          ret += self.state.time.days + "d:";
          unitsRendered.push("d");
        }

        if (self.state.time.hours > 0 || unitsRendered.length > 0) {
          ret += self.state.time.hours + "h:";
          unitsRendered.push("h");
        }

        if (self.state.time.minutes > 0 || unitsRendered.length > 0) {
          ret += self.state.time.minutes + "m:";
          unitsRendered.push("m");
        }

        if (self.props.noisySeconds || unitsRendered.length == 0
            || unitsRendered.indexOf('m') == 0)
        {
            // console.log(" -> toString adding seconds: ", self.state.time.seconds);
            ret += self.state.time.seconds + "s";
        }

        if (ret.endsWith(':')) {
          ret = ret.slice(0,-1);
        }
        if (ret.length > 0) {
          return ret;
        } else {
          return "--";
        }
    }

    updateTime() {
      if (this.state.initialTime) {
        this.setState({
          time: this.handleConvert(this.calculateDuration())
        });
      }
    }

    componentDidMount() {
      var self = this;
      if (self.state.run) {
        clearInterval(self.tick);
        self.tick = setInterval(self.updateTime, 250);
      }
    }

    componentWillUnmount() {
      clearInterval(this.tick);
    }

    render() {
      return (<span>{this.toString()}</span>);
    }
}
UpTime.defaultProps = {
  noisySeconds: false,
  caller: '',
  systemId: '' // System identifyer for uptime (e.g.: VM, NS, or process)
}

