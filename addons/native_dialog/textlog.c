#include <stdarg.h>
#include <stdio.h>
#include "allegro5/allegro.h"
#include "allegro5/allegro_native_dialog.h"
#include "allegro5/internal/aintern_native_dialog.h"
#include "allegro5/internal/aintern_native_dialog_cfg.h"
#include "allegro5/internal/aintern_dtor.h"
#include "allegro5/internal/aintern_system.h"

/* Text logs are only implemented for GTK+ and Windows so far. */
#if defined(ALLEGRO_CFG_NATIVE_DIALOG_GTK) || defined(ALLEGRO_CFG_NATIVE_DIALOG_WINDOWS) || defined(ALLEGRO_CFG_NATIVE_DIALOG_OSX)
   #define HAVE_TEXT_LOG
#endif


#ifdef HAVE_TEXT_LOG
/* This will only return when the text window is closed. */
static void *textlog_thread_proc(ALLEGRO_THREAD *thread, void *arg)
{
   ALLEGRO_NATIVE_DIALOG *textlog = arg;

   if (!_al_open_native_text_log(textlog)) {
      al_lock_mutex(textlog->tl_text_mutex);
      textlog->tl_init_error = true;
      al_signal_cond(textlog->tl_text_cond);
      al_unlock_mutex(textlog->tl_text_mutex);
   }

   return thread;
}
#endif


/* Function: al_open_native_text_log
 */
ALLEGRO_TEXTLOG *al_open_native_text_log(char const *title, int flags)
{
   ALLEGRO_NATIVE_DIALOG *textlog = NULL;

   /* Avoid warnings when log windows are unimplemented. */
   (void)title;
   (void)flags;

#ifdef HAVE_TEXT_LOG
   textlog = al_calloc(1, sizeof *textlog);
   textlog->title = al_ustr_new(title);
   textlog->flags = flags;
   textlog->tl_thread = al_create_thread(textlog_thread_proc, textlog);
   textlog->tl_text_cond = al_create_cond();
   textlog->tl_text_mutex = al_create_mutex();
   textlog->tl_pending_text = al_ustr_new("");
   al_init_user_event_source(&textlog->tl_events);

   /* Unlike the other dialogs, this one never blocks as the intended
    * use case is a log window running in the background for debugging
    * purposes when no console can be used. Therefore we have it run
    * in a separate thread.
    */
   textlog->tl_init_error = false;
   textlog->tl_done = false;
   al_start_thread(textlog->tl_thread);
   al_lock_mutex(textlog->tl_text_mutex);
   while (!textlog->tl_done && !textlog->tl_init_error) {
      al_wait_cond(textlog->tl_text_cond, textlog->tl_text_mutex);
   }
   al_unlock_mutex(textlog->tl_text_mutex);

   if (textlog->tl_init_error) {
      al_close_native_text_log((ALLEGRO_TEXTLOG *)textlog);
      return NULL;
   }

   _al_register_destructor(_al_dtor_list, textlog,
      (void (*)(void *))al_close_native_text_log);
#endif

   return (ALLEGRO_TEXTLOG *)textlog;
}


/* Function: al_close_native_text_log
 */
void al_close_native_text_log(ALLEGRO_TEXTLOG *textlog)
{
   ALLEGRO_NATIVE_DIALOG *dialog = (ALLEGRO_NATIVE_DIALOG *)textlog;

   if (!dialog)
      return;

#ifdef HAVE_TEXT_LOG
   if (!dialog->tl_init_error) {
      al_lock_mutex(dialog->tl_text_mutex);
      dialog->tl_done = false;

      _al_close_native_text_log(dialog);

      while (!dialog->tl_done) {
         al_wait_cond(dialog->tl_text_cond, dialog->tl_text_mutex);
      }

      _al_unregister_destructor(_al_dtor_list, dialog);
   }

   al_ustr_free(dialog->title);
   al_ustr_free(dialog->tl_pending_text);

   al_destroy_user_event_source(&dialog->tl_events);

   al_unlock_mutex(dialog->tl_text_mutex);

   al_destroy_thread(dialog->tl_thread);
   al_destroy_cond(dialog->tl_text_cond);
   al_destroy_mutex(dialog->tl_text_mutex);
   al_free(dialog);
#endif
}


/* Function: al_append_native_text_log
 */
void al_append_native_text_log(ALLEGRO_TEXTLOG *textlog,
   char const *format, ...)
{
   ALLEGRO_NATIVE_DIALOG *dialog = (ALLEGRO_NATIVE_DIALOG *)textlog;
   va_list args;

   /* Fall back to printf if no window. */
   if (!dialog) {
      va_start(args, format);
      vprintf(format, args);
      va_end(args);
      return;
   }

#ifdef HAVE_TEXT_LOG
   al_lock_mutex(dialog->tl_text_mutex);

   /* We could optimise the case where format="%s". */
   va_start(args, format);
   al_ustr_vappendf(dialog->tl_pending_text, format, args);
   va_end(args);

   _al_append_native_text_log(dialog);

   al_unlock_mutex(dialog->tl_text_mutex);
#endif
}


/* Function: al_get_native_text_log_event_source
 */
ALLEGRO_EVENT_SOURCE *al_get_native_text_log_event_source(
   ALLEGRO_TEXTLOG *textlog)
{
   ALLEGRO_NATIVE_DIALOG *dialog = (ALLEGRO_NATIVE_DIALOG *)textlog;
   ASSERT(dialog);

   return &dialog->tl_events;
}


/* vim: set sts=3 sw=3 et: */
