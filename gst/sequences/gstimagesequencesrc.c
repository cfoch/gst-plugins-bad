/* GStreamer
 * Copyright (C) 2014 Cesar Fabian Orccon Chipana
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstimagesequencesrc
 *
 * Reads buffers from a location. The location is or a printf pattern
 * or a playlist of files. Check below to see examples.
 *
 * <refsect2>
 * <title>Example launch line</title>

 * Plays a sequence of all images which matches the given printf location pattern at 24 fps.
 * GstImageSequenceSrc has internally an index value which goes form src->start_index to src->stop_index.
 * 
 *
 * |[
 * gst-launch-1.0 -v imagesequencesrc location="whatever.playlist" framerate="24/1" ! decodebin ! videoconvert ! xvimagesink
 * ]|
 * Let's suposse you've created a playlist file 'whatever.playlist'
 * |[
 * metadata,framerate=(fraction)3/1
 * image,location=/path/to/a.png
 * image,location=/path/to/b.png
 * image,location=/path/to/c.png
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gio/gio.h>
#include <stdlib.h>
#include "gstimagesequencesrc.h"
#include <gst/base/gsttypefindhelper.h>
#include <glib/gstdio.h>

/* prototypes */
static GstFlowReturn gst_imagesequencesrc_create (GstPushSrc * src,
    GstBuffer ** buffer);
static void gst_imagesequencesrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_imagesequence_src_change_state (GstElement *
    element, GstStateChange transition);
static void gst_imagesequencesrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_imagesequencesrc_dispose (GObject * object);
static GstCaps *gst_imagesequencesrc_getcaps (GstBaseSrc * bsrc,
    GstCaps * filter);
static gboolean gst_imagesequencesrc_query (GstBaseSrc * bsrc,
    GstQuery * query);
static void gst_imagesequencesrc_parse_location (GstImageSequenceSrc * src);

/* pad templates */
static GstStaticPadTemplate gst_imagesequencesrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* Utilities */
static void gst_imagesequencesrc_set_duration (GstImageSequenceSrc * src);
static void gst_imagesequencesrc_set_caps (GstImageSequenceSrc * src,
    GstCaps * value);

GST_DEBUG_CATEGORY_STATIC (gst_imagesequencesrc_debug_category);
#define GST_CAT_DEFAULT gst_imagesequencesrc_debug_category

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_START_INDEX,
  PROP_STOP_INDEX,
  PROP_FRAMERATE,
  PROP_LOOP,
  PROP_FILENAMES,
  PROP_URI
};

#define DEFAULT_LOCATION "%05d"
#define DEFAULT_URI "imagesequence://."
#define DEFAULT_FRAMERATE "1/1"
#define DEFAULT_INDEX 0
#define DEFAULT_START 0
#define DEFAULT_STOP 0


static void gst_imagesequencesrc_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define _do_init \
  G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_imagesequencesrc_uri_handler_init); \
  GST_DEBUG_CATEGORY_INIT (gst_imagesequencesrc_debug_category, "imagesequencesrc", 0, "imagesequencesrc element");
#define gst_imagesequencesrc_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE (GstImageSequenceSrc, gst_imagesequencesrc,
    GST_TYPE_PUSH_SRC, _do_init);

static gboolean
is_seekable (GstBaseSrc * bsrc)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (bsrc);

  if ((src->filenames != NULL) && (src->fps_n) && (src->fps_d))
    return TRUE;
  return FALSE;
}

static gboolean
do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstImageSequenceSrc *src;

  src = GST_IMAGESEQUENCESRC (bsrc);

  src->index = src->start_index;
  return TRUE;
}

