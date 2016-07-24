import React from 'react';

export default class HelloWorldTwo extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
            <HelloWorldTwoView />
        );
        return html;
    }
}

class HelloWorldTwoView extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
           <div>HelloWorld: Hello World Two</div>
        );
        return html;
    }
}

