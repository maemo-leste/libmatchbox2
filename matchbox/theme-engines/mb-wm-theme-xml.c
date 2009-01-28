/*
 *  Matchbox Window Manager 2 - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2007 OpenedHand Ltd - http://o-hand.com
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

#include "mb-wm-theme-xml.h"
#include "mb-wm-theme.h"

/*****************************************************************
 * XML Parser stuff
 */
MBWMXmlButton *
mb_wm_xml_button_new ()
{
  MBWMXmlButton * b = mb_wm_util_malloc0 (sizeof (MBWMXmlButton));

  b->x = 0;
  b->y = 0;
  b->active_x = 0;
  b->active_y = 0;
  b->inactive_x = 0;
  b->inactive_y = 0;

  return b;
}

void
mb_wm_xml_button_free (MBWMXmlButton * b)
{
  free (b);
}

MBWMXmlDecor *
mb_wm_xml_decor_new ()
{
  MBWMXmlDecor * d = mb_wm_util_malloc0 (sizeof (MBWMXmlDecor));
  return d;
}

void
mb_wm_xml_decor_free (MBWMXmlDecor * d)
{
  MBWMList * l;

  if (!d)
    return;

  l = d->buttons;
  while (l)
    {
      MBWMXmlButton * b = l->data;
      MBWMList * n = l->next;
      mb_wm_xml_button_free (b);
      free (l);

      l = n;
    }

  if (d->font_family)
    free (d->font_family);

  free (d);
}

MBWMXmlClient *
mb_wm_xml_client_new ()
{
  MBWMXmlClient * c = mb_wm_util_malloc0 (sizeof (MBWMXmlClient));

  c->client_x      = -1;
  c->client_y      = -1;
  c->client_width  = -1;
  c->client_height = -1;

  return c;
}

void
mb_wm_xml_client_free (MBWMXmlClient * c)
{
  MBWMList * l;

  if (!c)
    return;

  l = c->decors;
  while (l)
    {
      MBWMXmlDecor * d = l->data;
      MBWMList * n = l->next;
      mb_wm_xml_decor_free (d);
      free (l);

      l = n;
    }

  free (c);
}

void
mb_wm_xml_clr_from_string (MBWMColor * clr, const char *s)
{
  int  r, g, b, a;

  if (!s || *s != '#')
    {
      clr->set = False;
      return;
    }

  sscanf (s+1,"%2x%2x%2x%2x", &r, &g, &b, &a);
  clr->r = (double) r / 255.0;
  clr->g = (double) g / 255.0;
  clr->b = (double) b / 255.0;
  clr->a = (double) a / 255.0;

  clr->set = True;
}

MBWMXmlClient *
mb_wm_xml_client_find_by_type (MBWMList *l, MBWMClientType type)
{
  while (l)
    {
      MBWMXmlClient * c = l->data;
      if (c->type == type)
	return c;

      l = l->next;
    }

  return NULL;
}

MBWMXmlDecor *
mb_wm_xml_decor_find_by_type (MBWMList *l, MBWMDecorType type)
{
  while (l)
    {
      MBWMXmlDecor * d = l->data;
      if (d->type == type)
	return d;

      l = l->next;
    }

  return NULL;
}

MBWMXmlButton *
mb_wm_xml_button_find_by_type (MBWMList *l, MBWMDecorButtonType type)
{
  while (l)
    {
      MBWMXmlButton * b = l->data;
      if (b->type == type)
	return b;

      l = l->next;
    }

  return NULL;
}

#if 0
void
mb_wm_xml_client_dump (MBWMList * l)
{
  printf ("=== XML Clients =====\n");
  while (l)
    {
      MBWMXmlClient * c = l->data;
      MBWMList *l2 = c->decors;
      printf ("===== client type %d =====\n", c->type);

      while (l2)
	{
	  MBWMXmlDecor * d = l2->data;
	  MBWMList *l3 = d->buttons;
	  printf ("======= decor type %d =====\n", d->type);

	  while (l3)
	    {
	      MBWMXmlButton * b = l3->data;
	      printf ("========= button type %d =====\n", d->type);

	      l3 = l3->next;
	    }

	  l2 = l2->next;
	}

      l = l->next;
    }
  printf ("=== XML Clients End =====\n");
}
#endif
