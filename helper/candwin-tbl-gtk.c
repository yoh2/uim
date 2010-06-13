/*

  Copyright (c) 2003-2010 uim Project http://code.google.com/p/uim/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <uim/uim.h>
#include <uim/uim-helper.h>
#include <uim/uim-scm.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../gtk/caret-state-indicator.h"

#define UIM_TYPE_CANDIDATE_WINDOW	(candidate_window_get_type())
#define UIM_CANDIDATE_WINDOW(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), candidate_window_get_type(), UIMCandidateWindow))
#define UIM_IS_CANDIDATE_WINDOW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), UIM_TYPE_CANDIDATE_WINDOW))
#define UIM_IS_CANDIDATE_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), UIM_TYPE_CANDIDATE_WINDOW))
#define UIM_CANDIDATE_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UIM_TYPE_CANDIDATE_WINDOW, UIMCandidateWindowClass))

typedef struct _UIMCandidateWindow	UIMCandidateWindow;
typedef struct _UIMCandidateWindowClass	UIMCandidateWindowClass;

struct _UIMCandidateWindow {
  GtkWindow parent;

  GtkWidget *view;
  GtkWidget *num_label;
  GtkWidget *scrolled_window;
  GtkWidget *viewport;
  GtkWidget *vbox;
  GtkWidget *frame;

  GPtrArray *stores;
  GPtrArray *buttons;
  gchar *labelchar_table;

  guint nr_candidates;
  guint display_limit;
  gint candidate_index;
  gint page_index;

  gint pos_x;
  gint pos_y;
  gint width;
  gint height;

  GtkWidget *caret_state_indicator;

  gboolean is_active;
  gboolean need_hilite;
};

struct _UIMCandidateWindowClass {
  GtkWindowClass parent_class;

  /* signals */
  void (*index_changed) (UIMCandidateWindowClass *candwin);
};

static UIMCandidateWindow *cwin; /* use single candwin */

GType candidate_window_get_type(void);
UIMCandidateWindow *candidate_window_new(void);

/* copied from uim-cand-win-gtk.c */
static gint uim_cand_win_gtk_get_index(UIMCandidateWindow *cwin);
static void uim_cand_win_gtk_set_index(UIMCandidateWindow *cwin, gint index);
static void uim_cand_win_gtk_set_page(UIMCandidateWindow *cwin, gint page);
static void uim_cand_win_gtk_set_page_candidates(UIMCandidateWindow *cwin, guint page, GSList *candidates);

static void uim_cand_win_gtk_layout(void);
static void uim_cand_win_gtk_show(UIMCandidateWindow *cwin);

#define CANDWIN_DEFAULT_WIDTH	80

enum {
  INDEX_CHANGED_SIGNAL,
  NR_SIGNALS
};

enum {
  TERMINATOR = -1,
  COLUMN_CANDIDATE1,
  COLUMN_CANDIDATE2,
  COLUMN_CANDIDATE3,
  COLUMN_CANDIDATE4,
  COLUMN_CANDIDATE5,
  COLUMN_CANDIDATE6,
  COLUMN_CANDIDATE7,
  COLUMN_CANDIDATE8,
  COLUMN_CANDIDATE9,
  COLUMN_CANDIDATE10,
  COLUMN_CANDIDATE11,
  COLUMN_CANDIDATE12,
  COLUMN_CANDIDATE13
};

#define LABELCHAR_NR_COLUMNS 13
#define LABELCHAR_NR_ROWS 8
#define LABELCHAR_NR_CELLS (LABELCHAR_NR_COLUMNS * LABELCHAR_NR_ROWS)
#define INDEX(row,col) ((row) * LABELCHAR_NR_COLUMNS + (col))
/* 106 keyboard */
static gchar default_labelchar_table[LABELCHAR_NR_CELLS] = {
  '1','2','3','4','5', '6','7','8','9','0',   '-','^','\\',
  'q','w','e','r','t', 'y','u','i','o','p',   '@','[','\0',
  'a','s','d','f','g', 'h','j','k','l',';',   ':',']','\0',
  'z','x','c','v','b', 'n','m',',','.','/',   '\0','\0',' ',
  '!','"','#','$','%', '&','\'','(',')','\0', '=','~','|',
  'Q','W','E','R','T', 'Y','U','I','O','P',   '`','{','\0',
  'A','S','D','F','G', 'H','J','K','L','+',   '*','}','\0',
  'Z','X','C','V','B', 'N','M','<','>','?',   '_','\0','\0',
};
/* labelchar_table consists of four blocks
 *   blockLR  blockA
 *   blockLRS blockAS
 */
