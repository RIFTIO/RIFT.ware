jest.dontMock('../framework/widgets/skyquake_container/skyquakeNav.jsx');
// /skyquake/framework/widgets/skyquake_container/skyquakeNav.jsx
import React from 'react';
import ReactDOM from 'react-dom';
import TestUtils from 'react-addons-test-utils';

// import {returnLinkItem} from '../framework/widgets/skyquake_container/skyquakeNav.jsx';

const SkyquakeNav = require('../framework/widgets/skyquake_container/skyquakeNav.jsx')

describe('SkyquakeNav', () => {

    let exampleRoutes;
    let node;
    beforeEach(function() {
            node = document.createElement('div');
            exampleRoutes = [{
                "label": "Hello World Component 1",
                "route": "/helloworld/#hello",
                "component": "./helloWorldOne.jsx",
                "path": "",
                "type": "internal",
                "isExternal": true
            },{

                "label": "Hello World Component 1",
                "route": "hello",
                "component": "./helloWorldOne.jsx",
                "path": "",
                "type": "internal",
                "isExternal": false
            }];
    });
    describe('returnLinkItem', () => {
        it('Returns an <a> tag when external', () => {
            let element = SkyquakeNav.returnLinkItem(exampleRoutes[0]);
            let Tag = TestUtils.renderIntoDocument(element);
            let TagNode = ReactDOM.findDOMNode(Tag);
            expect(Tag.constructor.displayName).toEqual("A");
            expect(TagNode.attributes.href).not.toEqual(undefined);
        });
        it('Returns a <Link> tag when internal', () => {
            let element = SkyquakeNav.returnLinkItem(exampleRoutes[1]);
            let Tag = TestUtils.renderIntoDocument(element);
            let TagNode = ReactDOM.findDOMNode(Tag);
            expect(TagNode.constructor.displayName).toEqual("Link");
            expect(TagNode.attributes.href).not.toEqual(undefined);
        })
    })
})
