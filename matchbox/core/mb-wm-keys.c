#include "mb-wm.h"
#include <ctype.h> 		/* isalpha etc */

/**
 * All the keybinding information: a list of keybindings and the current
 * masks for the various modifier keys; in MBWindowManager.
 *
 * \bug FIXME: Probably do want to hide these here
 */
struct MBWMKeys
{
  /** Always points to first binding */
  MBWMList *bindings;

  int MetaMask;
  int HyperMask;
  int SuperMask;
  int AltMask;
  int ModeMask;
  int NumLockMask;
  int ScrollLockMask;
  int lock_mask;
};

static Bool
keysym_needs_shift (MBWindowManager *wm, KeySym keysym)
{
  int    min_kc, max_kc, keycode, col;
  KeySym k;

  XDisplayKeycodes(wm->xdpy, &min_kc, &max_kc);

  for (keycode = min_kc; keycode <= max_kc; keycode++)
    for (col = 0;
	 (k = XKeycodeToKeysym (wm->xdpy, keycode, col)) != NoSymbol;
	 col++)
      if (k == keysym && col == 1)
	return True;

  return False;
}

static Bool
key_binding_set_grab (MBWindowManager *wm,
		      MBWMKeyBinding  *key,
		      Bool             ungrab)
{
  int ignored_mask = 0;

  MBWM_ASSERT (wm->keys != NULL);

  /* Needed to grab all Locked combo's too */
  while (ignored_mask < (int) wm->keys->lock_mask)
    {
      if (ignored_mask & ~(wm->keys->lock_mask))
	{
	  ++ignored_mask;
	  continue;
	}

      if (ungrab)
	{
	  MBWM_DBG("ungrabbing %i , %i",
		   XKeysymToKeycode(wm->xdpy, key->keysym),
		   key->modifier_mask);

	  XUngrabKey(wm->xdpy,
		     XKeysymToKeycode(wm->xdpy, key->keysym),
		     key->modifier_mask | ignored_mask,
		     wm->root_win->xwindow);
	}
      else
	{
	  int result;

	  mb_wm_util_trap_x_errors();

	  MBWM_DBG ("grabbing keycode: %i, keysym %li, mask: %i",
		   XKeysymToKeycode(wm->xdpy, key->keysym),
		   key->keysym,
		   key->modifier_mask | ignored_mask);

	  XGrabKey(wm->xdpy, XKeysymToKeycode(wm->xdpy, key->keysym),
		   key->modifier_mask | ignored_mask,
		   wm->root_win->xwindow, True, GrabModeAsync, GrabModeAsync);

	  result = mb_wm_util_untrap_x_errors();

	  if (result != Success)
	    {
	      if (result == BadAccess)
		mb_wm_util_warn ("Some other program is already using the key %s with modifiers %x as a binding\n",
				 (XKeysymToString(key->keysym)) ? XKeysymToString (key->keysym) : "unknown",
				 key->modifier_mask | ignored_mask );
	      else
		mb_wm_util_warn ("Unable to grab the key %s with modifiers %x as a binding\n",
				 (XKeysymToString(key->keysym)) ? XKeysymToString (key->keysym) : "unknown",
				 key->modifier_mask | ignored_mask );
	      return False;
	    }
	}

      ++ignored_mask;
    }

  return True;
}

void
mb_wm_keys_binding_remove_all (MBWindowManager    *wm)
{

}

void
mb_wm_keys_binding_remove (MBWindowManager    *wm,
			   MBWMKeyBinding     *binding)
{

  key_binding_set_grab (wm, binding, True);
}

MBWMKeyBinding*
mb_wm_keys_binding_add (MBWindowManager    *wm,
			KeySym              ks,
			int                 mask,
			MBWMKeyPressedFunc  press_func,
			MBWMKeyDestroyFunc  destroy_func,
			void               *userdata)
{
  MBWMKeyBinding *binding = NULL;
  MBWMKeys       *keys = wm->keys;

  MBWM_ASSERT (wm->keys != NULL);

  binding = mb_wm_util_malloc0(sizeof(MBWMKeyBinding));

  binding->keysym        = ks;
  binding->modifier_mask = mask;
  binding->pressed       = press_func;
  binding->destroy       = destroy_func;
  binding->userdata      = userdata;

  if (key_binding_set_grab (wm, binding, False))
    {
      keys->bindings = mb_wm_util_list_append(keys->bindings, binding);
      return binding;
    }

  /* Grab failed */
  free(binding);
  return NULL;
}

