/*
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#define MAEMO_CHANGES 1

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "hildon.h"

GtkWidget *a;
HildonDialog *b, *c;

static char current_window (void)
{
	if (a && gtk_window_has_toplevel_focus (GTK_WINDOW (a)))
		return 'a';
	else if (b && gtk_window_has_toplevel_focus (GTK_WINDOW (b)))
		return 'b';
	else if (c && gtk_window_has_toplevel_focus (GTK_WINDOW (c)))
		return 'c';
	else
		return '?';
}

gboolean do_something (gpointer dummy)
{
	static count = 0;
	static char expecting = '?';

	switch (count++)
	{
		case 0:
			a = hildon_window_new ();
			gtk_window_set_title ( GTK_WINDOW (a), "a (should have two transients)");
			gtk_widget_show_all (GTK_WIDGET (a));
			g_warning ("Opened a window 'a'");
			expecting = 'a';
			break;

		case 2:
			b = HILDON_DIALOG (hildon_dialog_new ());
			gtk_window_set_transient_for (GTK_WINDOW (b), GTK_WINDOW (a));
			gtk_window_set_title ( GTK_WINDOW (b), "b (should be transient to a)");
			gtk_widget_show_all (GTK_WIDGET (b));
			g_warning ("Opened another window 'b' and set it transient to 'a'");
			expecting = 'b';
			break;


		case 4:
			c = HILDON_DIALOG (hildon_dialog_new ());
			gtk_window_set_title (GTK_WINDOW (c), "c (should also be transient to a)");
			gtk_window_set_transient_for (GTK_WINDOW (c), GTK_WINDOW (a));
			gtk_widget_show_all (GTK_WIDGET (c));
			g_warning ("Opened a third window 'c', and set 'c' transient to 'a'");
			expecting = 'c';
			break;

		case 6:
			gtk_object_destroy (GTK_OBJECT (c)); c = NULL;

			g_warning ("Destroyed that third window 'c'; focus should now pass to the other transient, 'b'");
			expecting = 'b';
			break;

		case 8:
			g_warning ("Pass.");
			gtk_object_destroy (GTK_OBJECT (a));  a = NULL;
			gtk_object_destroy (GTK_OBJECT (b));  b = NULL;
			gtk_main_quit ();
			break;

		default: /* odd numbers */
			if (current_window () == expecting)
				g_warning ("%c is focussed as expected", current_window ());
			else
			{
				g_warning ("Fail: %c is focussed but we expected %c.", current_window (),
						expecting);
				//	gtk_main_quit ();
			}
	}
}

	int
main (int argc, char **argv)
{

	gtk_init (&argc, &argv);

	gtk_timeout_add (1000, do_something, NULL);

	gtk_main ();

	return 0;
}
