/* hashmap.vala
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
 * Hashtable implementation of the Map interface.
 */
public class Vala.HashMap<K,V> : Map<K,V> {
	public override int size {
		get { return (int)_table.size (); }
	}

	public HashFunc key_hash_func {
		set {
			_key_hash_func = value;
			reset ();
		}
	}

	public EqualFunc key_equal_func {
		set {
			_key_equal_func = value;
			reset ();
		}
	}

	public EqualFunc value_equal_func {
		set { _value_equal_func = value; }
	}

	// concurrent modification protection
	private int _stamp = 0;

	private HashTable<K,V> _table;
	private HashFunc _key_hash_func;
	private EqualFunc _key_equal_func;
	private EqualFunc _value_equal_func;

	public HashMap (HashFunc key_hash_func = GLib.direct_hash, EqualFunc key_equal_func = GLib.direct_equal, EqualFunc value_equal_func = GLib.direct_equal) {
		_key_hash_func = key_hash_func;
		_key_equal_func = key_equal_func;
		_value_equal_func = value_equal_func;
		reset ();
	}

	private void reset () {
		_stamp++;

		var orig_table = _table;
		_table = new HashTable<K,V> (_key_hash_func, _key_equal_func);

		if (orig_table != null && orig_table.size () != 0) {
			K key;
			V value;
			var iter = HashTableIter<K,V> (_table);

			while (iter.next (out key, out value)) {
				_table.insert (key, value);
			}
		}
	}

	public override Set<K> get_keys () {
		return new KeySet<K,V> (this);
	}

	public override Collection<V> get_values () {
		return new ValueCollection<K,V> (this);
	}

	public override Vala.MapIterator<K,V> map_iterator () {
		return new MapIterator<K,V> (this);
	}

	public override bool contains (K key) {
		return _table.contains (key);
	}

	public override V? get (K key) {
		return _table.get (key);
	}

	public override void set (K key, V value) {
		_stamp++;
		_table.insert (key, value);
	}

	public override bool remove (K key) {
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

	~HashMap () {
		clear ();
	}

	private class KeySet<K,V> : Set<K> {
		public HashMap<K,V> map {
			set { _map = value; }
		}

		private HashMap<K,V> _map;

		public KeySet (HashMap map) {
			this.map = map;
		}

		public override Type get_element_type () {
			return typeof (K);
		}

		public override Iterator<K> iterator () {
			return new KeyIterator<K,V> (_map);
		}

		public override int size {
			get { return _map.size; }
		}

		public override bool add (K key) {
			assert_not_reached ();
		}

		public override void clear () {
			assert_not_reached ();
		}

		public override bool remove (K key) {
			assert_not_reached ();
		}

		public override bool contains (K key) {
			return _map.contains (key);
		}
	}

	private class MapIterator<K,V> : Vala.MapIterator<K, V> {
		public HashMap<K,V> map {
			set {
				_map = value;
				_stamp = _map._stamp;
				_iter = HashTableIter<K,V> (_map._table);
				_started = false;
			}
		}

		private HashMap<K,V> _map;
		private HashTableIter<K,V> _iter;
		private weak K _key;
		private weak V _value;
		private bool _started;

		// concurrent modification protection
		private int _stamp;

		public MapIterator (HashMap map) {
			this.map = map;
		}

		public override bool next () {
			assert (_stamp == _map._stamp);
			_started = true;
			return _iter.next (out _key, out _value);
		}

		public override K? get_key () {
			assert (_stamp == _map._stamp);
			assert (_started);
			return _key;
		}

		public override V? get_value () {
			assert (_stamp == _map._stamp);
			assert (_started);
			return _value;
		}
	}

	private class KeyIterator<K,V> : Iterator<K> {
		public HashMap<K,V> map {
			set {
				_map = value;
				_iter = new MapIterator<K,V> (_map);
			}
		}

		private HashMap<K,V> _map;
		private MapIterator<K,V> _iter;

		public KeyIterator (HashMap map) {
			this.map = map;
		}

		public override bool next () {
			return _iter.next ();
		}

		public override K? get () {
			return _iter.get_key ();
		}
	}

	private class ValueCollection<K,V> : Collection<V> {
		public HashMap<K,V> map {
			set { _map = value; }
		}

		private HashMap<K,V> _map;

		public ValueCollection (HashMap map) {
			this.map = map;
		}

		public override Type get_element_type () {
			return typeof (V);
		}

		public override Iterator<V> iterator () {
			return new ValueIterator<K,V> (_map);
		}

		public override int size {
			get { return _map.size; }
		}

		public override bool add (V value) {
			assert_not_reached ();
		}

		public override void clear () {
			assert_not_reached ();
		}

		public override bool remove (V value) {
			assert_not_reached ();
		}

		public override bool contains (V value) {
			V current_value;
			var iter = HashTableIter<K,V> (_map._table);

			while (iter.next (null, out current_value)) {
				if (_map._value_equal_func (current_value, value)) {
					return true;
				}
			}
			return false;
		}
	}

	private class ValueIterator<K,V> : Iterator<V> {
		public HashMap<K,V> map {
			set {
				_map = value;
				_iter = new MapIterator<K,V> (_map);
			}
		}

		private HashMap<K,V> _map;
		private MapIterator<K,V> _iter;

		public ValueIterator (HashMap map) {
			this.map = map;
		}

		public override bool next () {
			return _iter.next ();
		}

		public override V? get () {
			return _iter.get_value ();
		}
	}
}

