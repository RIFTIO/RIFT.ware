namespace Gdk {
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.AppLaunchContext", since = "vala-0.24")]
	public class X11AppLaunchContext : Gdk.AppLaunchContext {
		[CCode (has_construct_function = false)]
		protected X11AppLaunchContext ();
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Cursor", since = "vala-0.24")]
	public class X11Cursor : Gdk.Cursor {
		[CCode (has_construct_function = false)]
		protected X11Cursor ();
		public static X.Cursor get_xcursor (Gdk.Cursor cursor);
		public static unowned X.Display get_xdisplay (Gdk.Cursor cursor);
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Display", since = "vala-0.24")]
	public class X11Display : Gdk.Display {
		[CCode (has_construct_function = false)]
		protected X11Display ();
		public static void broadcast_startup_message (Gdk.Display display, string message_type, ...);
		public static int error_trap_pop (Gdk.Display display);
		public static void error_trap_pop_ignored (Gdk.Display display);
		public static void error_trap_push (Gdk.Display display);
		public static unowned string get_startup_notification_id (Gdk.Display display);
		public static uint32 get_user_time (Gdk.Display display);
		public static unowned X.Display get_xdisplay (Gdk.Display display);
		public static void grab (Gdk.Display display);
		public static void set_cursor_theme (Gdk.Display display, string theme, int size);
		public static void set_startup_notification_id (Gdk.Display display, string startup_id);
		public static void set_window_scale (Gdk.Display display, int scale);
		public static int string_to_compound_text (Gdk.Display display, string str, out Gdk.Atom encoding, int format, uchar[] ctext, int length);
		public static int text_property_to_text_list (Gdk.Display display, Gdk.Atom encoding, int format, uchar[] text, int length, string list);
		public static void ungrab (Gdk.Display display);
		public static bool utf8_to_compound_text (Gdk.Display display, string str, out Gdk.Atom encoding, int format, uchar[] ctext, int length);
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.DisplayManager", since = "vala-0.24")]
	public class X11DisplayManager : Gdk.DisplayManager {
		[CCode (has_construct_function = false)]
		protected X11DisplayManager ();
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.DragContext", since = "vala-0.24")]
	public class X11DragContext : Gdk.DragContext {
		[CCode (has_construct_function = false)]
		protected X11DragContext ();
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Keymap", since = "vala-0.24")]
	public class X11Keymap : Gdk.Keymap {
		[CCode (has_construct_function = false)]
		protected X11Keymap ();
		public static int get_group_for_state (Gdk.Keymap keymap, uint state);
		public static bool key_is_modifier (Gdk.Keymap keymap, uint keycode);
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Screen", since = "vala-0.24")]
	public class X11Screen : Gdk.Screen {
		[CCode (has_construct_function = false)]
		protected X11Screen ();
		public static uint32 get_current_desktop (Gdk.Screen screen);
		public static X.ID get_monitor_output (Gdk.Screen screen, int monitor_num);
		public static uint32 get_number_of_desktops (Gdk.Screen screen);
		public static int get_screen_number (Gdk.Screen screen);
		public static unowned string get_window_manager_name (Gdk.Screen screen);
		public static unowned X.Screen get_xscreen (Gdk.Screen screen);
		public static unowned Gdk.Visual lookup_visual (Gdk.Screen screen, X.VisualID xvisualid);
		public static bool supports_net_wm_hint (Gdk.Screen screen, Gdk.Atom property);
		public virtual signal void window_manager_changed ();
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Visual", since = "vala-0.24")]
	public class X11Visual : Gdk.Visual {
		[CCode (has_construct_function = false)]
		protected X11Visual ();
		public static unowned X.Visual get_xvisual (Gdk.Visual visual);
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Window", since = "vala-0.24")]
	public class X11Window : Gdk.Window {
		[CCode (has_construct_function = false)]
		protected X11Window ();
		public static unowned Gdk.Window foreign_new_for_display (Gdk.Display display, X.Window window);
		public static uint32 get_desktop (Gdk.Window window);
		public static X.Window get_xid (Gdk.Window window);
		public static unowned Gdk.Window lookup_for_display (Gdk.Display display, X.Window window);
		public static void move_to_current_desktop (Gdk.Window window);
		public static void move_to_desktop (Gdk.Window window, uint32 desktop);
		public static void set_frame_extents (Gdk.Window window, int left, int right, int top, int bottom);
		public static void set_frame_sync_enabled (Gdk.Window window, bool frame_sync_enabled);
		public static void set_hide_titlebar_when_maximized (Gdk.Window window, bool hide_titlebar_when_maximized);
		public static void set_theme_variant (Gdk.Window window, string variant);
		public static void set_user_time (Gdk.Window window, uint32 timestamp);
		public static void set_utf8_property (Gdk.Window window, string name, string value);
	}
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.atom_to_xatom", since = "vala-0.24")]
	public static X.Atom x11_atom_to_xatom (Gdk.Atom atom);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.atom_to_xatom_for_display", since = "vala-0.24")]
	public static X.Atom x11_atom_to_xatom_for_display (Gdk.Display display, Gdk.Atom atom);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.free_compound_text", since = "vala-0.24")]
	public static void x11_free_compound_text (uchar[] ctext);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.free_text_list", since = "vala-0.24")]
	public static void x11_free_text_list (string list);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_default_root_xwindow", since = "vala-0.24")]
	public static X.Window x11_get_default_root_xwindow ();
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_default_scree", since = "vala-0.24")]
	public static int x11_get_default_screen ();
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_default_xdisplay", since = "vala-0.24")]
	public static unowned X.Display x11_get_default_xdisplay ();
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_server_time", since = "vala-0.24")]
	public static uint32 x11_get_server_time (Gdk.Window window);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_xatom_by_name", since = "vala-0.24")]
	public static X.Atom x11_get_xatom_by_name (string atom_name);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_xatom_by_name_for_display", since = "vala-0.24")]
	public static X.Atom x11_get_xatom_by_name_for_display (Gdk.Display display, string atom_name);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_xatom_nam", since = "vala-0.24")]
	public static unowned string x11_get_xatom_name (X.Atom xatom);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.get_xatom_name_for_display", since = "vala-0.24")]
	public static unowned string x11_get_xatom_name_for_display (Gdk.Display display, X.Atom xatom);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.grab_server", since = "vala-0.24")]
	public static void x11_grab_server ();
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.Display.lookup_for_xdisplay", since = "vala-0.24")]
	public static unowned Gdk.Display x11_lookup_xdisplay (X.Display xdisplay);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.register_standard_event_type", since = "vala-0.24")]
	public static void x11_register_standard_event_type (Gdk.Display display, int event_base, int n_events);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.set_sm_client_id", since = "vala-0.24")]
	public static void x11_set_sm_client_id (string sm_client_id);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.ungrab_server", since = "vala-0.24")]
	public static void x11_ungrab_server ();
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.xatom_to_atom", since = "vala-0.24")]
	public static Gdk.Atom x11_xatom_to_atom (X.Atom xatom);
	[CCode (cheader_filename = "gdk/gdkx.h")]
	[Deprecated (replacement = "Gdk.X11.xatom_to_atom_for_display", since = "vala-0.24")]
	public static Gdk.Atom x11_xatom_to_atom_for_display (Gdk.Display display, X.Atom xatom);

}
