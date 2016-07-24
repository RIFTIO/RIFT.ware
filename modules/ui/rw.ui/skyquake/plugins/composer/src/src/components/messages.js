
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/20/15.
 */
'use strict';

import React from 'react'

import imgOnboard from '../../../node_modules/open-iconic/svg/cloud-upload.svg'

const message = {
	detailsWelcome() {
		return <p className="welcome-message">Select an object to view details.</p>;
	},
	canvasWelcome() {
		return (
			<span>
				<p className="welcome-message">Double-click a Descriptor to open.</p>
				<p className="welcome-message">Or drag a Descriptor to add to Canvas.</p>
			</span>
		);
	},
	get showMoreTitle() {
		return 'Show More';
	},
	get showLessTitle() {
		return 'Show Less';
	},
	get catalogWelcome() {
		return <p className="welcome-message">To onboard a descriptor, drag the package to the catalog or click the Onboard button (<img style={{width: '20px'}} src={imgOnboard} />) to select the package.</p>;
	},
	getSaveActionLabel(isNew) {
		return isNew ? 'Onboard' : 'Update';
	}
};

export default message;