static void
gst_imagesequencesrc_class_init (GstImageSequenceSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_imagesequencesrc_set_property;
  gobject_class->get_property = gst_imagesequencesrc_get_property;

  /* Setting properties */
  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Pattern to create file names of input files. File names are created "
          "by calling sprintf() with the pattern and the current index.",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LOOP,
      g_param_spec_boolean ("loop", "Loop",
          "Whether to repeat from the beginning when all files have been read.",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FILENAMES,
      g_param_spec_boxed ("filenames-list", "Filenames (path) List",
          "Set a list of filenames directly instead of a location pattern."
          "If you *get* the current list, you will obtain a copy of it.",
          G_TYPE_STRV, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_START_INDEX,
      g_param_spec_int ("start-index", "Start Index",
          "Start value of index. When the end of the loop is reached, "
          "the internal index will be set to the value start-index.", 0,
          INT_MAX, DEFAULT_START, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STOP_INDEX,
      g_param_spec_int ("stop-index", "Stop Index", "Stop value of index.",
          -1, INT_MAX, DEFAULT_STOP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMERATE,
      gst_param_spec_fraction ("framerate", "Framerate",
          "Set the framerate to internal caps.",
          1, 1, G_MAXINT, 1, -1, -1,
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "Set the uri.",
          "The URI of an ImageSequenceSrc is as follow:",
          "imagesequencesrc://{location}"
          DEFAULT_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_imagesequencesrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "ImageSequenceSrc plugin", "Src/File",
      "Creates an image-sequence video stream",
      "Cesar Fabian Orccon Chipana <cfoch.fabian@gmail.com>");

  gstbasesrc_class->get_caps = gst_imagesequencesrc_getcaps;
  gstbasesrc_class->query = gst_imagesequencesrc_query;
  gstbasesrc_class->is_seekable = is_seekable;
  gstbasesrc_class->do_seek = do_seek;

  gstpushsrc_class->create = gst_imagesequencesrc_create;

  gstelement_class->change_state = gst_imagesequence_src_change_state;

  gobject_class->dispose = gst_imagesequencesrc_dispose;
}

static void
gst_imagesequencesrc_set_stop_index (GstImageSequenceSrc * src)
{
  guint max_stop = g_list_length (src->filenames) - 1;

  g_assert (src->stop_index >= 0 && max_stop >= 0);
  g_assert (src->stop_index >= src->index && src->stop_index <= max_stop);
  GST_DEBUG_OBJECT (src, "Set (stop-index) property to (%d)", src->stop_index);
}

static void
gst_imagesequencesrc_set_start_index (GstImageSequenceSrc * src)
{
  guint max_start = g_list_length (src->filenames) - 1;

  g_assert (src->start_index >= 0 && max_start >= 0);
  g_assert (src->start_index <= max_start);

  /* Move index to the start */
  src->index = src->start_index;
  src->iter_list = g_list_nth (src->filenames, src->index);
  GST_DEBUG_OBJECT (src, "Set (start-index) property  to (%d)",
      src->start_index);
}


static GstStateChangeReturn
gst_imagesequence_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (src->location) {
        gst_imagesequencesrc_parse_location (src);
        gst_imagesequencesrc_set_start_index (src);
        gst_imagesequencesrc_set_stop_index (src);
      }
      ret = GST_ELEMENT_CLASS (parent_class)->change_state (element,
          transition);
      break;
    default:
      ret = GST_ELEMENT_CLASS (parent_class)->change_state (element,
          transition);
      break;
  }
  return ret;
}

static void
gst_imagesequencesrc_init (GstImageSequenceSrc * src)
{
  GstBaseSrc *bsrc = GST_BASE_SRC (src);

  gst_base_src_set_format (bsrc, GST_FORMAT_TIME);
  src->duration = GST_CLOCK_TIME_NONE;
  src->location = g_strdup (DEFAULT_LOCATION);
  src->filenames = NULL;
  src->index = 0;
  src->start_index = 0;
  src->stop_index = 0;
  src->fps_n = src->fps_d = -1;
}

static GstCaps *
gst_imagesequencesrc_getcaps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstCaps *caps;
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (bsrc);

  if (src->caps) {
    if (filter)
      caps = gst_caps_intersect_full (filter, src->caps,
          GST_CAPS_INTERSECT_FIRST);
    else
      caps = gst_caps_ref (src->caps);
  } else {
    if (filter)
      caps = gst_caps_ref (filter);
    else
      caps = gst_caps_new_any ();
  }

  GST_DEBUG_OBJECT (src, "Caps: %s", gst_caps_to_string (src->caps));
  GST_DEBUG_OBJECT (src, "Filter caps: %s", gst_caps_to_string (filter));
  GST_DEBUG_OBJECT (src, "Returning caps: %s", gst_caps_to_string (caps));

  return caps;
}

static gboolean
gst_imagesequencesrc_query (GstBaseSrc * bsrc, GstQuery * query)
{
  gboolean ret;
  GstImageSequenceSrc *src;

  src = GST_IMAGESEQUENCESRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      gst_query_parse_duration (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_TIME:
          if ((src->filenames != NULL) && (!src->loop)) {
            gst_query_set_duration (query, format, src->duration);
          } else {
            gst_query_set_duration (query, format, -1);
          }
          ret = TRUE;
          break;
        default:
          ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      }
      break;
    }
    default:
      ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      break;
  }

  return ret;
}

