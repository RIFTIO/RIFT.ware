/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import {
    Router, Route, hashHistory, IndexRoute
}
from 'react-router';
import SkyquakeContainer from 'widgets/skyquake_container/skyquakeContainer.jsx';

export default function(config, context) {
    let routes = [];
    let index = null;
    let components = null;
    if (config && config.routes) {
        routes =  buildRoutes(config.routes)
        function buildRoutes(routes) {
            return routes.map(function(route, index) {
                let routeConfig = {};
                if (route.route && route.component) {
                    // ES6 modules need to specify default
                    let path = route.route;
                    if(route.path && route.path.replace(' ', '') != '') {
                        path = route.path
                    }
                    routeConfig = {
                        path: path,
                        name: route.label,
                        getComponent: function(location, cb) {
                            require.ensure([], (require) => {
                                cb(null, context(route.component).default)
                            })
                        }
                    }
                    if(route.routes && (route.routes.length > 0)){
                        routeConfig.childRoutes = buildRoutes(route.routes)
                    }
                } else {
                    console.error('Route not properly configured. Check that both path and component are specified');
                }
                return routeConfig;
            });
        }
        routes.push({
            path: '/login',
            name: 'Login',
            component:  require('../login/login.jsx').default
        });
        routes.push({
            path:'*',
            name: 'Dashboard',
            getComponent: function(loc, cb) {
                cb(null, context(config.dashboard).default);
            }
        });
        if (config.dashboard) {
            // ES6 modules need to specify default
            index = <IndexRoute component={context(config.dashboard).default} />;
        } else {
            index = DefaultDashboard
        }

        const rootRoute = {
            component: SkyquakeContainer,
            path: '/',
            name: 'Dashboard',
            indexRoute: {
                component: (config.dashboard) ? context(config.dashboard).default : DefaultDashboard
            },
            childRoutes: routes
        }

        return((
            <Router history={hashHistory} routes={rootRoute}>
            </Router>
        ))
    } else {
        console.error('There are no routes configured in the config.json file');
    }
}

//When no default dashboard is specified in the plugin_config.json, use this component.
class DefaultDashboard extends React.Component {
    constructor(props) {
        super(props)
    }
    render() {
        let html;
        html = <div> This is a default dashboard page component </div>;
        return html;
    }
}
