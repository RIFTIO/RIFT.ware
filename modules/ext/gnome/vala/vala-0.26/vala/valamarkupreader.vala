/* valamarkupreader.vala
 *
 * Copyright (C) 2008-2009  Jürg Billeter
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
 * Simple reader for a subset of XML.
 */
public class Vala.MarkupReader : Object {
	private string _name = null;

	public string filename { get; private set; }

	// avoid a notify per element start/end
	public string name {
		get { return _name; }
	}

	public string content { get; private set; }

	MappedFile mapped_file;

	char* begin;
	char* current;
	char* end;

	int line;
	int column;

	Map<string,string> attributes = new HashMap<string,string> (str_hash, str_equal);
	bool empty_element;

	public MarkupReader (string filename) {
		this.filename = filename;

		try {
			mapped_file = new MappedFile (filename, false);
			begin = mapped_file.get_contents ();
			end = begin + mapped_file.get_length ();

			current = begin;

			line = 1;
			column = 1;
		} catch (FileError e) {
			Report.error (null, "Unable to map file `%s': %s".printf (filename, e.message));
		}
	}

	public string? get_attribute (string attr) {
		return attributes[attr];
	}

	/*
	 * Returns a copy of the current attributes.
	 *
	 * @return map of current attributes
	 */
	public Map<string,string> get_attributes () {
		var result = new HashMap<string,string> (str_hash, str_equal);
		foreach (var key in attributes.get_keys ()) {
			result.set (key, attributes.get (key));
		}
		return result;
	}

	public string read_name () {
		return read_name_real (ref current, ref line, ref column);
	}

	private string read_name_real (ref char* current_, ref int line_, ref int column_) {
		var end = this.end;
		var current = current_, line = line_, column = column_;
		char* start = current;
		while (current < end) {
			if (Posix.strchr (" =>/\n\t", current[0]) != null) {
				break;
			}
			unichar u = ((string) current).get_char_validated ((long) (end - current));
			if (u != (unichar) (-1)) {
				current += u.to_utf8 (null);
			} else {
				Report.error (null, "invalid UTF-8 character");
			}
		}
		if (current == start) {
			// syntax error: invalid name
		}
		current_ = current;
		line_ = line;
		column_ = column;
		return ((string) start).substring (0, (int) (current - start));
	}

	public MarkupTokenType read_token (out SourceLocation token_begin, out SourceLocation token_end) {
		return read_token_real (out token_begin, out token_end,
		                        ref current, ref line, ref column);
	}

	private MarkupTokenType read_token_real (out SourceLocation token_begin, out SourceLocation token_end,
	                                         ref char* current_, ref int line_, ref int column_) {
		var begin = this.begin, end = this.end;
		var current = current_, line = line_, column = column_;

		attributes.clear ();

		if (empty_element) {
			empty_element = false;
			token_begin = SourceLocation (begin, line, column);
			token_end = SourceLocation (begin, line, column);
			return MarkupTokenType.END_ELEMENT;
		}

		space_real (ref current, ref line, ref column);

		MarkupTokenType type = MarkupTokenType.NONE;
		token_begin = SourceLocation (current, line, column);

		if (current >= end) {
			type = MarkupTokenType.EOF;
		} else if (current[0] == '<') {
			current++;
			if (current >= end) {
				// error
			} else {
				switch (current[0]) {
				case '?':
					// processing instruction
					break;
				case '!':
					// comment or doctype
					current++;
					if (current < end - 1 && ((string) current).has_prefix ("--")) {
						// comment
						current += 2;
						while (current < end - 2) {
							if (((string) current).has_prefix ("-->")) {
								// end of comment
								current += 3;
								break;
							} else if (current[0] == '\n') {
								line++;
								column = 0;
							}
							current++;
						}

						// ignore comment, read next token
						current_ = current;
						line_ = line;
						column_ = column;
						return read_token_real (out token_begin, out token_end,
						                        ref current_, ref line_, ref column_);
					}
					break;
				case '/':
					type = MarkupTokenType.END_ELEMENT;
					current++;
					_name = read_name_real (ref current, ref line, ref column);
					if (current >= end || current[0] != '>') {
						// error
					}
					current++;
					break;
				default:
					type = MarkupTokenType.START_ELEMENT;
					_name = read_name_real (ref current, ref line, ref column);
					space_real (ref current, ref line, ref column);
					while (current < end && Posix.strchr (">/", current[0]) == null) {
						string attr_name = read_name_real (ref current, ref line, ref column);
						if (current >= end || current[0] != '=') {
							// error
						}
						current++;
						if (current >= end || Posix.strchr ("\"'", current[0]) == null) {
							// error
						}
						char quote = current[0];
						current++;

						string attr_value = text_real (quote, false,
						                               ref current, ref line, ref column);

						if (current >= end || current[0] != quote) {
							// error
						}
						current++;
						attributes.set (attr_name, attr_value);
						space_real (ref current, ref line, ref column);
					}
					if (current[0] == '/') {
						empty_element = true;
						current++;
						space_real (ref current, ref line, ref column);
					} else {
						empty_element = false;
					}
					if (current >= end || current[0] != '>') {
						// error
					}
					current++;
					break;
				}
			}
		} else {
			space_real (ref current, ref line, ref column);

			if (current[0] != '<') {
				content = text_real ('<', true, ref current, ref line, ref column);
			} else {
				// no text
				// read next token
				current_ = current;
				line_ = line;
				column_ = column;
				return read_token_real (out token_begin, out token_end,
				                        ref current_, ref line_, ref column_);
			}

			type = MarkupTokenType.TEXT;
		}

		token_end = SourceLocation (current, line, column - 1);

		current_ = current;
		line_ = line;
		column_ = column;
		return type;
	}