static gchar **
_file_get_lines (GFile * file)
{
  gsize size;
  GRegex *clean_action_str;

  GError *err = NULL;
  gchar *content = NULL, *escaped_content = NULL, **lines = NULL;

  clean_action_str = g_regex_new ("\\\\\n|#.*\n", G_REGEX_CASELESS, 0, NULL);

  /* TODO Handle GCancellable */
  if (!g_file_load_contents (file, NULL, &content, &size, NULL, &err))
    goto failed;

  if (g_strcmp0 (content, "") == 0)
    goto failed;

  escaped_content = g_regex_replace (clean_action_str, content, -1, 0, "", 0,
      NULL);
  g_free (content);

  lines = g_strsplit (escaped_content, "\n", 0);
  g_free (escaped_content);

done:

  return lines;

failed:
  if (err) {
    GST_WARNING ("Failed to load contents: %d %s", err->code, err->message);
    g_error_free (err);
  }

  if (content)
    g_free (content);
  content = NULL;

  if (escaped_content)
    g_free (escaped_content);
  escaped_content = NULL;

  if (lines)
    g_strfreev (lines);
  lines = NULL;

  goto done;
}

static gchar **
_get_lines (const gchar * _file)
{
  GFile *file = NULL;
  gchar **lines = NULL;

  GST_DEBUG ("Trying to load %s", _file);
  if ((file = g_file_new_for_path (_file)) == NULL) {
    GST_WARNING ("%s wrong uri", _file);
    return NULL;
  }

  lines = _file_get_lines (file);

  g_object_unref (file);

  return lines;
}

static GList *
_lines_get_strutures (gchar ** lines)
{
  gint i;
  GList *structures = NULL;

  for (i = 0; lines[i]; i++) {
    GstStructure *structure;

    if (g_strcmp0 (lines[i], "") == 0)
      continue;

    structure = gst_structure_from_string (lines[i], NULL);
    if (structure == NULL) {
      GST_ERROR ("Could not parse action %s", lines[i]);
      goto failed;
    }

    structures = g_list_append (structures, structure);
  }

done:
  if (lines)
    g_strfreev (lines);

  return structures;

failed:
  if (structures)
    g_list_free_full (structures, (GDestroyNotify) gst_structure_free);
  structures = NULL;

  goto done;
}

static gboolean
gst_imagesequencesrc_set_location_from_file (GstImageSequenceSrc * src)
{
  GList *structures, *l;
  gchar **lines;
  gint fps_n, fps_d;
  gchar **filenames;
  gint i;

  lines = _get_lines (src->location);
  if (lines == NULL)
    return FALSE;

  structures = _lines_get_strutures (lines);
  filenames = malloc (sizeof (gchar *));
  i = 0;

  for (l = structures; l != NULL; l = l->next) {
    const gchar *filename;
    gst_structure_get_fraction (l->data, "framerate", &fps_n, &fps_d);
    filename = gst_structure_get_string (l->data, "location");
    if (filename) {
      filenames = realloc (filenames, (i + 2) * sizeof (gchar *));
      filenames[i] = g_strdup (filename);
      i++;
    }
  }
  filenames[i] = NULL;

  g_object_set (src, "filenames-list", filenames,
      "framerate", fps_n, fps_d, NULL);
  g_strfreev (filenames);
  return TRUE;
}

static void
gst_imagesequencesrc_set_location (GstImageSequenceSrc * src,
    const gchar * location)
{
  GST_DEBUG_OBJECT (src, "setting location: %s", location);
  g_free (src->location);
  src->location = g_strdup (location);
}

static gchar *
_get_filename (GstImageSequenceSrc * src, gint i)
{
  gchar *filename;

  GST_DEBUG ("%d", i);
  filename = g_strdup_printf (src->location, i);
  if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
    g_free (filename);
    filename = NULL;
  }
  return filename;
}

