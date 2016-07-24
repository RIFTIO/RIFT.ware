import React from 'react';
export default function(Component) {
  class SkyquakeComponent extends React.Component {
    constructor(props, context) {
        super(props, context);
        this.router = context.router;
        this.actions = context.flux.actions.global;
    }
    render(props) {
        return <Component {...this.props} router={this.router} actions={this.actions} flux={this.context.flux} />
    }
  }
  SkyquakeComponent.contextTypes = {
    router: React.PropTypes.object,
    flux: React.PropTypes.object
  };
  return SkyquakeComponent;
}
