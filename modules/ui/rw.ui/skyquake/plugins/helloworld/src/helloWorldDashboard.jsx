import React from 'react';

export default class HelloWorldDashboard extends React.Component {
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
           <div>HelloWorld: HelloWorldDashboard</div>
        );
        return html;
    }
}