	public string text (char end_char, bool rm_trailing_whitespace) {
		return text_real (end_char, rm_trailing_whitespace,
		                  ref current, ref line, ref column);
	}

	private string text_real (char end_char, bool rm_trailing_whitespace,
	                          ref char* current_, ref int line_, ref int column_) {
		var end = this.end;
		var current = current_, line = line_, column = column_;
		StringBuilder content = new StringBuilder ();
		char* text_begin = current;
		char* last_linebreak = current;

		while (current < end && current[0] != end_char) {
			unichar u = ((string) current).get_char_validated ((long) (end - current));
			switch (u) {
			case (unichar) (-1):
				Report.error (null, "invalid UTF-8 character");
				break;
			case '&':
				char* next_pos = current + u.to_utf8 (null);
				if (((string) next_pos).has_prefix ("amp;")) {
					content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
					content.append_c ('&');
					current += 5;
					text_begin = current;
				} else if (((string) next_pos).has_prefix ("quot;")) {
					content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
					content.append_c ('"');
					current += 6;
					text_begin = current;
				} else if (((string) next_pos).has_prefix ("apos;")) {
					content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
					content.append_c ('\'');
					current += 6;
					text_begin = current;
				} else if (((string) next_pos).has_prefix ("lt;")) {
					content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
					content.append_c ('<');
					current += 4;
					text_begin = current;
				} else if (((string) next_pos).has_prefix ("gt;")) {
					content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
					content.append_c ('>');
					current += 4;
					text_begin = current;
				} else {
					current += u.to_utf8 (null);
				}
				break;
			case '\n': // Split from the default case for performance
				line++;
				column = 0;
				last_linebreak = current;
				current += u.to_utf8 (null);
				column++;
				break;
			default:
				current += u.to_utf8 (null);
				column++;
				break;
			}
		}

		if (text_begin != current) {
			content.append_len ((string) text_begin, (ssize_t) (current - text_begin));
		}

		column += (int) (current - last_linebreak);

		// Removes trailing whitespace
		if (rm_trailing_whitespace) {
			char* str_pos = ((char*)content.str) + content.len;
			for (str_pos--; str_pos > ((char*)content.str) && str_pos[0].isspace(); str_pos--);
			content.erase ((ssize_t) (str_pos-((char*) content.str) + 1), -1);
		}

		current_ = current;
		line_ = line;
		column_ = column;
		return content.str;
	}

	public void space () {
		space_real (ref current, ref line, ref column);
	}

	private void space_real (ref char* current_, ref int line_, ref int column_) {
		var end = this.end;
		var current = current_, line = line_, column = column_;

		while (current < end) {
			switch (current[0]) {
			case '\n':
				line++;
				column = 1;
				break;
			case ' ':
			case '\t':
			case '\r':
				column++;
				break;
			default:
				current_ = current;
				line_ = line;
				column_ = column;
				return;
			}
			current++;
		}
	}
}

public enum Vala.MarkupTokenType {
	NONE,
	START_ELEMENT,
	END_ELEMENT,
	TEXT,
	EOF;

	public unowned string to_string () {
		switch (this) {
		case START_ELEMENT: return "start element";
		case END_ELEMENT: return "end element";
		case TEXT: return "text";
		case EOF: return "end of file";
		default: return "unknown token type";
		}
	}
}

