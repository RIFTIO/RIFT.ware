
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';

//Currently displays all buffer state messages. Should consider showing only the n most recent.
//TODO remove loader when current state is running
//TODO need to look at refactoring this
class ThrottledMessage extends React.Component {

  constructor(props) {
    super(props);
    let self = this;
    this.displayBuffer = [];
    this.bufferInterval;
    this.last_id = null;
    this.state = {};
    this.state.loading = props.loading;
    this.state.buffer = {};
    this.state.displayMessage = 'Loading...'
  }

  componentWillReceiveProps(nextProps) {
    let buffer = nextProps.buffer;
    if(buffer.length) {
      this.buildBufferObject(nextProps);
      this.bufferIt(this.props);
    }
  }

  componentDidMount() {
    if(this.props.buffer.length) {
      let buffer = this.props.buffer;
      this.buildBufferObject(this.props);
      this.bufferIt(this.props);
    }
  }
  componentWillUnmount() {
    clearInterval(this.bufferInterval);
  }
  buildBufferObject(props) {
    let self = this;
    let buffer = self.state.buffer;
    this.last_id = props.buffer[props.buffer.length -1].id;
    props.buffer.map(function(item) {
      if(!buffer[item.id]) {
        buffer[item.id] = {
          displayed: false,
          data: item
        }
        self.displayBuffer.push(buffer[item.id]);
      }
    });
  }

  bufferIt(props) {
    let self = this
    clearInterval(self.bufferInterval);
    self.bufferInterval = setInterval(function() {
      let currentStatus = self.props.currentStatus;
      let failed = currentStatus == 'failed';
      for (let i = 0; i < self.displayBuffer.length; i++) {
        if(!self.displayBuffer[i].displayed) {
          self.displayBuffer[i].displayed = true;
          let displaymsg;
          if(failed) {
            displaymsg = self.displayBuffer[self.displayBuffer.length-1].data.description;
            clearInterval(self.bufferInterval);
                self.props.onEnd(failed);
          } else {
            displaymsg = self.displayBuffer[i].data.description;
          }
          self.setState({
              displayMessage: displaymsg
          });
          break;
        }
      }

      if((currentStatus == 'running' || currentStatus == 'started' || currentStatus == 'stopped' || failed) && self.displayBuffer[self.displayBuffer.length - 1].displayed ) {
                clearInterval(self.bufferInterval);
                self.props.onEnd(failed);
        }
    }, 600)
  }
  render() {
    if(!this.props.hasFailed) {
      return (<span className='throttledMessageText'>{this.state.displayMessage}</span>)
    } else {
      return (<span> </span>)
    }
  }
}

ThrottledMessage.defaultProps = {
  buffer: []
}

export default class operationalStatus extends React.Component {
  constructor(props) {
    super(props);
    this.state = {};
    this.state.message = 'Loading...';
    this.state.messageHistory = {};
  }
  componentWillReceiveProps(nextProps) {

  }
  finishedMessages(){
    this.setState({
      loading: false
    })
  }
  statusMessage(currentStatus, currentStatusDetails) {
    var message = currentStatus;
    if (currentStatusDetails) {
       message += ' - ' + currentStatusDetails;
    }
    return message;
  }
  render() {
    let html;
    let isDisplayed = this.props.display;
    let isFailed = (this.props.currentStatus == 'failed') || false;
    let title = (!this.props.loading || isFailed) ? <h2>History</h2> : '';
    let status = this.props.status.map(function(status, index) {
      return (
        <li key={index}>{status.description}</li>
      )
    }).reverse();
    if(this.props.loading) {
      if (!isFailed) {
        isDisplayed = true;
        //If there is no collection of status event message, just display currentStatus
        if(status.length) {
              html = (
                      <div className={this.props.className + '_loading'}>
                        <Loader show={!isFailed}/>
                        <ThrottledMessage currentStatus={this.props.currentStatus} buffer={this.props.status} onEnd={this.props.doneLoading}/>
                      </div>
              )
        } else {
          html = (
                      <div className={this.props.className + '_loading'}>
                        <Loader show={!isFailed}/>
                        {this.statusMessage(this.props.currentStatus,this.props.currentStatusDetails)}
                      </div>
              )
        }
      } else {
          isDisplayed = true;
              html = (

                        <ul>
                        <ThrottledMessage currentStatus={this.props.currentStatus} buffer={this.props.status} onEnd={this.props.doneLoading} hasFailed={isFailed}/>
                          {status}
                        </ul>
              )
      }
    } else {
      html = (
          <ul>
            {status}
          </ul>
      )
    }
    return (<div className={this.props.className + (isDisplayed ? '_open':'_close')}>{title} {html}</div>);
  }
}

operationalStatus.defaultProps = {
  status: [],
  loading: true,
  display: false
}



