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

#include "mb-wm.h"


static void
mb_wm_stack_ensure_trans_foreach (MBWindowManagerClient *client, void *data)
{
  MBWMList * t = mb_wm_client_get_transients (client);

  mb_wm_stack_move_top (client);

  mb_wm_util_list_foreach
    (t, (MBWMListForEachCB) mb_wm_stack_ensure_trans_foreach, NULL);

  mb_wm_util_list_free (t);
}

void
mb_wm_stack_dump (MBWindowManager *wm)
{
  MBWindowManagerClient *client;
  MBWMStackLayerType     stacking_layer;

  g_warning ("\n==== window stack =====\n");

  mb_wm_stack_enumerate_reverse (wm, client)
    {
      MBWindowManagerClient *trans_client = client;
      int                    i = 0, j = 0;
      char                   prefix[128] = {0};

      while ((trans_client = mb_wm_client_get_transient_for(trans_client)))
	i++;

      if (i)
	{
	  for (j=0;j<=i*2;j+=2)
	    { prefix[j] = ' '; prefix[j+1] = ' '; }

	  strcpy(&prefix[i*2], " +--");
	}

      stacking_layer = mb_wm_client_get_stacking_layer (client);

      g_warning ("%s XID: %lx NAME: %s, type %d, layer %d\n",
	      prefix,
	      MB_WM_CLIENT_XWIN(client),
	      client->window->name ? client->window->name : "unknown",
	      MB_WM_CLIENT_CLIENT_TYPE (client),
	      stacking_layer);
    }

  g_warning ("======================\n\n");
}

void
mb_wm_stack_ensure (MBWindowManager *wm)
{
  MBWindowManagerClient *client, *seen, *next;
  int                    i;
  MBWMStackLayerType     stacking_layer;

  if (wm->stack_bottom == NULL)
    return;

  /* Ensure the window stack is corrent;
   *  - with respect to client layer types
   *  - transients are stacked within these layers also
   *
   * We need to be careful here as we modify stacking list
   * in place while enumerating it.
   *
   * FIXME: This isn't optimal
  */

  /* bottom -> top on layer types */
  for (i=1; i<N_MBWMStackLayerTypes; i++)
    {
      /* Push each layer type to top, handling transients */
      client = wm->stack_bottom;
      seen   = NULL;

      while (client != seen && client != NULL)
	{
	  /* get the next valid client ( ignore transients ) before
	   * modifying the list
	  */
	  next = client->stacked_above;

	  while (next && mb_wm_client_get_transient_for (next))
	    next = next->stacked_above;

	  stacking_layer = mb_wm_client_get_stacking_layer (client);

	  if (stacking_layer == i
	      && mb_wm_client_get_transient_for (client) == NULL)
	    {
	      /* Keep track of the first client modified so we
               * know when to stop iterating.
	      */
	      if (seen == NULL)
		seen = client;

	      mb_wm_client_stack (client, 0);
	    }
	  client = next;
	}
    }

  /*  ENABLE ME WHEN YOU NEED ME
  mb_wm_stack_dump (wm);
  */
}

void
mb_wm_stack_insert_above_client (MBWindowManagerClient *client,
				 MBWindowManagerClient *client_below)
{
  MBWindowManager *wm = client->wmref;

  MBWM_ASSERT (client != NULL);

  if (client_below == NULL)
    {
      /* NULL so nothing below add at bottom */
      if (wm->stack_bottom)
	{
	  client->stacked_above = wm->stack_bottom;
	  wm->stack_bottom->stacked_below = client;
	}

      wm->stack_bottom = client;
    }
  else
    {
      client->stacked_below = client_below;
      client->stacked_above = client_below->stacked_above;
      if (client->stacked_below) client->stacked_below->stacked_above = client;
      if (client->stacked_above) client->stacked_above->stacked_below = client;
    }

  if (client_below == wm->stack_top)
    wm->stack_top = client;

  wm->stack_n_clients++;
}


