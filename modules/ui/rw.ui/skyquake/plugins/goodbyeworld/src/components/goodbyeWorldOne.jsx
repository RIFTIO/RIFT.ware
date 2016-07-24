import React from 'react';
export default class GoodbyeWorldApp extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
            <GoodbyeWorldView />
        );
        return html;
    }
}

class GoodbyeWorldView extends React.Component {
    constructor(props) {
        super(props);
    }
    render() {
        let html;
        html = (
           <div>GoodbyeWorld: Goodbye World One</div>
        );
        return html;
    }
}

