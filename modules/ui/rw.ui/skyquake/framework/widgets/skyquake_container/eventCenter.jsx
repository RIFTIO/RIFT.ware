/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

 /**
  * EventCenter module to display a list of events from the system
  * @module framework/widgets/skyquake_container/EventCenter
  * @author Kiran Kashalkar <kiran.kashalkar@riftio.com>
  *
  */

import React from 'react';
import { Link } from 'react-router';
import Utils from 'utils/utils.js';
import Crouton from 'react-crouton';
import TreeView from 'react-treeview';
import _ from 'lodash';
import '../../../node_modules/react-treeview/react-treeview.css';
import './eventCenter.scss';

export default class EventCenter extends React.Component {
	constructor(props) {
		super(props);
		this.state = {};
		this.state.isOpen = false;
	}

	componentWillReceiveProps(props) {
		let stateObject = {};

		let notificationList = sessionStorage.getItem('notificationList');
		let latestNotification = sessionStorage.getItem('latestNotification');

		if (props.newNotificationEvent && props.newNotificationMsg) {
			if (latestNotification) {
				latestNotification = JSON.parse(latestNotification);
				if (!_.isEqual(props.newNotificationMsg, latestNotification)) {
					stateObject.newNotificationEvent = props.newNotificationEvent;
					stateObject.newNotificationMsg = props.newNotificationMsg;
					sessionStorage.setItem('latestNotification', JSON.stringify(stateObject.newNotificationMsg));
				} else {
					stateObject.newNotificationEvent = false;
					stateObject.newNotificationMsg = null;
				}
			} else {
				stateObject.newNotificationEvent = true;
				stateObject.newNotificationMsg = props.newNotificationMsg;
				sessionStorage.setItem('latestNotification', JSON.stringify(stateObject.newNotificationMsg));
			}
		} else {
			stateObject.newNotificationEvent = false;
			stateObject.newNotificationMsg = null;
		}

		if (notificationList) {
			stateObject.notifications = _.merge(notificationList, props.notifications);
		} else {
			stateObject.notifications = props.notifications;
		}
		sessionStorage.setItem('notifications', JSON.stringify(stateObject.notifications));
		this.setState(stateObject);
	}

	newNotificationReset = () => {
        this.setState({
            newNotificationEvent: false
        });
    }

	onClickToggleOpenClose(event) {
		this.props.onToggle();
		this.setState({
			isOpen: !this.state.isOpen
		});
	}
	constructTree(details) {
		let markup = [];
		for (let key in details) {
			if (typeof(details[key]) == "object") {
			    let html = (
			      	<TreeView key={key} nodeLabel={key}>
			          {this.constructTree(details[key])}
			      	</TreeView>
			    );
			    markup.push(html);
			} else {
				markup.push((<div key={key} className="info">{key} = {details[key].toString()}</div>));
			}
		}
		return markup;
	}

	getNotificationFields(notification) {
		let notificationFields = {};

		notificationFields.source = notification.source;
		notificationFields.eventTime = notification.eventTime;

		Object.keys(notification).map((notificationKey) => {
			if (_.indexOf(['source', 'eventTime'], notificationKey) == -1) {
				notificationFields.eventKey = notificationKey;
				notificationFields.details = notification[notificationFields.eventKey];
			}
		});

		return notificationFields;
	}

	render() {
		let html;
		let displayNotifications = [];
		let displayNotificationsTableHead = null;

		if (this.state.notifications && this.state.notifications.length > 0) {
			displayNotificationsTableHead = (
				<thead>
					<tr key='header' className='header'>
						<th className='source column'> Source Event Stream </th>
						<th className='timestamp column'> Timestamp </th>
						<th className='event column'> Event </th>
						<th className='details column'> Details </th>
					</tr>
				</thead>
			);
		}

		this.state.notifications && this.state.notifications.map((notification, notifIndex) => {
			let notificationFields = {};

			notificationFields = this.getNotificationFields(notification);

			displayNotifications.push(
				<tr key={notifIndex} className='notificationItem'>
					<td className='source column'> {notificationFields.source} </td>
					<td className='timestamp column'> {notificationFields.eventTime} </td>
					<td className='event column'> {notificationFields.eventKey} </td>
					<td className='details column'>
						<TreeView key={notifIndex + '-detail'} nodeLabel='Event Details'>
							{this.constructTree(notificationFields.details)}
						</TreeView>
					</td>
				</tr>
			);
		});

		let openedClassName = this.state.isOpen ? 'open' : 'closed';
		html = (
			<div className={'eventCenter ' + openedClassName}>
				{
                    (this.state.newNotificationEvent && this.state.newNotificationMsg)?
                        <div className='notification'>
                            <Crouton
                                id={Date.now()}
                                message={this.getNotificationFields(this.state.newNotificationMsg).eventKey +
                                	' notification received. Check Event Center for more details.'}
                                type={'info'}
                                hidden={!(this.state.newNotificationEvent && this.state.newNotificationMsg)}
                                onDismiss={this.newNotificationReset}
                                timeout={this.props.dismissTimeout}
                            />
                        </div>:
                        null
                }
				<h1 className='title' data-open-close-icon={this.state.isOpen ? 'open' : 'closed'}
					onClick={this.onClickToggleOpenClose.bind(this)}>
					EVENT CENTER
				</h1>
				<div className='notificationListBody'>
					<table className='notificationList'>
						{displayNotificationsTableHead}
						<tbody>
							{displayNotifications}
						</tbody>
					</table>
				</div>
			</div>
		);

		return html;
	}
}

EventCenter.defaultProps = {
	dismissTimeout: 3000
}