MBWMKeyBinding*
mb_wm_keys_binding_add_with_spec (MBWindowManager    *wm,
				  const char         *keystr,
				  MBWMKeyPressedFunc  press_func,
				  MBWMKeyDestroyFunc  destroy_func,
				  void               *userdata)
{
  char           *orig, *p, *q, *keydef = NULL;
  int             i = 0, mask = 0;
  Bool            want_shift = False;
  KeySym          ks;
  MBWMKeys       *keys = wm->keys;
  MBWMKeyBinding *binding = NULL;

  struct { char *def; int mask; } lookup[] =
    {
      { "ctrl", ControlMask },
      { "alt",  keys->AltMask },
      { "meta", keys->MetaMask },
      { "super",keys->SuperMask },
      { "hyper",keys->HyperMask },
      { "mod1", Mod1Mask },
      { "mod2", Mod2Mask },
      { "mod3", Mod3Mask },
      { "mod4", Mod4Mask },
      { "mod5", Mod5Mask },
      { "shift",-1 },
      { NULL, 0 }
    };

  orig = p = strdup(keystr);

  /* parse '<mod><mod><mod>key' */

  while (*p != '\0')
    {
      Bool found = False;

      if (*p == '<')
	{
	  q = ++p; i = 0;

	  while (*q != '\0' && *q != '>')
	    q++;

	  if (*q == '\0')
	    goto out; /* Parse error */

	  while (lookup[i].def != NULL && !found)
	    {
	      if (!strncasecmp(p, lookup[i].def, q-p) && lookup[i].mask)
		{
		  if (lookup[i].mask == -1)
		    want_shift = True;
		  else
		    mask |= lookup[i].mask;
		  found = True;
		}
	      i++;
	    }

	  if (found)
	      p = q;
	  else
	    goto out;
	}
      else if (!isspace(*p))
	{
	  keydef = p;
	  break;
	}

      p++;
    }

  if (!keydef)
    goto out;

  MBWM_DBG("keydefinition is %s, want_shift is %i", keydef, want_shift);

  if ((ks = XStringToKeysym(keydef)) == (KeySym)NULL)
    {
      if (islower(keydef[0]))          /* Try again, changing case */
	keydef[0] = toupper(keydef[0]);
      else
	keydef[0] = tolower(keydef[0]);

      if ((ks = XStringToKeysym(keydef)) == (KeySym)NULL)
	{
	  mb_wm_util_warn ("Cant find keysym for %s", keydef);
	  goto out;
	}
    }

  if (keysym_needs_shift(wm, ks) || want_shift)
    mask |= ShiftMask;

  /* If we grab keycode 0, we end up grabbing the entire keyboard.. */
  if (XKeysymToKeycode(wm->xdpy, ks) == 0 && mask == 0)
    {
      MBWM_DBG("Cant find a keycode for keysym %li", ks);
      goto out;
    }

  binding = mb_wm_keys_binding_add (wm, ks, mask,
				    press_func, destroy_func, userdata);

 out:

  free (orig);
  return binding;
}

void 				/* FIXME: rename */
mb_wm_keys_press (MBWindowManager *wm,
		  KeySym           keysym,
		  int              modifier_mask)
{
  MBWMList       *iter;
  MBWMKeyBinding *binding;

  if (!wm->keys)
    return;

  MBWM_DBG ("Looking up keysym <%li>, ( mask %i )", keysym, modifier_mask);

  iter = wm->keys->bindings;

  while (iter)
    {
      int ignored_mask = 0;

      binding = (MBWMKeyBinding*)iter->data;

      MBWM_DBG ("Checking up keysym <%li>, ( mask %i )",
	       binding->keysym,
	       binding->modifier_mask);

      /* FIXME: Assumes multiple bindings per key */
      while (ignored_mask < (int) wm->keys->lock_mask)
	{
	  if (ignored_mask & ~(wm->keys->lock_mask))
	    {
	      ++ignored_mask;
	      continue;
	    }

	  if (binding->pressed
	      && binding->keysym == keysym
	      && binding->modifier_mask | (ignored_mask == modifier_mask))
	    {
	      binding->pressed(wm, binding, binding->userdata);
	      break;
	    }

	  ++ignored_mask;
	}

      iter = mb_wm_util_list_next(iter);
    }
}


Bool
mb_wm_keys_init(MBWindowManager *wm)
{
  int              mod_idx, mod_key, col, kpm;
  XModifierKeymap *mod_map;
  MBWMKeys        *keys;

  mod_map = XGetModifierMapping(wm->xdpy);

  keys = wm->keys = mb_wm_util_malloc0(sizeof(MBWMKeys));

  /* Figure out modifier masks */

  kpm = mod_map->max_keypermod;
  for (mod_idx = 0; mod_idx < 8; mod_idx++)
    for (mod_key = 0; mod_key < kpm; mod_key++)
      {
	KeySym last_sym = 0;
	for (col = 0; col < 4; col += 2)
	  {
	    KeyCode code = mod_map->modifiermap[mod_idx * kpm + mod_key];
	    KeySym sym = (code ? XKeycodeToKeysym(wm->xdpy, code, col) : 0);

	    if (sym == last_sym) continue;
	    last_sym = sym;

	    switch (sym)
	      {
	      case XK_Mode_switch:
		/* XXX store_modifier("Mode_switch", mode_bit); */
		break;
	      case XK_Meta_L:
	      case XK_Meta_R:
		keys->MetaMask |= (1 << mod_idx);
		break;
	      case XK_Super_L:
	      case XK_Super_R:
		keys->SuperMask |= (1 << mod_idx);
		break;
	      case XK_Hyper_L:
	      case XK_Hyper_R:
		keys->HyperMask |= (1 << mod_idx);
		break;
	      case XK_Alt_L:
	      case XK_Alt_R:
		keys->AltMask |= (1 << mod_idx);
		break;
	      case XK_Num_Lock:
		keys->NumLockMask |= (1 << mod_idx);
		break;
	      case XK_Scroll_Lock:
		keys->ScrollLockMask |= (1 << mod_idx);
		break;
	      }
	  }
      }

  /* XXX check this. assume alt <=> meta if only either set */
  if (!keys->AltMask)  keys->AltMask  = keys->MetaMask;
  if (!keys->MetaMask) keys->MetaMask = keys->AltMask;

  keys->lock_mask = keys->ScrollLockMask | keys->NumLockMask | LockMask;

  if (mod_map) XFreeModifiermap(mod_map);

  return True;
}