static void
gst_imagesequencesrc_parse_location (GstImageSequenceSrc * src)
{
  GST_DEBUG_OBJECT (src, "Parsing filenames according location: %s",
      src->location);
  if (g_strrstr (src->location, "%")) {
    gint i;
    gchar *filename;

    GST_DEBUG_OBJECT (src, "Location is a printf pattern.");
    g_list_free_full (src->filenames, g_free);
    src->filenames = NULL;
    for (i = src->start_index; (filename = _get_filename (src, i)); i++) {
      if ((src->stop_index > 0) && (i > src->stop_index))
        break;
      src->filenames = g_list_append (src->filenames, g_strdup (filename));
      g_free (filename);
    }
    src->stop_index = i - 1;
    if (src->start_index > 0) {
      src->stop_index = src->stop_index - src->start_index;
      src->index = src->start_index = 0;
    }
  } else {
    GST_DEBUG_OBJECT (src, "Location is a playlist filename.");
    gst_imagesequencesrc_set_location_from_file (src);
    if (src->stop_index == 0)
      g_object_set (src, "stop-index", g_list_length (src->filenames) - 1,
          NULL);
  }
}


static void
gst_imagesequencesrc_set_duration (GstImageSequenceSrc * src)
{
  /* Calculating duration */
  src->duration = gst_util_uint64_scale (GST_SECOND *
      (src->stop_index - src->start_index + 1), src->fps_d, src->fps_n);
}

static void
gst_imagesequencesrc_set_caps (GstImageSequenceSrc * src, GstCaps * caps)
{
  GstCaps *new_caps;

  g_assert (caps != NULL);
  new_caps = gst_caps_copy (caps);

  if (src->iter_list) {
    GValue fps = G_VALUE_INIT;
    g_value_init (&fps, GST_TYPE_FRACTION);
    gst_value_set_fraction (&fps, src->fps_n, src->fps_d);
    gst_caps_set_value (new_caps, "framerate", &fps);
  }

  gst_caps_replace (&src->caps, new_caps);
  gst_pad_set_caps (GST_BASE_SRC_PAD (src), new_caps);

  GST_DEBUG_OBJECT (src, "Setting new caps: %s", gst_caps_to_string (new_caps));
}

