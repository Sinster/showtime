/*
 *  Notifications
 *  Copyright (C) 2009 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdio.h>

#include "showtime.h"
#include "prop/prop.h"
#include "notifications.h"
#include "misc/callout.h"
#include "event.h"
#include "keyring.h"

static prop_t *notify_prop_entries;

/**
 *
 */
static void
setstr(char **p, htsmsg_t *m, const char *fname)
{
  const char *s;

  if(p == NULL)
    return;

  if((s = htsmsg_get_str(m, fname)) != NULL)
    *p = strdup(s);
  else
    *p = NULL;
}

/**
 *
 */
void
notifications_init(void)
{
  prop_t *root = prop_create(prop_get_global(), "notifications");
  
  notify_prop_entries = prop_create(root, "nodes");

}

/**
 *
 */
static void
notify_timeout(callout_t *c, void *aux)
{
  prop_t *p = aux;
  prop_destroy(p);
  prop_ref_dec(p);
  free(c);
}

/**
 *
 */
void *
notify_add(prop_t *root, notify_type_t type, const char *icon, int delay,
	   rstr_t *fmt, ...)
{
  char msg[256];
  prop_t *p;
  const char *typestr;
  int tl;
  va_list ap, apx;

  switch(type) {
  case NOTIFY_INFO:    typestr = "info";    tl = TRACE_INFO;  break;
  case NOTIFY_WARNING: typestr = "warning"; tl = TRACE_INFO;  break;
  case NOTIFY_ERROR:   typestr = "error";   tl = TRACE_ERROR; break;
  default: return NULL;
  }
  
  va_start(ap, fmt);
  va_copy(apx, ap);

  tracev(0, tl, "notify", rstr_get(fmt), ap);

  vsnprintf(msg, sizeof(msg), rstr_get(fmt), apx);

  va_end(ap);
  va_end(apx);

  rstr_release(fmt);

  p = prop_create_root(NULL);

  prop_set_string(prop_create(p, "text"), msg);
  prop_set_string(prop_create(p, "type"), typestr);

  if(icon != NULL)
    prop_set_string(prop_create(p, "icon"), icon);

  p = prop_ref_inc(p);

  if(prop_set_parent(p, root ?: notify_prop_entries))
    prop_destroy(p);
  
  if(delay != 0) {
    callout_arm(NULL, notify_timeout, p, delay);
    return NULL;
  }
  return p;
}

/**
 *
 */
void
notify_destroy(void *p)
{
  prop_destroy(p);
  prop_ref_dec(p);
}







/**
 *
 */
static void 
eventsink(void *opaque, prop_event_t event, ...)
{
  event_t *e, **ep = opaque;
  va_list ap;
  va_start(ap, event);

  if(event != PROP_EXT_EVENT)
    return;

  if(*ep)
    event_release(*ep);
  e = va_arg(ap, event_t *);
  atomic_add(&e->e_refcount, 1);
  *ep = e;
}


/**
 *
 */
event_t *
popup_display(prop_t *p)
{
  prop_courier_t *pc = prop_courier_create_waitable();
  event_t *e = NULL;

  prop_t *r = prop_create(p, "eventSink");
  prop_sub_t *s = prop_subscribe(0, 
				 PROP_TAG_CALLBACK, eventsink, &e, 
				 PROP_TAG_ROOT, r,
				 PROP_TAG_COURIER, pc,
				 NULL);

  /* Will show the popup */
  if(prop_set_parent(p, prop_create(prop_get_global(), "popups"))) {
    /* popuproot is a zombie, this is an error */
    abort();
  }

  while(e == NULL)
    prop_courier_wait_and_dispatch(pc);

  prop_unsubscribe(s);
  return e;
}



/**
 *
 */
int
message_popup(const char *message, int flags)
{
  prop_t *p;
  int rval;

  p = prop_create_root(NULL);

  prop_set_string(prop_create(p, "type"), "message");
  prop_set_string_ex(prop_create(p, "message"), NULL, message,
		     flags & MESSAGE_POPUP_RICH_TEXT ?
		     PROP_STR_RICH : PROP_STR_UTF8);
  if(flags & MESSAGE_POPUP_CANCEL)
    prop_set_int(prop_create(p, "cancel"), 1);
  if(flags & MESSAGE_POPUP_OK)
    prop_set_int(prop_create(p, "ok"), 1);

  event_t *e = popup_display(p);
  prop_destroy(p);
  
  if(event_is_action(e, ACTION_OK))
    rval = MESSAGE_POPUP_OK;
  else if(event_is_action(e, ACTION_CANCEL))
    rval = MESSAGE_POPUP_CANCEL;
  else
    rval = 0;

  event_release(e);
  return rval;
}



