/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import { Link } from 'react-router';
import Utils from 'utils/utils.js';
import Crouton from 'react-crouton';
import 'style/common.scss';

//Temporary, until api server is on same port as webserver
var rw = require('utils/rw.js');
var API_SERVER = rw.getSearchParams(window.location).api_server;
var UPLOAD_SERVER = rw.getSearchParams(window.location).upload_server;
//Remove once logging plugin is implemented

//
// Internal classes/functions
//

class LogoutAppMenuItem extends React.Component {
    handleLogout() {
        Utils.clearAuthentication();
    }
    render() {
        return (
            <div className="app">
                <h2>
                    <a onClick={this.handleLogout}>
                        Logout
                    </a>
                </h2>
            </div>
        );
    }
}


//
// Exported classes and functions
//

//
/**
 * Skyquake Nav Component. Provides navigation functionality between all plugins
 */
export default class skyquakeNav extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
        this.state.validateErrorEvent = 0;
        this.state.validateErrorMsg = '';
    }
    validateError = (msg) => {
        this.setState({
            validateErrorEvent: true,
            validateErrorMsg: msg
        });
    }
    validateReset = () => {
        this.setState({
            validateErrorEvent: false
        });
    }
    returnCrouton = () => {
        return <Crouton
            id={Date.now()}
            message={this.state.validateErrorMsg}
            type={"error"}
            hidden={!(this.state.validateErrorEvent && this.state.validateErrorMsg)}
            onDismiss={this.validateReset}
        />;
    }
    render() {
        let html;
        html = (
                <div>
                {this.returnCrouton()}
            <nav className="skyquakeNav">
                {buildNav.call(this, this.props.nav, this.props.currentPlugin, this.props.store.getSysLogViewerURL)}
            </nav>

            </div>
        )
        return html;
    }
}
skyquakeNav.defaultProps = {
    nav: {}
}
/**
 * Returns a React Component
 * @param  {object} link  Information about the nav link
 * @param  {string} link.route Hash route that the SPA should resolve
 * @param  {string} link.name Link name to be displayed
 * @param  {number} index index of current array item
 * @return {object} component A React LI Component
 */
//This should be extended to also make use of internal/external links and determine if the link should refer to an outside plugin or itself.
export function buildNavListItem (k, link, index) {
    let html = false;
    if (link.type == 'external') {
        this.hasSubNav[k] = true;
        html = (
            <li key={index}>
                {returnLinkItem(link)}
            </li>
        );
    }
    return html;
}

/**
 * Builds a link to a React Router route or a new plugin route.
 * @param  {object} link Routing information from nav object.
 * @return {object}  component   returns a react component that links to a new route.
 */
export function returnLinkItem(link) {
    let ref;
    let route = link.route;
    if(link.isExternal) {
        ref = (
            <a href={route}>{link.label}</a>
        )
    } else {
        if(link.path && link.path.replace(' ', '') != '') {
            route = link.path;
        }
        if(link.query) {
            let query = {};
            query[link.query] = '';
            route = {
                pathname: route,
                query: query
            }
        }
        ref = (
           <Link to={route}>
                {link.label}
            </Link>
        )
    }
    return ref;
}

/**
 * Constructs nav for each plugin, along with available subnavs
 * @param  {array} nav List returned from /nav endpoint.
 * @return {array}     List of constructed nav element for each plugin
 */
export function buildNav(nav, currentPlugin, tmpLogFn) {
    let navList = [];
    let navListHTML = [];
    let secondaryNav = [];
    let self = this;
    self.hasSubNav = {};
    function clickHandler() {
        tmpLogFn().catch(function(){
            self.validateError("Log URL has not been configured.");
        })
    }
    let secondaryNavHTML = (
        <div className="secondaryNav" key="secondaryNav">
        {secondaryNav}
            <div className="app">
                <h2>
                    <a onClick={clickHandler}>
                        Log
                    </a>
                </h2>
            </div>
            <LogoutAppMenuItem />
        </div>
    )
    for (let k in nav) {
        if (nav.hasOwnProperty(k)) {
            self.hasSubNav[k] = false;
            let header = null;
            let navClass = "app";
            let routes = nav[k].routes;
            let navItem = {};
            //Primary plugin title and link to dashboard.
            let route;
            let NavList;
            if (API_SERVER) {
                route = routes[0].isExternal ? '/' + k + '/index.html?api_server=' + API_SERVER + '' + '&upload_server=' + UPLOAD_SERVER + '' : '';
            } else {
                route = routes[0].isExternal ? '/' + k + '/' : '';
            }
            let dashboardLink = returnLinkItem({
                isExternal: routes[0].isExternal,
                pluginName: nav[k].pluginName,
                label: nav[k].label || k,
                route: route
            });
            if (nav[k].pluginName == currentPlugin) {
                navClass += " active";
            }
            NavList = nav[k].routes.map(buildNavListItem.bind(self, k));
            navItem.priority = nav[k].priority;
            navItem.order = nav[k].order;
            navItem.html = (
                <div key={k} className={navClass}>
                    <h2>{dashboardLink} {self.hasSubNav[k] ? <span className="oi" data-glyph="caret-bottom"></span> : ''}</h2>
                    <ul className="menu">
                        {
                            NavList
                        }
                    </ul>
                </div>
            );
            navList.push(navItem)
        }
    }
    //Sorts nav items by order and returns only the markup
    navListHTML = navList.sort((a,b) => a.order - b.order).map(function(n) {
        if((n.priority  < 2)){
            return n.html;
        } else {
            secondaryNav.push(n.html);
        }
    });
    navListHTML.push(secondaryNavHTML);
    return navListHTML;
}
