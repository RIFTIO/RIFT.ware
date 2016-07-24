jest.dontMock('../framework/widgets/skyquake_container/skyquakeNav');

import React from 'react';
import ReactDOM from 'react-dom';
import TestUtils from 'react-addons-test-utils';
import SkyquakeNav from '../framework/widgets/skyquake_container/skyquakeNav';
// const SkyquakeNav = require('../framework/widgets/skyquake_container/skyquakeNav');

describe('SkyquakeNav', () => {

    let exampleRoutes = [{
        "label": "Hello World Component 1",
        "route": "/helloworld/#hello",
        "component": "./helloWorldOne.jsx",
        "path": "",
        "type": "internal",
        "isExternal": true
    }];
    let node;
    beforeEach(function() {
        node = document.createElement('div')
    });
    describe('returnLinkItem', () => {
        let element;
        beforeEach(function() {
            element = SkyquakeNav.returnLinkItem(exampleRoutes[0]);
        });
        it('Returns an <a> tag when external', () => {
            ReactDOM.render(element, node, function() {
                expect(node.tagName == "A";
            })
        })
    })
})
