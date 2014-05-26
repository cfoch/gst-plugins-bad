/* GStreamer
 *
 * unit test for imagesequencesrc
 *
 * Copyright (C) <2009> Fabian Orccon <cfoch.fabian@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <unistd.h>

#include <gst/check/gstcheck.h>

#define PLAYLIST_FILENAME "imagesequence-playlist"
#define BASE_TMP_FILENAME "gst_imagesequencesrc_test_"

struct user_data
{
  GstElement *pipeline;
  int acc;
};

static void
handoff_cb (GstElement * element, GstBuffer * buf, GstPad * pad,
    struct user_data *data)
{
  GstMapInfo info;
  int i;

  gst_memory_map (gst_buffer_get_memory (buf, 0), &info, GST_MAP_READ);

  /* i is the 264th byte that indicates the color of the image test file */
  /* If you open images files with an hexadecimal editor, you will see they */
  /* are following the order of 01, 02, 03 */

  i = info.data[625];
  data->acc++;

  /* We check we are receiving the correct data */
  fail_unless_equals_int (i, data->acc);
}

static gchar *
create_playlist (void)
{
  gint fd;
  int i;
  FILE *f;
  gchar *name_used;
  GError *error = NULL;

  fd = g_file_open_tmp (BASE_TMP_FILENAME "playlist_XXXXXX", &name_used,
      &error);

  if (error == NULL && (!fd))
    return FALSE;

  f = g_fopen (name_used, "w");
  if (f == NULL)
    return FALSE;

  g_fprintf (f, "metadata,framerate=(fraction)24/1\n");
  for (i = 1; i <= 3; i++) {
    gchar *img_filename, *path;

    img_filename = g_strdup_printf ("pixel%d.jpg", i);

    path = g_build_filename (GST_TEST_FILES_PATH, img_filename, NULL);

    g_fprintf (f, "image,location=%s\n", path);

    g_free (path);
    g_free (img_filename);
  }

  fclose (f);

  return g_strdup (name_used);
}

GST_START_TEST (test_parse_playlist)
{
  GstElement *pipeline, *imagesequencesrc, *fakesink;
  GError *error = NULL;
  gchar *location;
  GstBus *bus;
  GstMessage *msg;
  struct user_data *data;

  data = g_malloc (sizeof (struct user_data));

  pipeline = gst_parse_launch
      ("imagesequencesrc name=imgseq ! fakesink signal-handoffs=true", &error);
  fail_unless (pipeline != NULL, "Failed to create pipeline.");

  imagesequencesrc = gst_bin_get_by_name (GST_BIN (pipeline), "imgseq");

  location = create_playlist ();
  fail_unless (location != NULL, "Locations was not set");
  g_object_set (imagesequencesrc, "location", location, NULL);
  g_object_get (imagesequencesrc, "location", &location, NULL);
  fail_unless (location != NULL, "Location was not set.");

  fakesink = gst_bin_get_by_name (GST_BIN (pipeline), "fakesink0");
  fail_unless (fakesink != NULL, "Cannot get fakesink.");

  g_signal_connect (fakesink, "handoff", G_CALLBACK (handoff_cb), data);

  /* Initialize user data */
  data->acc = 0;
  data->pipeline = pipeline;

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_poll (bus, GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      break;
    case GST_MESSAGE_ERROR:
    {
      GError *err = NULL;
      gchar *dbg = NULL;

      gst_message_parse_error (msg, &err, &dbg);
      fail_unless (err != NULL, "GST_MESSAGE_ERROR");
      if (err)
        g_error_free (err);
      if (dbg)
        g_free (dbg);
      break;
    }
    default:
      break;
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_message_unref (msg);
  gst_object_unref (pipeline);
  gst_object_unref (bus);

}

GST_END_TEST;

static Suite *
imagesequencesrc_suite (void)
{
  Suite *s = suite_create ("imagesequencesrc");
  TCase *tc_chain = tcase_create ("imagesequencesrc");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_playlist);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = imagesequencesrc_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
