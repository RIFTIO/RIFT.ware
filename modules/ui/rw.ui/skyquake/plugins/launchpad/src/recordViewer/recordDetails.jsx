
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import LoadingIndicator from 'widgets/loading-indicator/loadingIndicator.jsx';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import Prism from 'prismjs';
import 'prismjs/themes/prism.css';
export default class RecordDetails extends React.Component{
  constructor(props) {
    super(props)
  }
  render(){
    let html;
    let text = JSON.stringify(this.props.data, undefined, 2);
    // html = this.props.isLoading ? <LoadingIndicator size={10} show={true} /> : <pre className="json">{JSON.stringify(this.props.data, undefined, 2)}</pre>;
    html = this.props.isLoading ? <LoadingIndicator size={10} show={true} /> : Prism.highlight(text, Prism.languages.javascript, 'javascript');
    return (
            <DashboardCard showHeader={true} title="Record Details" className={this.props.isLoading ? 'loading' : 'recordDetails'}>
              <pre className="language-js">
              <code dangerouslySetInnerHTML={{__html: html}} />
              </pre>
            </DashboardCard>
            );
  }
}
RecordDetails.defaultProps = {
  data: {}
}
