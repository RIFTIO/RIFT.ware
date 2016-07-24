/**
 * Created by onvelocity on 2/22/16.
 */

const checkFullScreen = ['fullscreen', 'mozFullScreen', 'webkitIsFullScreen', 'msFullscreenElement'];

export default function isFullScreen () {
	return checkFullScreen.reduce((r, name) => {
		if (name in document) {
			return document[name];
		}
		return r;
	}, false);
}