/**
 *
 */
int
text_dialog(const char *message, char** answer, int flags)
{
  htsmsg_t *m;
  rstr_t *r;

  prop_t *p = prop_create_root(NULL);

  prop_set_string(prop_create(p, "type"), "textDialog");
  prop_set_string_ex(prop_create(p, "message"), NULL, message,
		     flags & MESSAGE_POPUP_RICH_TEXT ?
		     PROP_STR_RICH : PROP_STR_UTF8);
  prop_t *string = prop_create(p, "input");
  if(flags & MESSAGE_POPUP_CANCEL)
    prop_set_int(prop_create(p, "cancel"), 1);
  if(flags & MESSAGE_POPUP_OK)
    prop_set_int(prop_create(p, "ok"), 1);
  
  event_t *e = popup_display(p);
  
  if(event_is_action(e, ACTION_OK)) {
    m = htsmsg_create_map();  
      
    r = prop_get_string(string);
    htsmsg_add_str(m, "input", r ? rstr_get(r) : "");
    rstr_release(r);
      
    htsmsg_get_str(m, "input");
    
    setstr(answer, m, "input");
  }
  
  prop_destroy(p);
  
  if(event_is_action(e, ACTION_CANCEL)){
      event_release(e);
      return -1;
  } 

  event_release(e);
  
  return 0;
}

/**
 *
 */