void
mb_wm_stack_append_top (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;

  mb_wm_stack_insert_above_client(client, wm->stack_top);
}

void
mb_wm_stack_prepend_bottom (MBWindowManagerClient *client)
{
  mb_wm_stack_insert_above_client(client, NULL);
}

void
mb_wm_stack_move_client_above_type (MBWindowManagerClient *client,
				    MBWMClientType         type_below)
{
  MBWindowManager       *wm = client->wmref;
  MBWindowManagerClient *highest_client = NULL;

  highest_client = mb_wm_stack_get_highest_by_type (wm, type_below);

  if (highest_client)
    mb_wm_stack_move_above_client(client, highest_client);
}


void
mb_wm_stack_move_above_client (MBWindowManagerClient *client,
			       MBWindowManagerClient *client_below)
{
  if (client == client_below) return;

  MBWM_ASSERT (client != NULL);
  MBWM_ASSERT (client_below != NULL);

  mb_wm_stack_remove(client);
  mb_wm_stack_insert_above_client(client, client_below);
}

MBWindowManagerClient*
mb_wm_stack_get_highest_by_type (MBWindowManager       *wm,
				 MBWMClientType         type)
{
  MBWindowManagerClient *c = NULL;

  mb_wm_stack_enumerate_reverse (wm,c)
    if (MB_WM_CLIENT_CLIENT_TYPE(c) & type)
      return c;

  return NULL;
}

MBWindowManagerClient*
mb_wm_stack_get_highest_full_screen (MBWindowManager       *wm)
{
  MBWindowManagerClient *c = NULL;

  mb_wm_stack_enumerate_reverse (wm,c)
    if (mb_wm_client_window_is_state_set (
	  c->window, MBWMClientWindowEWMHStateFullscreen))
      return c;

  return NULL;
}

MBWindowManagerClient*
mb_wm_stack_get_lowest_by_type(MBWindowManager *w, MBWMClientType wanted_type)

{
  MBWindowManagerClient *c = NULL;

  mb_wm_stack_enumerate(w,c)
    if (MB_WM_CLIENT_CLIENT_TYPE(c) & wanted_type)
      return c;

  return NULL;
}

MBWindowManagerClient *
mb_wm_stack_cycle_by_type(MBWindowManager *wm, MBWMClientType type,
			  Bool reverse)
{
  if (reverse)
    {
      MBWindowManagerClient *prev, *highest;

      highest = mb_wm_stack_get_highest_by_type (wm, type);

      if (!highest)
	return highest;

      prev = highest->stacked_below;

      while (prev && (!(type & MB_WM_CLIENT_CLIENT_TYPE (prev))))
	{
	  prev = prev->stacked_below;
	}

      if (prev && highest && prev != highest)
	{
	  mb_wm_stack_move_above_client (prev, highest);
	}

      return prev;
    }
  else
    {
      MBWindowManagerClient *lowest, *highest;

      lowest  = mb_wm_stack_get_lowest_by_type (wm, type);
      highest = mb_wm_stack_get_highest_by_type (wm, type);

      if (lowest && highest && lowest != highest)
	{
	  mb_wm_stack_move_above_client (lowest, highest);
	}

      return lowest;
    }
}

void
mb_wm_stack_remove (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;

  if (wm->stack_top == wm->stack_bottom)
    {
      if (wm->stack_top != client)
	{
	  MBWM_DBG("Client stack corruption !!!");
	}
      else
	wm->stack_top = wm->stack_bottom = NULL;
    }
  else
    {
      if (client == wm->stack_top)
	{
	  wm->stack_top = client->stacked_below;
	}

      if (client == wm->stack_bottom)
	wm->stack_bottom = client->stacked_above;

      if (client->stacked_below != NULL)
	client->stacked_below->stacked_above = client->stacked_above;
      if (client->stacked_above != NULL)
	client->stacked_above->stacked_below = client->stacked_below;
    }

  client->stacked_above = client->stacked_below = NULL;

  wm->stack_n_clients--;
}