#define BLOCK_A_ROW_START 0
#define BLOCK_A_ROW_END 4
#define BLOCK_A_COLUMN_START 10
#define BLOCK_A_COLUMN_END LABELCHAR_NR_COLUMNS
#define BLOCK_LRS_ROW_START BLOCK_A_ROW_END
#define BLOCK_LRS_ROW_END LABELCHAR_NR_ROWS
#define BLOCK_LRS_COLUMN_START 0
#define BLOCK_LRS_COLUMN_END BLOCK_A_COLUMN_START
#define BLOCK_AS_ROW_START BLOCK_LRS_ROW_START
#define BLOCK_AS_ROW_END BLOCK_LRS_ROW_END
#define BLOCK_AS_COLUMN_START BLOCK_LRS_COLUMN_END
#define BLOCK_AS_COLUMN_END LABELCHAR_NR_COLUMNS

#define BLOCK_SPACING 20
#define HOMEPOSITION_SPACING 2
#define BLOCK_SPACING 20
#define HOMEPOSITION_SPACING 2
#define SPACING_LEFT_BLOCK_COLUMN 4
#define SPACING_RIGHT_BLOCK_COLUMN (BLOCK_A_COLUMN_START - 1)
#define SPACING_UP_BLOCK_ROW (BLOCK_A_ROW_END - 1)
#define SPACING_LEFTHAND_FAR_COLUMN 3
#define SPACING_RIGHTHAND_FAR_COLUMN 5
#define SPACING_UPPER_FAR_ROW 0
#define SPACING_SHIFT_UPPER_FAR_ROW 4

static void candidate_window_init(UIMCandidateWindow *cwin);
static void candidate_window_class_init(UIMCandidateWindowClass *klass);

static gboolean configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data);

static GType candidate_window_type = 0;
static GTypeInfo const object_info = {
  sizeof (UIMCandidateWindowClass),
  NULL,
  NULL,
  (GClassInitFunc) candidate_window_class_init,
  NULL,
  NULL,
  sizeof(UIMCandidateWindow),
  0,
  (GInstanceInitFunc) candidate_window_init,
};

static gint cand_win_gtk_signals[NR_SIGNALS] = {0};

static unsigned int read_tag;

static void init_candidate_win(void);
static void candwin_activate(gchar **str);
static void candwin_update(gchar **str);
static void candwin_move(char **str);
static void candwin_show(void);
static void candwin_deactivate(void);
static void candwin_set_nr_candidates(gchar **str);
static void candwin_set_page_candidates(gchar **str);
static void candwin_show_page(gchar **str);
static void str_parse(char *str);
static gchar *init_labelchar_table(void);

static void index_changed_cb(UIMCandidateWindow *cwin)
{
  fprintf(stdout, "index\n");
  fprintf(stdout, "%d\n\n", uim_cand_win_gtk_get_index(cwin));
  fflush(stdout);
}

GType
candidate_window_get_type(void)
{
  if (!candidate_window_type)
    candidate_window_type = g_type_register_static(GTK_TYPE_WINDOW,
		    "UIMCandidateWindow", &object_info, (GTypeFlags)0);
  return candidate_window_type;
}

static void candidate_window_class_init(UIMCandidateWindowClass *klass)
{
  cand_win_gtk_signals[INDEX_CHANGED_SIGNAL]
    = g_signal_new("index-changed",
		   G_TYPE_FROM_CLASS(klass),
		   G_SIGNAL_RUN_FIRST,
		   G_STRUCT_OFFSET(UIMCandidateWindowClass, index_changed),
		   NULL, NULL,
		   g_cclosure_marshal_VOID__VOID,
		   G_TYPE_NONE, 0);
}

UIMCandidateWindow *
candidate_window_new(void)
{
  GObject *obj = g_object_new(UIM_TYPE_CANDIDATE_WINDOW, "type",
		  GTK_WINDOW_POPUP, NULL);
  return UIM_CANDIDATE_WINDOW(obj);
}

/* copied from uim-cand-win-gtk.c */
static void
update_label(UIMCandidateWindow *cwin)
{
  char label_str[20];

  if (cwin->candidate_index >= 0)
    g_snprintf(label_str, sizeof(label_str), "%d / %d",
	       cwin->candidate_index + 1 , cwin->nr_candidates);
  else
    g_snprintf(label_str, sizeof(label_str), "- / %d",
	       cwin->nr_candidates);

  gtk_label_set_text(GTK_LABEL(cwin->num_label), label_str);
}