int
text_dialog_kbrd(const char *message, char** answer, int flags)
{
  htsmsg_t *m;
  rstr_t *r;

  prop_t *p = prop_create_root(NULL);

  prop_set_string(prop_create(p, "type"), "textDialogKbrd");
  prop_set_string_ex(prop_create(p, "message"), NULL, message,
		     flags & MESSAGE_POPUP_RICH_TEXT ?
		     PROP_STR_RICH : PROP_STR_UTF8);
  prop_t *string = prop_create(p, "input");
  if(flags & MESSAGE_POPUP_CANCEL)
    prop_set_int(prop_create(p, "cancel"), 1);
  if(flags & MESSAGE_POPUP_OK)
    prop_set_int(prop_create(p, "ok"), 1);
  
  event_t *e = popup_display_kbrd(p, string);
  if (event_is_action(e, ACTION_OK))
  {
    m = htsmsg_create_map();  
      
    r = prop_get_string(string);
    htsmsg_add_str(m, "input", r ? rstr_get(r) : "");
    rstr_release(r);
      
    htsmsg_get_str(m, "input");
    
    setstr(answer, m, "input");
  }
  
  prop_destroy(p);
  
  if(event_is_action(e, ACTION_CANCEL)){
      event_release(e);
      return -1;
  } 

  event_release(e);
  
  return 0;
}
event_t *
popup_display_kbrd(prop_t *p, prop_t *string)
{
  prop_courier_t *pc = prop_courier_create_waitable();
  event_t *e = NULL;

  prop_t *r = prop_create(p, "eventSink");
  prop_sub_t *s = prop_subscribe(0, 
				 PROP_TAG_CALLBACK, eventsink, &e, 
				 PROP_TAG_ROOT, r,
				 PROP_TAG_COURIER, pc,
				 NULL);

  /* Will show the popup */
  if(prop_set_parent(p, prop_create(prop_get_global(), "popups"))) {
    /* popuproot is a zombie, this is an error */
    abort();
  }

  while (e == NULL || (!event_is_action(e, ACTION_OK) && !event_is_action(e, ACTION_CANCEL)))
  {
    while(e == NULL)
      prop_courier_wait_and_dispatch(pc);
    if (!event_is_action(e, ACTION_OK) && !event_is_action(e, ACTION_CANCEL))
    {
      char *tmpInput;
      htsmsg_t *m;
      rstr_t *r;
      m = htsmsg_create_map();  
      
      r = prop_get_string(string);
      htsmsg_add_str(m, "input", r ? rstr_get(r) : "");
      rstr_release(r);
      htsmsg_get_str(m, "input");
    
      setstr(&tmpInput, m, "input");
      if (event_is_action(e, ACTION_KBRD_A))
        strcat(tmpInput, "a");
      else if (event_is_action(e, ACTION_KBRD_B))
        strcat(tmpInput, "b");
      else if (event_is_action(e, ACTION_KBRD_C))
        strcat(tmpInput, "c");
      else if (event_is_action(e, ACTION_KBRD_D))
        strcat(tmpInput, "d");
      else if (event_is_action(e, ACTION_KBRD_E))
        strcat(tmpInput, "e");
      else if (event_is_action(e, ACTION_KBRD_F))
        strcat(tmpInput, "f");
      else if (event_is_action(e, ACTION_KBRD_G))
        strcat(tmpInput, "g");
      else if (event_is_action(e, ACTION_KBRD_H))
        strcat(tmpInput, "h");
      else if (event_is_action(e, ACTION_KBRD_I))
        strcat(tmpInput, "i");
      else if (event_is_action(e, ACTION_KBRD_J))
        strcat(tmpInput, "j");
      else if (event_is_action(e, ACTION_KBRD_K))
        strcat(tmpInput, "k");
      else if (event_is_action(e, ACTION_KBRD_L))
        strcat(tmpInput, "l");
      else if (event_is_action(e, ACTION_KBRD_M))
        strcat(tmpInput, "m");
      else if (event_is_action(e, ACTION_KBRD_N))
        strcat(tmpInput, "n");
      else if (event_is_action(e, ACTION_KBRD_O))
        strcat(tmpInput, "o");
      else if (event_is_action(e, ACTION_KBRD_P))
        strcat(tmpInput, "p");
      else if (event_is_action(e, ACTION_KBRD_Q))
        strcat(tmpInput, "q");
      else if (event_is_action(e, ACTION_KBRD_R))
        strcat(tmpInput, "r");
      else if (event_is_action(e, ACTION_KBRD_S))
        strcat(tmpInput, "s");
      else if (event_is_action(e, ACTION_KBRD_T))
        strcat(tmpInput, "t");
      else if (event_is_action(e, ACTION_KBRD_U))
        strcat(tmpInput, "u");
      else if (event_is_action(e, ACTION_KBRD_V))
        strcat(tmpInput, "v");
      else if (event_is_action(e, ACTION_KBRD_W))
        strcat(tmpInput, "w");
      else if (event_is_action(e, ACTION_KBRD_X))
        strcat(tmpInput, "x");
      else if (event_is_action(e, ACTION_KBRD_Y))
        strcat(tmpInput, "y");
      else if (event_is_action(e, ACTION_KBRD_Z))
        strcat(tmpInput, "z");
      else if (event_is_action(e, ACTION_KBRD_0))
        strcat(tmpInput, "0");
      else if (event_is_action(e, ACTION_KBRD_1))
        strcat(tmpInput, "1");
      else if (event_is_action(e, ACTION_KBRD_2))
        strcat(tmpInput, "2");
      else if (event_is_action(e, ACTION_KBRD_3))
        strcat(tmpInput, "3");
      else if (event_is_action(e, ACTION_KBRD_4))
        strcat(tmpInput, "4");
      else if (event_is_action(e, ACTION_KBRD_5))
        strcat(tmpInput, "5");
      else if (event_is_action(e, ACTION_KBRD_6))
        strcat(tmpInput, "6");
      else if (event_is_action(e, ACTION_KBRD_7))
        strcat(tmpInput, "7");
      else if (event_is_action(e, ACTION_KBRD_8))
        strcat(tmpInput, "8");
      else if (event_is_action(e, ACTION_KBRD_9))
        strcat(tmpInput, "9");
      else if (event_is_action(e, ACTION_KBRD_COMMA))
        strcat(tmpInput, ",");
      else if (event_is_action(e, ACTION_KBRD_DOT))
        strcat(tmpInput, ".");
      else if (event_is_action(e, ACTION_KBRD_SPACE))
        strcat(tmpInput, " ");
      else if (event_is_action(e, ACTION_BS))
      {
        if (len > 0)
        {
          strncpy(tmpInput, tmpInput, strlen(tmpInput) - 1);
	  tmpInput[strlen(tmpInput) - 1] = '\0';
        }
      }
      prop_set_string(string, tmpInput);
      e = NULL;
    }
  }
  prop_unsubscribe(s);
  return e;
}
