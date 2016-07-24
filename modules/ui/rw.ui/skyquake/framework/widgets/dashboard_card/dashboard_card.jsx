
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import './dashboard_card.scss';

var cardClass = 'dashboardCard'//classSet(this.props.class);

var CardHeader = React.createClass({
  render() {
    var cardClassHeader = cardClass + '_header';
    if(this.props.className) {
        cardClassHeader += ' ' + this.props.className + '_header';
    }
   return (
    <header className={cardClassHeader}>
      <h3>
        {this.props.title}
      </h3>
    </header>
    )
  }
});
 CardHeader.defaultProps = {
  title: ' Loading...'
 }



var dashboardCard = React.createClass({
    componentDidMount: function() {

    },
    getDefaultProps: function() {
      return {
        isHidden: false
      }
    },
    render() {
      var cardClassWrapper = cardClass;
      var cardClassContent = cardClass + '_content';
      var cardClassContentBody = cardClassContent + '-body';
      var hasHeader;
      var cardClasses = [];
      if(this.props.className) {
        cardClasses = this.props.className.split(' ');
        cardClasses.map(function(c, i) {
          cardClassWrapper += ' ' + c;
          cardClassContent += ' ' + c + '_content';
          cardClassContentBody += ' ' + c + '-body';
        })

      }
      let closeCard = null;
    if (this.props.showHeader) {
      hasHeader = <CardHeader className={this.props.className} title={this.props.title}/>;
    };
    if (this.props.closeCard) {
      closeCard = this.props.closeCard;
    }
    return (
        <div className={cardClassWrapper} style={{display: this.props.isHidden ? 'none':'inherit'}}>
          {closeCard}
          <i className="corner-accent top left"></i>
          <i className="corner-accent top right"></i>
            {hasHeader}
            <div className={cardClassContent}>
              <div className={cardClassContentBody}>
                {this.props.children}
              </div>
            </div>
          <i className="corner-accent bottom left"></i>
          <i className="corner-accent bottom right"></i>
        </div>
      )
  }
})


// class DashboardCard extends React.Component {
//   constructor(props) {
//     super(props)
//   }
//   render() {

//   }
// }


export default dashboardCard;
