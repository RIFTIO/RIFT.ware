import React from 'react';

export default class HelloWorldApp extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
            <HelloWorldView />
        );
        return html;
    }
}

class HelloWorldView extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
           <div>HelloWorld: Hello World One</div>
        );
        return html;
    }
}