static void
button_clicked(GtkButton *button, gpointer cwin)
{
}

static void
cb_table_view_destroy(GtkWidget *widget, GPtrArray *stores)
{
  gint i;

  g_return_if_fail(GTK_IS_TABLE(widget));

  for (i = cwin->stores->len - 1; i >= 0; i--) {
    GtkListStore *store = g_ptr_array_remove_index(cwin->stores, i);
    if (store) {
      gtk_list_store_clear(store);
      g_object_unref(G_OBJECT(store));
    }
  }
  g_ptr_array_free(cwin->stores, TRUE);

  g_ptr_array_free(cwin->buttons, TRUE);

  if (cwin->labelchar_table != default_labelchar_table) {
    g_free(cwin->labelchar_table);
  }
}

static void
init_candidate_win(void) {
  cwin = candidate_window_new();
  g_signal_connect(G_OBJECT(cwin), "index-changed",
		   G_CALLBACK(index_changed_cb), NULL);
  g_signal_connect(G_OBJECT(cwin), "configure_event",
		   G_CALLBACK(configure_event_cb), NULL);
}

static void
candidate_window_init(UIMCandidateWindow *cwin)
{
  GdkRectangle cursor_location;
  gint row, col;

  cwin->vbox = gtk_vbox_new(FALSE, 0);
  cwin->frame = gtk_frame_new(NULL);

  cwin->stores = g_ptr_array_new();
  cwin->buttons = g_ptr_array_new();
  cwin->labelchar_table = init_labelchar_table();

  gtk_window_set_default_size(GTK_WINDOW(cwin),
		  CANDWIN_DEFAULT_WIDTH, -1);


  cwin->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cwin->scrolled_window),
				 GTK_POLICY_NEVER,
				 GTK_POLICY_NEVER);
  gtk_box_pack_start(GTK_BOX(cwin->vbox), cwin->scrolled_window, TRUE, TRUE, 0);

  cwin->view = gtk_table_new(LABELCHAR_NR_ROWS, LABELCHAR_NR_COLUMNS, FALSE);
  g_signal_connect(G_OBJECT(cwin->view), "destroy",
  		   G_CALLBACK(cb_table_view_destroy), cwin->stores);
  cwin->viewport = gtk_viewport_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(cwin->viewport), cwin->view);
  gtk_container_add(GTK_CONTAINER(cwin->scrolled_window), cwin->viewport);
  gtk_container_set_resize_mode(GTK_CONTAINER(cwin->viewport), GTK_RESIZE_PARENT);
  for (row = 0; row < LABELCHAR_NR_ROWS; row++) {
    for (col = 0; col < LABELCHAR_NR_COLUMNS; col++) {
      GtkWidget *button;
      button = gtk_button_new_with_label("  ");
      g_signal_connect(button, "clicked", G_CALLBACK(button_clicked), cwin);
      gtk_table_attach_defaults(GTK_TABLE(cwin->view), button,
                                col, col + 1, row, row + 1);
      g_ptr_array_add(cwin->buttons, button);
    }
  }
  gtk_table_set_col_spacing(GTK_TABLE(cwin->view), SPACING_LEFT_BLOCK_COLUMN,
      BLOCK_SPACING);
  gtk_table_set_col_spacing(GTK_TABLE(cwin->view), SPACING_RIGHT_BLOCK_COLUMN,
      BLOCK_SPACING);
  gtk_table_set_row_spacing(GTK_TABLE(cwin->view), SPACING_UP_BLOCK_ROW,
      BLOCK_SPACING);
  gtk_table_set_col_spacing(GTK_TABLE(cwin->view), SPACING_LEFTHAND_FAR_COLUMN,
      HOMEPOSITION_SPACING);
  gtk_table_set_col_spacing(GTK_TABLE(cwin->view), SPACING_RIGHTHAND_FAR_COLUMN,
      HOMEPOSITION_SPACING);
  gtk_table_set_row_spacing(GTK_TABLE(cwin->view), SPACING_UPPER_FAR_ROW,
      HOMEPOSITION_SPACING);
  gtk_table_set_row_spacing(GTK_TABLE(cwin->view), SPACING_SHIFT_UPPER_FAR_ROW,
      HOMEPOSITION_SPACING);

  gtk_container_add(GTK_CONTAINER(cwin->frame), cwin->vbox);
  gtk_container_add(GTK_CONTAINER(cwin), cwin->frame);
  gtk_container_set_border_width(GTK_CONTAINER(cwin->frame), 0);

  cwin->num_label = gtk_label_new("");

  gtk_box_pack_start(GTK_BOX(cwin->vbox), cwin->num_label, FALSE, FALSE, 0);

  cwin->pos_x = 0;
  cwin->pos_y = 0;
  cwin->is_active = FALSE;
  cwin->need_hilite = FALSE;
  cwin->caret_state_indicator = caret_state_indicator_new();

  cursor_location.x = 0;
  cursor_location.y = 0;
  cursor_location.height = 0;
  caret_state_indicator_set_cursor_location(cwin->caret_state_indicator, &cursor_location);
}

