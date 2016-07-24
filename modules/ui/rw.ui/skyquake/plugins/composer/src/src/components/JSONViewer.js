/**
 * Created by onvelocity on 2/2/16.
 */

import React from 'react'
import Prism from 'prismjs'
import PureRenderMixin from 'react-addons-pure-render-mixin'

import '../styles/JSONViewer.scss'

const cssString = `
					/*
					 copied from node_modules/prismjs/themes/prismjs.css
					 */
					:not(pre) > code[class*="language-"],
					pre[class*="language-"] {
					font-size: 12px;
					border: 1px solid rgba(220, 220, 220, 0.5);
					border-radius: 4px;
					background-color: rgba(255, 255, 255, .25);
				}

					/* Inline code */
					:not(pre) > code[class*="language-"] {
					padding: .1em;
				}

					.token.comment,
					.token.prolog,
					.token.doctype,
					.token.cdata {
					color: slategray;
				}

					.token.punctuation {
					color: #333;
				}

					.namespace {
					opacity: .7;
				}

					.token.property,
					.token.tag,
					.token.boolean,
					.token.number,
					.token.constant,
					.token.symbol,
					.token.deleted {
					color: #905;
				}

					.token.selector,
					.token.attr-name,
					.token.string,
					.token.char,
					.token.builtin,
					.token.inserted {
					color: #df5000;
				}

					.token.operator,
					.token.entity,
					.token.url,
					.language-css .token.string,
					.style .token.string {
					color: #a67f59;
					background: hsla(0, 0%, 100%, .5);
				}

					.token.atrule,
					.token.attr-value,
					.token.keyword {
					color: #07a;
				}

					.token.function {
					color: #DD4A68;
				}

					.token.regex,
					.token.important,
					.token.variable {
					color: #e90;
				}

					.token.important,
					.token.bold {
					font-weight: bold;
				}
					.token.italic {
					font-style: italic;
				}

					.token.entity {
					cursor: help;
				}
`;

const JSONViewer = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState: function () {
		return {};
	},
	getDefaultProps: function () {
		return {};
	},
	componentWillMount: function () {
	},
	componentDidMount: function () {
	},
	componentDidUpdate: function () {
	},
	componentWillUnmount: function () {
	},
	render() {
		const text = JSON.stringify(this.props.json, undefined, 12).replace(/    /g, ' ');
		const result = Prism.highlight(text, Prism.languages.javascript, 'javascript');
		return (
			<div className="JSONViewer">
				<style dangerouslySetInnerHTML={{__html: cssString}}></style>
				<label className="descriptor">
					<pre className="language-js">
						<code dangerouslySetInnerHTML={{__html: result}} />
					</pre>
				</label>
			</div>
		);
	}
});

export default JSONViewer;
