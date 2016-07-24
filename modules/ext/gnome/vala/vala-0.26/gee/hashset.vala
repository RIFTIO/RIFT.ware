/* hashset.vala
 *
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1997-2000  GLib Team and others
 * Copyright (C) 2007-2009  Jürg Billeter
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * Author:
 * 	Jürg Billeter <j@bitron.ch>
 */

using GLib;

/**
 * Hashtable implementation of the Set interface.
 */
public class Vala.HashSet<G> : Set<G> {
	public override int size {
		get { return (int)_table.size (); }
	}

	public HashFunc hash_func {
		set {
			_hash_func = value;
			reset ();
		}
	}

	public EqualFunc equal_func {
		set {
			_equal_func = value;
			reset ();
		}
	}

	// concurrent modification protection
	private int _stamp = 0;

	private HashTable<G,G> _table;
	private HashFunc _hash_func;
	private EqualFunc _equal_func;

	public HashSet (HashFunc hash_func = GLib.direct_hash, EqualFunc equal_func = GLib.direct_equal) {
		_hash_func = hash_func;
		_equal_func = equal_func;
		reset ();
	}

	private void reset () {
		_stamp++;

		var orig_table = _table;
		_table = new HashTable<G,G> (_hash_func, _equal_func);

		if (orig_table != null && orig_table.size () != 0) {
			G key;
			var iter = HashTableIter<G,G> (_table);

			while (iter.next (out key, null)) {
				_table.insert (key, key);
			}
		}
	}

	public override bool contains (G key) {
		return _table.contains (key);
	}

	public override Type get_element_type () {
		return typeof (G);
	}

	public override Vala.Iterator<G> iterator () {
		return new Iterator<G> (this);
	}

	public override bool add (G key) {
		if (_table.contains (key)) {
			return false;
		}
		_stamp++;
		_table.insert (key, key);
		return true;
	}

	public override bool remove (G key) {
		if (_table.remove (key)) {
			_stamp++;
			return true;
		}
		return false;
	}

	public override void clear () {
		_stamp++;
		_table.remove_all ();
	}

	~HashSet () {
		clear ();
	}

	private class Iterator<G> : Vala.Iterator<G> {
		public HashSet<G> set {
			set {
				_set = value;
				_stamp = _set._stamp;
				_iter = HashTableIter<G,G> (_set._table);
				_started = false;
			}
		}

		private HashSet<G> _set;
		private HashTableIter<G,G> _iter;
		private weak G _key;
		private bool _started;

		// concurrent modification protection
		private int _stamp;

		public Iterator (HashSet set) {
			this.set = set;
		}

		public override bool next () {
			assert (_stamp == _set._stamp);
			_started = true;
			return _iter.next (out _key, null);
		}

		public override G? get () {
			assert (_stamp == _set._stamp);
			assert (_started);
			return _key;
		}
	}
}

