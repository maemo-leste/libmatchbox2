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

#ifndef _HAVE_MB_WM_DECOR_H
#define _HAVE_MB_WM_DECOR_H

#define MB_WM_DECOR(c) ((MBWMDecor*)(c))
#define MB_WM_DECOR_CLASS(c) ((MBWMDecorClass*)(c))
#define MB_WM_TYPE_DECOR (mb_wm_decor_class_type ())


#define MB_WM_DECOR_BUTTON(c) ((MBWMDecorButton*)(c))
#define MB_WM_DECOR_BUTTON_CLASS(c) ((MBWMDecorButtonClass*)(c))
#define MB_WM_TYPE_DECOR_BUTTON (mb_wm_decor_button_class_type ())

typedef void (*MBWMDecorButtonPressedFunc) (MBWindowManager   *wm,
					    MBWMDecorButton   *button,
					    void              *userdata);

typedef void (*MBWMDecorButtonReleasedFunc) (MBWindowManager   *wm,
					     MBWMDecorButton   *button,
					     void              *userdata);


typedef void (*MBWMDecorDestroyUserData)       (MBWMDecor *, void *);
typedef void (*MBWMDecorButtonDestroyUserData) (MBWMDecorButton *, void *);

typedef enum MBWMDecorDirtyState
{
  MBWMDecorDirtyNot   = 0,
  MBWMDecorDirtyTitle = (1<<0),
  MBWMDecorDirtyPaint = (1<<1),

  MBWMDecorDirtyFull  = 0xffffffff,
} MBWMDecorDirtyState;

/**
 * A decor (that is, a description of one of the four edges of a window's
 * decorations); each decorated MBWindowManagerClient has four of these.
 */
struct MBWMDecor
{
  MBWMObject                parent;
  MBWMDecorType             type;
  Window                    xwin;
  MBWindowManagerClient    *parent_client;
  MBGeometry                geom;
  MBWMDecorDirtyState       dirty;
  Bool                      absolute_packing;
  /**
   * A list of MBWMDecorButton objects.
   */
  MBWMList                 *buttons;
  int                       pack_start_x;
  int                       pack_end_x;

  unsigned long             press_cb_id;
  unsigned long             release_cb_id;

  void                     *themedata;
  MBWMDecorDestroyUserData  destroy_themedata;
};

/**
 * Class of MBWMDecor.
 */
struct MBWMDecorClass
{
  MBWMObjectClass        parent;
};


MBWMDecor*
mb_wm_decor_new (MBWindowManager     *wm,
		 MBWMDecorType        type);

void
mb_wm_decor_handle_repaint (MBWMDecor *decor);

void
mb_wm_decor_handle_resize (MBWMDecor *decor);

Window
mb_wm_decor_get_x_window (MBWMDecor *decor);

MBWMDecorType
mb_wm_decor_get_type (MBWMDecor *decor);

int
mb_wm_decor_get_pack_start_x (MBWMDecor *decor);

int
mb_wm_decor_get_pack_end_x (MBWMDecor *decor);

const MBGeometry*
mb_wm_decor_get_geometry (MBWMDecor *decor);

MBWindowManagerClient*
mb_wm_decor_get_parent (MBWMDecor *decor);

void
mb_wm_decor_mark_dirty (MBWMDecor *decor);

void
mb_wm_decor_mark_title_dirty (MBWMDecor *decor);

MBWMDecorDirtyState
mb_wm_decor_get_dirty_state (MBWMDecor *decor);

void
mb_wm_decor_attach (MBWMDecor             *decor,
		    MBWindowManagerClient *client);

void
mb_wm_decor_detach (MBWMDecor *decor);

void
mb_wm_decor_set_theme_data (MBWMDecor * decor, void *userdata,
			    MBWMDecorDestroyUserData destroy);

void *
mb_wm_decor_get_theme_data (MBWMDecor* decor);

typedef enum MBWMDecorButtonState
{
  MBWMDecorButtonStateInactive = 0,
  MBWMDecorButtonStatePressed

} MBWMDecorButtonState ;

typedef enum MBWMDecorButtonType
{
  MBWMDecorButtonCustom     = 0,
  MBWMDecorButtonClose      = 1,
  MBWMDecorButtonMenu       = 2,
  MBWMDecorButtonMinimize   = 3,
  MBWMDecorButtonFullscreen = 4,
  MBWMDecorButtonAccept     = 5,
  MBWMDecorButtonHelp       = 6,
} MBWMDecorButtonType;

typedef enum MBWMDecorButtonPack
  {
    MBWMDecorButtonPackStart = 0,
    MBWMDecorButtonPackEnd   = 1,
  }
MBWMDecorButtonPack;

/**
 * One of the buttons on a MBWMDecor.
 */
struct MBWMDecorButton
{
  MBWMObject                  parent;
  MBWMDecorButtonType         type;
  MBWMDecorButtonPack         pack;
  MBWMDecor                  *decor;

  MBGeometry                  geom;

  /* in priv ? */
  Bool                        visible;
  Bool                        needs_sync;
  Bool                        realized;
  Bool                        press_activated;
  MBWMDecorButtonState        state;
  MBWMDecorButtonFlags        flags;

  MBWMDecorButtonPressedFunc  press;
  MBWMDecorButtonReleasedFunc release;

  unsigned long               press_cb_id;

  /* Data for any custom button callbacks */
  void                       *userdata;
  MBWMDecorButtonDestroyUserData destroy_userdata;

  /* Data utilized by the theme engine */
  void                       *themedata;
  MBWMDecorButtonDestroyUserData destroy_themedata;

};

/**
 * Class for an MBWMDecorButton.
 */
struct MBWMDecorButtonClass
{
  MBWMObjectClass   parent;

  /*
     show;
     hide;
     realize; ??
  */
};

typedef enum
{
  MBWMDecorButtonSignalPressed = 1,
  MBWMDecorButtonSignalReleased = 2,
} MBWMDecorButtonSignal;

int
mb_wm_decor_button_class_type (void);

void
mb_wm_decor_button_show (MBWMDecorButton *button);

void
mb_wm_decor_button_hide (MBWMDecorButton *button);

void
mb_wm_decor_button_move_to (MBWMDecorButton *button, int x, int y);

void
mb_wm_decor_button_handle_repaint (MBWMDecorButton *button);

MBWMDecorButton*
mb_wm_decor_button_new (MBWindowManager               *wm,
			MBWMDecorButtonType            type,
			MBWMDecorButtonPack            pack,
			MBWMDecor                     *decor,
			MBWMDecorButtonPressedFunc     press,
			MBWMDecorButtonReleasedFunc    release,
			MBWMDecorButtonFlags           flags);


MBWMDecorButton*
mb_wm_decor_button_stock_new (MBWindowManager            *wm,
			      MBWMDecorButtonType         type,
			      MBWMDecorButtonPack         pack,
			      MBWMDecor                  *decor,
			      MBWMDecorButtonFlags        flags);

void
mb_wm_decor_button_set_user_data (MBWMDecorButton * button, void *userdata,
				  MBWMDecorButtonDestroyUserData destroy);

void *
mb_wm_decor_button_get_user_data (MBWMDecorButton * button);

void
mb_wm_decor_button_set_theme_data (MBWMDecorButton * button, void *themedata,
				   MBWMDecorButtonDestroyUserData destroy);

void *
mb_wm_decor_button_get_theme_data (MBWMDecorButton* button);

int
mb_wm_decor_class_type ();

#endif