void
gst_imagesequencesrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (object);

  switch (property_id) {
    case PROP_LOCATION:
      gst_imagesequencesrc_set_location (src, g_value_get_string (value));
      break;
    case PROP_FRAMERATE:
    {
      src->fps_n = gst_value_get_fraction_numerator (value);
      src->fps_d = gst_value_get_fraction_denominator (value);
      GST_DEBUG_OBJECT (src, "Set (framerate) property to (%d/%d)", src->fps_n,
          src->fps_d);
      break;
    }
    case PROP_START_INDEX:
    {
      guint max_start = g_list_length (src->filenames) - 1;

      src->start_index = g_value_get_int (value);

      g_assert (src->start_index >= 0 && max_start >= 0);
      g_assert (src->start_index <= max_start);

      /* Move index to the start */
      src->index = src->start_index;
      src->iter_list = g_list_nth (src->filenames, src->index);
      GST_DEBUG_OBJECT (src, "Set (start-index) property  to (%d)",
          src->start_index);
      break;
    }
    case PROP_STOP_INDEX:
    {
      src->stop_index = g_value_get_int (value);
      break;
    }
    case PROP_LOOP:
      src->loop = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (src, "Set (loop) property to (%d)", src->loop);
      break;
    case PROP_URI:
    {
      gchar *location;
      g_free (src->uri);
      src->uri = g_strdup (g_value_get_string (value));
      location = gst_uri_get_location (src->uri);
      gst_imagesequencesrc_set_location (src, location);
      src->index = src->start_index;
      break;
    }
    case PROP_FILENAMES:
    {
      gchar **filenames;
      guint count;
      filenames = g_value_get_boxed (value);

      GST_DEBUG_OBJECT (src, "Set (filenames) property. First filename: %s\n",
          filenames[0]);

      src->iter_list = NULL;
      src->index = 0;

      g_list_free_full (src->filenames, g_free);
      src->filenames = NULL;

      count = 0;
      while (*filenames != NULL) {
        src->filenames = g_list_append (src->filenames, g_strdup (*filenames));
        filenames++;
        count++;
      }

      if (((src->stop_index == 0) || (src->stop_index > count)) && (count > 0))
        src->stop_index = count - 1;

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_imagesequencesrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (object);

  GST_DEBUG_OBJECT (src, "get_property");

  switch (property_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->location);
      break;
    case PROP_LOOP:
      g_value_set_boolean (value, src->loop);
      break;
    case PROP_FILENAMES:
    {
      gchar **filenames;
      GList *l;
      int size;

      size = 0;
      filenames = (gchar **) malloc ((size + 1) * sizeof (gchar *));

      for (l = src->filenames; l != NULL; l = l->next) {
        size++;
        filenames = (gchar **) realloc (filenames, size * sizeof (gchar *));
        filenames[size - 1] = g_strdup ((gchar *) l->data);
      }
      filenames[size] = NULL;

      g_value_set_static_boxed (value, filenames);
      break;
    }
      break;
    case PROP_START_INDEX:
      g_value_set_int (value, src->start_index);
      break;
    case PROP_STOP_INDEX:
      g_value_set_int (value, src->stop_index);
      break;
    case PROP_URI:
      g_value_set_string (value, src->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_imagesequencesrc_dispose (GObject * object)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (object);

  g_free (src->location);
  src->location = NULL;
  if (src->caps)
    gst_caps_unref (src->caps);
  g_list_free_full (src->filenames, g_free);
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstFlowReturn
gst_imagesequencesrc_create (GstPushSrc * bsrc, GstBuffer ** buffer)
{
  GstImageSequenceSrc *src;
  gsize size;
  gchar *data;
  gchar *filename;
  GstBuffer *buf;
  gboolean ret;
  GError *error = NULL;
  GstClockTime buffer_duration;

  src = GST_IMAGESEQUENCESRC (bsrc);

  if (src->filenames == NULL)
    return GST_FLOW_ERROR;

  if (src->index > src->stop_index) {
    if (src->loop) {
      src->index = src->start_index;
    } else {
      src->index--;
      return GST_FLOW_EOS;
    }
  }
  if (src->index == src->start_index) {
    src->iter_list = g_list_nth (src->filenames, src->index);
  }

  filename = (gchar *) src->iter_list->data;

  ret = g_file_get_contents (filename, &data, &size, &error);
  if (!ret)
    goto handle_error;

  GST_DEBUG_OBJECT (src, "reading from file \"%s\".", filename);

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (0, data, size, 0, size, data, g_free));

  if ((src->index == src->start_index) && (!src->caps)) {
    GstCaps *caps;
    caps = gst_type_find_helper_for_buffer (NULL, buf, NULL);

    gst_imagesequencesrc_set_caps (src, caps);
    gst_imagesequencesrc_set_duration (src);
    gst_caps_unref (caps);
  }

  buffer_duration = gst_util_uint64_scale (GST_SECOND, src->fps_d, src->fps_n);

  GST_BUFFER_PTS (buf) = GST_BUFFER_DTS (buf) =
      (src->index - src->start_index) * buffer_duration;
  GST_BUFFER_OFFSET (buf) = src->offset;
  GST_BUFFER_OFFSET_END (buf) = src->offset + size;
  src->offset += size;

  GST_DEBUG_OBJECT (src, "read file \"%s\".", filename);

  *buffer = buf;

  src->iter_list = src->iter_list->next;
  src->index++;
  return GST_FLOW_OK;

handle_error:
  {
    if (error != NULL) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          ("Error while reading from file \"%s\".", filename),
          ("%s", error->message));
      g_error_free (error);
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          ("Error while reading from file \"%s\".", filename),
          ("%s", g_strerror (errno)));
    }
    g_free (filename);
    return GST_FLOW_ERROR;
  }
}

/**************** GstURIHandlerInterface ******************/

static GstURIType
gst_imagesequencesrc_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_imagesequencesrc_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { GST_IMAGESEQUENCE_URI_PROTOCOL, NULL };

  return protocols;
}

static gchar *
gst_imagesequencesrc_uri_get_uri (GstURIHandler * handler)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (handler);

  return g_strdup (src->uri);
}

static gboolean
gst_imagesequencesrc_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** err)
{
  GstImageSequenceSrc *src = GST_IMAGESEQUENCESRC (handler);

  g_object_set (src, "uri", uri, NULL);
  return TRUE;
}

static void
gst_imagesequencesrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_imagesequencesrc_uri_get_type;
  iface->get_protocols = gst_imagesequencesrc_uri_get_protocols;
  iface->get_uri = gst_imagesequencesrc_uri_get_uri;
  iface->set_uri = gst_imagesequencesrc_uri_set_uri;
}
