/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import './header.scss';
import Crouton from 'react-crouton';
import HeaderStore from './headerStore.js';
import ScreenLoader from '../screen-loader/screenLoader.jsx';
export default class AppHeader extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
        this.state.validateErrorEvent = 0;
        this.state.validateErrorMsg = '';
    }
    componentDidMount() {
        HeaderStore.listen(this.storeListener);
    }
    componentWillUnmount(){
        HeaderStore.unlisten(this.storeListener);
    }
    closeError() {
        LaunchpadFleetActions.validateReset()
    }
    storeListener = (state) => {
        this.setState(state);
    }
    render() {
        let html;
        let navItems = this.props.nav.map(function(nav, i) {
            if (nav.href || nav.onClick) {
                return <li key={i}><a {...nav}>{nav.name}</a></li>
            } else {
                return <li key={i}><span className="current"> {nav.name} </span></li>
            }
        });
        let errorMessage = <Crouton
                id={Date.now()}
                message={this.state.validateErrorMsg}
                type={"error"}
                hidden={!(this.state.validateErrorEvent && this.state.validateErrorMsg)}
                onDismiss={HeaderStore.validateReset}
            />

        // html = (
        //     <header className="header-app-component">
        //     {errorMessage}
        //     <ScreenLoader show={this.props.isLoading}/>
        //     <div className="header-app-main">
        //         <h1>{this.props.title}</h1>
        //         <CommonLinks></CommonLinks>
        //     </div>
        //     <div className="header-app-nav">
        //         <ul>
        //             {navItems}
        //         </ul>
        //     </div>
        //     </header>
        // );

        html = (
            <header className="header-app-component">
            {errorMessage}
            <ScreenLoader show={this.props.isLoading}/>
            <div className="header-app-nav">
                <ul>
                    {navItems}
                </ul>
            </div>
            </header>
        );

        return html;
    }
}
export class CommonLinks extends React.Component {
    constructor(props) {
        super(props);
    }
    openAbout() {
        generateRefPage();
        navigateTo('about')
    }
    openDebug() {
        generateRefPage();
        navigateTo('debug');
    }
    openLogs(type) {
        HeaderStore.getSysLogViewerURL();
    }
    render() {
        let links = [];
        setContext();
        links.push(<li key={'about'} onClick={this.openAbout}><a>About</a></li>);
        links.push(<li  key={'logs'} onClick={this.openLogs}><a>Logs</a></li>);
        links.push(<li  key={'debug'} onClick={this.openDebug}><a>Debug</a></li>);
        let html;
        html = (
            <nav>
                <ul>
                    {links}
                </ul>
            </nav>
        );
        return html;
    }
}
AppHeader.defaultProps = {
    nav: [],
    isLoading: false
}
function generateRefPage() {
    let applicationContext = window.sessionStorage.getItem('applicationContext') || 'launchpad';
    let hash = window.location.hash.split('/');
    let pageTitle = 'Dashboard';
    if (applicationContext === 'launchpad') {
        hash = '#/launchpad/' + hash[2];
    } else {
        hash = "#/"
    }
    let refPage = {
        'hash': hash,
        'title': pageTitle
    };
    window.sessionStorage.setItem('refPage', JSON.stringify(refPage));
    return refPage;
}
function navigateTo(loc) {
    window.location.hash = '#/' + loc;
}
function setContext() {
     let applicationContext;
     let hashOne = window.location.hash.split('/')[1];
    if(hashOne == "launchpad") {
      applicationContext = "launchpad";
    } else {
      applicationContext = "missioncontrol";
    }
    if(hashOne != 'about' && hashOne != 'debug'){
        window.sessionStorage.setItem('applicationContext', applicationContext);}
}
