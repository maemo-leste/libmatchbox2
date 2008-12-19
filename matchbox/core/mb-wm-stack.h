/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *
 *  Copyright (c) 2005 OpenedHand Ltd - http://o-hand.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_MB_WM_STACK_H_
#define _HAVE_MB_WM_STACK_H_

#define mb_wm_stack_enumerate(w,c)                               \
 if ((w)->stack_bottom)                                          \
   for ((c)=(w)->stack_bottom; (c) != NULL; (c)=(c)->stacked_above)

#define mb_wm_stack_enumerate_reverse(w,c)                       \
 if ((w)->stack_top)                                             \
   for ((c)=(w)->stack_top; (c) != NULL; (c)=(c)->stacked_below)

#define mb_wm_stack_enumerate_transients(w,c,t)                   \
 if ((w)->stack_bottom)                                           \
   for ((c)=(w)->stack_bottom; (c) != NULL; (c)=(c)->stacked_above) \
     if ((c)->trans == (t))


#define mb_wm_stack_move_top(c)                                   \
 mb_wm_stack_move_above_client((c), (c)->wmref->stack_top)

/*
#define mb_wm_stack_add_bottom(c)                                 \
 stack_move_below_client((c), (c)->wmref->stack_bottom)
*/

#define mb_wm_stack_empty(w) \
 ((w)->stack_bottom == NULL)

#define mb_wm_stack_size(w) \
 (w)->stack_n_clients

void
mb_wm_stack_ensure (MBWindowManager *wm);

void
mb_wm_stack_insert_above_client (MBWindowManagerClient *client,
				 MBWindowManagerClient *client_below);

void
mb_wm_stack_append_top (MBWindowManagerClient *client);

void
mb_wm_stack_prepend_bottom (MBWindowManagerClient *client);

void
mb_wm_stack_move_client_above_type (MBWindowManagerClient *client,
				    MBWMClientType         type_below);

void
mb_wm_stack_move_client_above_type (MBWindowManagerClient *client,
				    MBWMClientType         type_below);

MBWindowManagerClient *
mb_wm_stack_cycle_by_type(MBWindowManager *w, MBWMClientType type,
			  Bool reverse);

void
mb_wm_stack_move_above_client (MBWindowManagerClient *client,
			       MBWindowManagerClient *client_below);

MBWindowManagerClient*
mb_wm_stack_get_highest_by_type(MBWindowManager *w,
				MBWMClientType wanted_type);

MBWindowManagerClient*
mb_wm_stack_get_highest_full_screen (MBWindowManager       *wm);

MBWindowManagerClient*
mb_wm_stack_get_lowest_by_type(MBWindowManager *w,
			       MBWMClientType wanted_type);

void
mb_wm_stack_remove (MBWindowManagerClient *client);

void
mb_wm_stack_dump (MBWindowManager *wm);

#endif