static gchar *
init_labelchar_table(void)
{
  gchar *table;
  uim_lisp list;
  size_t len = 0;
  uim_lisp *ary0, *ary;
  guint i;

  list = uim_scm_symbol_value("uim-candwin-prog-layout");
  if (list == NULL || !uim_scm_listp(list)) {
    return default_labelchar_table;
  }
  ary0 = ary = (uim_lisp *)uim_scm_list2array(list, &len, NULL);
  if (ary == NULL || len <= 0) {
    if (ary0) {
      free(ary0);
    }
    return default_labelchar_table;
  }
  table = (gchar *)g_malloc(LABELCHAR_NR_CELLS);
  if (table == NULL) {
    free(ary0);
    return default_labelchar_table;
  }
  for (i = 0; i < LABELCHAR_NR_CELLS; i++, ary++) {
    table[i] = '\0';
    if (i < len) {
      char *str = uim_scm_c_str(*ary);
      if (str) {
        table[i] = *str;
        free(str);
      }
    }
  }
  free(ary0);
  return table;
}

static void
get_row_column(gchar *labelchar_table, const gchar labelchar, gint *row, gint *col)
{
  gint i;
  for (i = 0; i < LABELCHAR_NR_CELLS; i++) {
    if (labelchar_table[i] == labelchar) {
      *row = i / LABELCHAR_NR_COLUMNS;
      *col = i % LABELCHAR_NR_COLUMNS;
      return;
    }
  }
  *row = 0;
  *col = 0;
}

static void
set_candidate(UIMCandidateWindow *cwin, GSList *node, GtkListStore *store)
{
  if (node) {
    GtkTreeIter ti;
    gint row = 0;
    gint col = 0;
    gint i;
    gchar *str = node->data;
    /* "heading label char\acandidate\aannotation" */
    gchar **column = g_strsplit(str, "\a", 3);

    get_row_column(cwin->labelchar_table, column[0][0], &row, &col);
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &ti);
    for (i = 0; i < row; i++) {
      gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &ti);
    }
    gtk_list_store_set(store, &ti, col, column[1], TERMINATOR);

    g_strfreev(column);
    g_free(str);
  } else {
    /* No need to set any data for empty row. */
  }
}

static void
candwin_activate(gchar **str)
{
  gsize rbytes, wbytes;
  gint i, nr_stores = 1;
  guint j = 1;
  gchar *utf8_str;
  const gchar *charset;
  guint display_limit;
  GSList *candidates = NULL;

  if (cwin->stores == NULL)
    cwin->stores = g_ptr_array_new();

  /* remove old data */
  for (i = cwin->stores->len - 1; i >= 0; i--) {
    GtkListStore *store = g_ptr_array_remove_index(cwin->stores, i);
    if (store) {
      gtk_list_store_clear(store);
      g_object_unref(G_OBJECT(store));
    }
  }

  if (!strncmp(str[1], "charset=", 8))
    charset = str[1] + 8;
  else
    charset = "UTF-8";

  if (!strncmp(str[2], "display_limit=", 14)) {
    display_limit = atoi(str[2] + 14);
    i = 3;
  } else {
    display_limit = 0;
    i = 2;
  }

  for ( ; str[i]; i++) {
    if (strcmp(str[i], "") == 0) {
      break;
    }
    utf8_str = g_convert(str[i],
			 -1,
			 "UTF-8",
			 charset,
			 &rbytes, &wbytes, NULL);

    candidates = g_slist_prepend(candidates, utf8_str);
    j++;
  }
  candidates = g_slist_reverse(candidates);

  cwin->candidate_index = -1;
  cwin->nr_candidates = j - 1;
  cwin->display_limit = display_limit;
  cwin->need_hilite = FALSE;

  if (candidates == NULL)
    return;

  /* calculate number of GtkListStores to create */
  if (display_limit) {
    nr_stores = cwin->nr_candidates / display_limit;
    if (cwin->nr_candidates > display_limit * nr_stores)
      nr_stores++;
  }

  /* create GtkListStores, and set candidates */
  for (i = 0; i < nr_stores; i++) {
    GtkListStore *store = gtk_list_store_new(LABELCHAR_NR_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	G_TYPE_STRING);
    GSList *node;

    g_ptr_array_add(cwin->stores, store);

    for (j = 0; j < LABELCHAR_NR_ROWS; j++) {
      GtkTreeIter ti;
      gtk_list_store_append(store, &ti);
    }

    /* set candidates */
    for (j = i * display_limit, node = g_slist_nth(candidates, j);
	 display_limit ? j < display_limit * (i + 1) : j < cwin->nr_candidates;
	 j++, node = g_slist_next(node))
    {
      set_candidate(cwin, node, store);
    }
  }
  g_slist_free(candidates);

  uim_cand_win_gtk_set_page(cwin, 0);
  update_label(cwin);

  uim_cand_win_gtk_show(cwin);
  cwin->is_active = TRUE;
}

