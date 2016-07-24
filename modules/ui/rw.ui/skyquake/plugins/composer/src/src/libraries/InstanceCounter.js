/**
 * Created by onvelocity on 1/28/16.
 */

function getInstanceCount(type, bump = true) {
	// keep a global counter
	let count = 0;
	const data = sessionStorage.getItem('instance-counts');
	const counts = JSON.parse(data || '{}');
	if (counts.hasOwnProperty(type)) {
		count = counts[type];
		if (bump) {
			counts[type] = ++count;
		} else {
			return count;
		}
	} else {
		count = counts[type] = 1;
	}
	sessionStorage.setItem('instance-counts', JSON.stringify(counts));
	return count;
}

export default {

	/**
	 * Get instance count for given type without bumping the number.
	 *
	 * @param type
	 * @returns {number}
	 */
	current(type) {
		return getInstanceCount(type, false);
	},

	/**
	 * Get the instance count for the given type - will bump the counter and return the new value.
	 *
	 * @param type
	 * @returns {number}
	 */
	count(type) {
		return getInstanceCount(type);
	}

}