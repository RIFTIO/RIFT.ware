
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import './crash.scss';
import TreeView from 'react-treeview';
import '../node_modules/react-treeview/react-treeview.css';
import AppHeader from 'widgets/header/header.jsx';
var crashActions = require('./crashActions.js');
var crashStore = require('./crashStore.js');
// var MissionControlStore = require('../missioncontrol/missionControlStore.js');
function openDashboard() {
  window.location.hash = "#/";
}
class CrashDetails extends React.Component {
  constructor(props) {
    super(props)
    var self = this;
    this.state = crashStore.getState();
    crashStore.listen(this.storeListener);
  }
  storeListener = (data) => {
     this.setState({
        list:data.crashList,
        noDebug:!this.hasDebugData(data.crashList)
      });
  }
  componentWillUnmount(){
    crashStore.unlisten(this.storeListener);
  }
  componentWillMount() {
    crashStore.get();
  }
  hasDebugData(list) {
    console.log(list);
    if (list && list.length > 0) {
      for (let i = 0; i < list.length; i++) {
        var trace = list[i].backtrace;
        for (let j = 0; j < trace.length; j++) {
          console.log(trace[j])
          if (trace[j].detail) {
            return true;
          }
        }
      }
    }
    return false;
  }
  downloadFile(fileName, urlData) {
    var replacedNewLines = urlData.replace(/\\n/g, '\n');
    var replacedTabs = replacedNewLines.replace(/\\t/g, '\t');
    var replacedQuotes= replacedTabs.replace(/\\"/g, '"');
    var textFileBlob = new Blob([replacedQuotes], {type: 'text/plain;charset=UTF-8'});
    var aLink = document.createElement('a');
    var evt = document.createEvent("HTMLEvents");
    evt.initEvent("click");
    aLink.download = fileName;
    aLink.href = window.URL.createObjectURL(textFileBlob);
    aLink.dispatchEvent(evt);
  }
  render() {
    let html;
    var list = null;
    if (this.state != null) {
      var tree = <div style={{'marginLeft':'auto', 'marginRight':'auto', 'width':'230px', 'padding':'90px'}}> No Debug Information Available </div>;
      if (!this.state.noDebug)
      {
        var tree = this.state.list && this.state.list.map((node, i) => {
                  var vm = node.name;
                  var vm_label = <span>{vm}</span>;
                  var backtrace = node.backtrace;
                  return (
                    <TreeView key={vm + '|' + i} nodeLabel={vm_label} defaultCollapsed={false}>
                      {backtrace.map(details => {

                        //Needed to decode HTML
                        var text_temp = document.createElement("textarea")
                        text_temp.innerHTML = details.detail;
                        var text_temp = text_temp.value;
                        var arr = text_temp.split(/\n/);
                        var text = [];
                        for (let i = 0; i < arr.length; i++) {
                          text.push(arr[i]);
                          text.push(<br/>)
                        }

                        return (
                          <TreeView nodeLabel={<span>{details.id}</span>} key={vm + '||' + details.id} defaultCollapsed={false}>
                            <p>{text}</p>
                          </TreeView>
                        );
                      })}
                    </TreeView>
                  );
                })}
      html = (
        <div className="crash-details-wrapper">
              <div className="form-actions">
                <button role="button" className="dark" onClick={this.state.noDebug ? false : this.downloadFile.bind(this, 'crash.txt', 'data:text;charset=UTF-8,' + decodeURIComponent(JSON.stringify(this.state.list, null, 2)))}> Download Crash Details</button>
              </div>
              <div className="crash-container">
                <h2> Debug Information </h2>
                <div className="tree">{tree}</div>
              </div>
        </div>
              );
    } else {
      html = <div className="crash-container"></div>
    };
    let refPage = window.sessionStorage.getItem('refPage');
    refPage = JSON.parse(refPage);
    return (
            <div className="crash-app">
              {html}
            </div>
            );
  }
}
export default CrashDetails;