static void
candwin_update(gchar **str)
{
  int index, need_hilite;
  sscanf(str[1], "%d", &index);
  sscanf(str[2], "%d", &need_hilite);
  cwin->need_hilite = (need_hilite == 1) ? TRUE : FALSE;

  uim_cand_win_gtk_set_index(cwin, index);
}

static void
candwin_move(char **str)
{
  sscanf(str[1], "%d", &cwin->pos_x);
  sscanf(str[2], "%d", &cwin->pos_y);

  uim_cand_win_gtk_layout();
}

static void
candwin_show(void)
{
  if (cwin->is_active)
    uim_cand_win_gtk_show(cwin);
}

static void
candwin_deactivate(void)
{
  gtk_widget_hide(GTK_WIDGET(cwin));
  cwin->is_active = FALSE;
}

static void
caret_state_show(gchar **str)
{
  int timeout;

  sscanf(str[1], "%d", &timeout);
  caret_state_indicator_update(cwin->caret_state_indicator, cwin->pos_x, cwin->pos_y, str[2]);
  if (timeout != 0)
    caret_state_indicator_set_timeout(cwin->caret_state_indicator, timeout * 1000);
  gtk_widget_show_all(GTK_WIDGET(cwin->caret_state_indicator));
}

static void
caret_state_update()
{
  caret_state_indicator_update(cwin->caret_state_indicator, cwin->pos_x, cwin->pos_y, NULL);
}

static void
caret_state_hide()
{
  gtk_widget_hide(cwin->caret_state_indicator);
}

static void
candwin_set_nr_candidates(gchar **str)
{
  guint nr, display_limit;
  gint i, nr_stores = 1;

  sscanf(str[1], "%ud", &nr);
  sscanf(str[2], "%ud", &display_limit);

  cwin->candidate_index = -1;
  cwin->nr_candidates = nr;
  cwin->display_limit = display_limit;
  cwin->need_hilite = FALSE;
  cwin->is_active = TRUE;

  if (cwin->stores == NULL)
    cwin->stores = g_ptr_array_new();

  /* remove old data */
  for (i = cwin->stores->len - 1; i >= 0; i--) {
    GtkListStore *store = g_ptr_array_remove_index(cwin->stores, i);
    if (store) {
      gtk_list_store_clear(store);
      g_object_unref(G_OBJECT(store));
    }
  }

  /* calculate number of GtkListStores to create */
  if (display_limit) {
    nr_stores = nr / display_limit;
    if (nr > display_limit * nr_stores)
      nr_stores++;
  }

  /* setup dummy array */
  for (i = 0; i < nr_stores; i++)
    g_ptr_array_add(cwin->stores, NULL);
}

static void
candwin_set_page_candidates(gchar **str)
{
  gsize rbytes, wbytes;
  gint i;
  guint j = 1;
  gchar *utf8_str;
  const gchar *charset;
  GSList *candidates = NULL;
  int page;

  if (!strncmp(str[1], "charset=", 8))
    charset = str[1] + 8;
  else
    charset = "UTF-8";

  if (!strncmp(str[2], "page=", 5)) {
    page = atoi(str[2] + 5);
    i = 3;
  } else {
    /* shouldn't happen */
    page = 0;
    i = 2;
  }

  for ( ; str[i]; i++) {
    if (strcmp(str[i], "") == 0) {
      break;
    }
    utf8_str = g_convert(str[i],
			 -1,
			 "UTF-8",
			 charset,
			 &rbytes, &wbytes, NULL);

    candidates = g_slist_prepend(candidates, utf8_str);
    j++;
  }
  candidates = g_slist_reverse(candidates);

  uim_cand_win_gtk_set_page_candidates(cwin, page, candidates);
  g_slist_free(candidates);
}

