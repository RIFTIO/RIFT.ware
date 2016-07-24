
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Loader from '../loading-indicator/loadingIndicator.jsx';
import React from 'react';

export default class ScreenLoader extends React.Component {
  constructor(props) {
    super(props)
  }
  render() {
    let overlayStyle = {
      position: 'fixed',
      zIndex: 999,
      width: '100%',
      height: '100%',
      top: 0,
      // right: 0,
      // bottom: 0,
      left: 0,
      display: this.props.show ? 'flex' : 'none',
      justifyContent: 'center',
      alignItems: 'center',
      scroll: 'none',
      backgroundColor: 'rgba(0,0,0,0.5)'
    };
    return (
      <div style={overlayStyle}><Loader size="10"/></div>
    );
  }
}
