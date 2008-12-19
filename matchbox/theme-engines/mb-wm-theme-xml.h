#ifndef _HAVE_MB_WM_THEME_PRIVATE_H
#define _HAVE_MB_WM_THEME_PRIVATE_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/theme-engines/mb-wm-theme.h>
/*
 * Helper structs for xml theme
 */
typedef struct Button
{
  MBWMDecorButtonType type;
  MBWMDecorButtonPack packing;

  MBWMColor clr_fg;
  MBWMColor clr_bg;

  int x;
  int y;
  int width;
  int height;

  /* Needed by png themes */
  int active_x;
  int active_y;

  int inactive_x;
  int inactive_y;

  int press_activated;
} MBWMXmlButton;

typedef enum _MBWMXmlFontUnits
{
  MBWMXmlFontUnitsPixels,
  MBWMXmlFontUnitsPoints,
} MBWMXmlFontUnits;

typedef struct Decor
{
  MBWMDecorType type;

  MBWMColor clr_fg;
  MBWMColor clr_bg;

  int x;
  int y;
  int width;
  int height;
  int pad_offset;
  int pad_length;
  int show_title;

  int                font_size;
  MBWMXmlFontUnits   font_units;
  char             * font_family;

  MBWMList * buttons;
}MBWMXmlDecor;

typedef struct Client
{
  MBWMClientType  type;

  int client_x;
  int client_y;
  int client_width;
  int client_height;

  Bool shaped;

  MBWMList       *decors;

  MBWMClientLayoutHints layout_hints;
}MBWMXmlClient;

MBWMXmlButton *
mb_wm_xml_button_new ();

void
mb_wm_xml_button_free (MBWMXmlButton * b);

MBWMXmlDecor *
mb_wm_xml_decor_new ();

void
mb_wm_xml_decor_free (MBWMXmlDecor * d);

MBWMXmlClient *
mb_wm_xml_client_new ();

void
mb_wm_xml_client_free (MBWMXmlClient * c);

MBWMXmlClient *
mb_wm_xml_client_find_by_type (MBWMList *l, MBWMClientType type);

MBWMXmlDecor *
mb_wm_xml_decor_find_by_type (MBWMList *l, MBWMDecorType type);

MBWMXmlButton *
mb_wm_xml_button_find_by_type (MBWMList *l, MBWMDecorButtonType type);

void
mb_wm_xml_clr_from_string (MBWMColor * clr, const char *s);

#endif