static void
candwin_show_page(gchar **str)
{
  int page;

  sscanf(str[1], "%d", &page);

  uim_cand_win_gtk_set_page(cwin, page);
  uim_cand_win_gtk_show(cwin);
}

static void str_parse(gchar *str)
{
  gchar **tmp;
  gchar *command;

  tmp = g_strsplit(str, "\f", 0);
  command = tmp[0];

  if (command) {
    if (strcmp("activate", command) == 0) {
      candwin_activate(tmp);
    } else if (strcmp("select", command) == 0) {
      candwin_update(tmp);
    } else if (strcmp("show", command) == 0) {
      candwin_show();
    } else if (strcmp("hide", command) == 0) {
      gtk_widget_hide_all(GTK_WIDGET(cwin));
    } else if (strcmp("move", command) == 0) {
      candwin_move(tmp);
    } else if (strcmp("deactivate", command) == 0) {
      candwin_deactivate();
    } else if (strcmp("show_caret_state", command) == 0) {
      caret_state_show(tmp);
    } else if (strcmp("update_caret_state", command) == 0) {
      caret_state_update();
    } else if (strcmp("hide_caret_state", command) == 0) {
      caret_state_hide();
    } else if (strcmp("set_nr_candidates", command) == 0) {
      candwin_set_nr_candidates(tmp);
    } else if (strcmp("set_page_candidates", command) == 0) {
      candwin_set_page_candidates(tmp);
    } else if (strcmp("show_page", command) == 0) {
      candwin_show_page(tmp);
    }
  }
  g_strfreev(tmp);
}

#define CANDIDATE_BUFFER_SIZE	4096
static gboolean
read_cb(GIOChannel *channel, GIOCondition c, gpointer p)
{
  char buf[CANDIDATE_BUFFER_SIZE];
  char *read_buf = strdup("");
  int i = 0;
  int n;
  gchar **tmp;
  int fd = g_io_channel_unix_get_fd(channel);

  while (uim_helper_fd_readable(fd) > 0) {
    n = read(fd, buf, CANDIDATE_BUFFER_SIZE - 1);
    if (n == 0) {
      close(fd);
      exit(EXIT_FAILURE);
    }
    if (n == -1)
      return TRUE;
    buf[n] = '\0';
    read_buf = realloc(read_buf, strlen(read_buf) + n + 1);
    strcat(read_buf, buf);
  }

  tmp = g_strsplit(read_buf, "\f\f", 0);

  while (tmp[i]) {
    str_parse(tmp[i]);
    i++;
  }
  g_strfreev(tmp);
  free(read_buf);
  return TRUE;
}

int
main(int argc, char *argv[])
{
  GIOChannel *channel;

  /* disable uim context in annotation window */
  setenv("GTK_IM_MODULE", "gtk-im-context-simple", 1);

  gtk_set_locale();
  gtk_init(&argc, &argv);
  if (uim_init() < 0)
    return 0;

  init_candidate_win();

  channel = g_io_channel_unix_new(0);
  read_tag = g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR,
			    read_cb, 0);
  g_io_channel_unref(channel);

  gtk_main();
  uim_quit();

  return 0;
}

/* copied from uim-cand-win-gtk.c */
static gint
uim_cand_win_gtk_get_index(UIMCandidateWindow *cwin)
{
  g_return_val_if_fail(UIM_IS_CANDIDATE_WINDOW(cwin), -1);

  return cwin->candidate_index;
}

/* copied from uim-cand-win-gtk.c */
static void
uim_cand_win_gtk_set_index(UIMCandidateWindow *cwin, gint index)
{
  gint new_page;

  g_return_if_fail(UIM_IS_CANDIDATE_WINDOW(cwin));

  if (index >= (gint) cwin->nr_candidates)
    cwin->candidate_index = 0;
  else
    cwin->candidate_index = index;

  if (cwin->candidate_index >= 0 && cwin->display_limit)
    new_page = cwin->candidate_index / cwin->display_limit;
  else
    new_page = cwin->page_index;

  if (cwin->page_index != new_page)
    uim_cand_win_gtk_set_page(cwin, new_page);

  update_label(cwin);
}

static void
update_table_button(GtkTreeModel *model, GPtrArray *buttons, gchar *labelchar_table)
{
  GtkTreeIter ti;
  gint row, col;
  gboolean hasValue = TRUE;
  gtk_tree_model_get_iter_first(model, &ti);
  for (row = 0; row < LABELCHAR_NR_ROWS; row++) {
    for (col = 0; col < LABELCHAR_NR_COLUMNS; col++) {
      GValue value = {0};
      const gchar *str = NULL;
      GtkButton *button;
      button = g_ptr_array_index(buttons, INDEX(row, col));
      if (hasValue) {
        gtk_tree_model_get_value(model, &ti, col, &value);
        str = g_value_get_string(&value);
      }
      if (str == NULL) {
        str = "  ";
	if (labelchar_table[INDEX(row, col)] == '\0') {
          gtk_button_set_relief(button, GTK_RELIEF_NONE);
	} else {
          gtk_button_set_relief(button, GTK_RELIEF_HALF);
	}
        gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
      } else {
        gtk_button_set_relief(button, GTK_RELIEF_NORMAL);
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
      }
      gtk_button_set_label(button, str);
      if (hasValue) {
        g_value_unset(&value);
      }
    }
    if (hasValue) {
      hasValue = gtk_tree_model_iter_next(model, &ti);
    }
  }
}

/* copied from uim-cand-win-gtk.c */
static void
uim_cand_win_gtk_set_page(UIMCandidateWindow *cwin, gint page)
{
  guint len, new_page;
  gint new_index;

  g_return_if_fail(UIM_IS_CANDIDATE_WINDOW(cwin));
  g_return_if_fail(cwin->stores);

  len = cwin->stores->len;
  g_return_if_fail(len);

  if (page < 0)
    new_page = len - 1;
  else if (page >= (gint) len)
    new_page = 0;
  else
    new_page = page;

  update_table_button(GTK_TREE_MODEL(cwin->stores->pdata[new_page]),
                      cwin->buttons, cwin->labelchar_table);

  cwin->page_index = new_page;

  if (cwin->display_limit) {
    if (cwin->candidate_index >= 0)
      new_index
	= (new_page * cwin->display_limit) + (cwin->candidate_index % cwin->display_limit);
    else
      new_index = -1;
  } else {
    new_index = cwin->candidate_index;
  }

  if (new_index >= (gint) cwin->nr_candidates)
    new_index = cwin->nr_candidates - 1;

 /* shrink the window */
  gtk_window_resize(GTK_WINDOW(cwin), CANDWIN_DEFAULT_WIDTH, 1);

  uim_cand_win_gtk_set_index(cwin, new_index);
}

/* copied from uim-cand-win-gtk.c and adjusted */
static void
uim_cand_win_gtk_set_page_candidates(UIMCandidateWindow *cwin,
				     guint page,
				     GSList *candidates)
{
  GtkListStore *store;
  GSList *node;
  gint j, len;

  g_return_if_fail(UIM_IS_CANDIDATE_WINDOW(cwin));

  if (candidates == NULL)
    return;

  len = g_slist_length(candidates);

  /* create GtkListStores, and set candidates */
  store = gtk_list_store_new(LABELCHAR_NR_COLUMNS, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  cwin->stores->pdata[page] = store;
  for (j = 0; j < LABELCHAR_NR_ROWS; j++) {
    GtkTreeIter ti;
    gtk_list_store_append(store, &ti);
  }
  /* set candidates */
  for (j = 0, node = g_slist_nth(candidates, j);
       j < len;
       j++, node = g_slist_next(node))
  {
    set_candidate(cwin, node, store);
  }
}

static void
uim_cand_win_gtk_layout()
{
  int x, y;
  int screen_width, screen_height;

  screen_width = gdk_screen_get_width(gdk_screen_get_default());
  screen_height = gdk_screen_get_height(gdk_screen_get_default());

  if (screen_width < cwin->pos_x + cwin->width)
    x = cwin->pos_x - cwin->width;
  else
    x = cwin->pos_x;

  if (screen_height < cwin->pos_y + cwin->height)
    y = cwin->pos_y - cwin->height - 20; /* FIXME: Preedit height is needed to
					    be sent by uim-xim */
  else
    y = cwin->pos_y;

  gtk_window_move(GTK_WINDOW(cwin), x, y);
}

static gboolean
is_empty_block(GPtrArray *buttons, gint rowstart, gint rowend, gint colstart, gint colend)
{
  gint row, col;
  for (row = rowstart; row < rowend; row++) {
    for (col = colstart; col < colend; col++) {
      GtkButton *button;
      GtkReliefStyle relief;
      button = g_ptr_array_index(buttons, INDEX(row, col));
      relief = gtk_button_get_relief(button);
      if (relief == GTK_RELIEF_NORMAL) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

static void
show_table(GtkTable *view, GPtrArray *buttons)
{
  /* hide empty blocks.
   * pattern0(full table)
   *   blockLR  blockA
   *   blockLRS blockAS  (for shift key)
   * pattern1(minimal blocks)
   *   blockLR
   * pattern2(without shift blocks)
   *   blockLR  blockA
   * pattern3(without symbol blocks)
   *   blockLR
   *   blockLRS
   */
  gint row, col;
  gint hide_row, hide_col;
  gint row_spacing, col_spacing;
  gboolean blockA, blockAS, blockLRS;
  blockA = !is_empty_block(buttons, BLOCK_A_ROW_START, BLOCK_A_ROW_END,
      BLOCK_A_COLUMN_START, BLOCK_A_COLUMN_END);
  blockAS = !is_empty_block(buttons, BLOCK_AS_ROW_START, BLOCK_AS_ROW_END,
      BLOCK_AS_COLUMN_START, BLOCK_AS_COLUMN_END);
  blockLRS = !is_empty_block(buttons, BLOCK_LRS_ROW_START, BLOCK_LRS_ROW_END,
      BLOCK_LRS_COLUMN_START, BLOCK_LRS_COLUMN_END);

  hide_row = LABELCHAR_NR_ROWS;
  hide_col = LABELCHAR_NR_COLUMNS;
  if (blockAS) { /* pattern0(full table) */
    hide_row = LABELCHAR_NR_ROWS;
    hide_col = LABELCHAR_NR_COLUMNS;
  } else if (blockLRS) {
    if (blockA) { /* pattern0(full table) */
      hide_row = LABELCHAR_NR_ROWS;
      hide_col = LABELCHAR_NR_COLUMNS;
    } else { /* pattern3(without symbol blocks) */
      hide_row = LABELCHAR_NR_ROWS;
      hide_col = BLOCK_A_COLUMN_START;
    }
  } else if (blockA) { /* pattern2(without shift blocks) */
    hide_row = BLOCK_A_ROW_END;
    hide_col = LABELCHAR_NR_COLUMNS;
  } else { /* pattern1(minimal blocks) */
    hide_row = BLOCK_A_ROW_END;
    hide_col = BLOCK_A_COLUMN_START;
  }

  for (row = 0; row < LABELCHAR_NR_ROWS; row++) {
    for (col = 0; col < LABELCHAR_NR_COLUMNS; col++) {
      GtkButton *button;
      button = g_ptr_array_index(buttons, INDEX(row, col));
      if (row >= hide_row || col >= hide_col) {
        gtk_widget_hide(GTK_WIDGET(button));
      } else {
        gtk_widget_show(GTK_WIDGET(button));
      }
    }
  }
  if (hide_col <= BLOCK_A_COLUMN_START) {
    col_spacing = 0;
  } else {
    col_spacing = BLOCK_SPACING;
  }
  if (hide_row <= BLOCK_LRS_ROW_START) {
    row_spacing = 0;
  } else {
    row_spacing = BLOCK_SPACING;
  }
  gtk_table_set_col_spacing(view, SPACING_RIGHT_BLOCK_COLUMN, col_spacing);
  gtk_table_set_row_spacing(view, SPACING_UP_BLOCK_ROW, row_spacing);
  if (row_spacing) {
    gtk_table_set_row_spacing(view, SPACING_SHIFT_UPPER_FAR_ROW,
        HOMEPOSITION_SPACING);
  } else {
    gtk_table_set_row_spacing(view, SPACING_SHIFT_UPPER_FAR_ROW, 0);
  }
  /* gtk_table_resize(view, hide_row, hide_col); */
  gtk_widget_show(GTK_WIDGET(view));
}


static void
uim_cand_win_gtk_show(UIMCandidateWindow *cwin)
{
  show_table(GTK_TABLE(cwin->view), cwin->buttons);
  gtk_widget_show(GTK_WIDGET(cwin->viewport));
  gtk_widget_show(GTK_WIDGET(cwin->scrolled_window));
  gtk_widget_show(GTK_WIDGET(cwin->num_label));
  gtk_widget_show(GTK_WIDGET(cwin->vbox));
  gtk_widget_show(GTK_WIDGET(cwin->frame));
  gtk_widget_show(GTK_WIDGET(cwin));
}

static gboolean
configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
  cwin->width = event->width;
  cwin->height = event->height;

  uim_cand_win_gtk_layout();

  return FALSE;
}