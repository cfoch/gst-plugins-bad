/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gstgleffects.h"

#define USING_OPENGL(context) (gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL)
#define USING_OPENGL3(context) (gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL3)
#define USING_GLES(context) (gst_gl_context_get_gl_api (context) & GST_GL_API_GLES)
#define USING_GLES2(context) (gst_gl_context_get_gl_api (context) & GST_GL_API_GLES2)
#define USING_GLES3(context) (gst_gl_context_get_gl_api (context) & GST_GL_API_GLES3)

static void
gst_gl_effects_identity_callback (gint width, gint height, guint texture,
    gpointer data)
{
  GstGLEffects *effects = GST_GL_EFFECTS (data);
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLContext *context = filter->context;
  GstGLFuncs *gl = context->gl_vtable;

#if GST_GL_HAVE_OPENGL
  if (USING_OPENGL (context)) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif
#if GST_GL_HAVE_GLES2
  if (USING_GLES2 (context)) {
    GstGLShader *shader =
        g_hash_table_lookup (effects->shaderstable, "identity0");

    if (!shader) {
      shader = gst_gl_shader_new (context);
      g_hash_table_insert (effects->shaderstable, (gchar *) "identity0",
          shader);

      if (shader) {
        GError *error = NULL;
        gst_gl_shader_set_vertex_source (shader, vertex_shader_source);
        gst_gl_shader_set_fragment_source (shader, identity_fragment_source);

        gst_gl_shader_compile (shader, &error);
        if (error) {
          GST_ERROR ("%s", error->message);
          g_error_free (error);
          error = NULL;
          gst_gl_shader_use (NULL);
        } else {
          filter->draw_attr_position_loc =
              gst_gl_shader_get_attribute_location (shader, "a_position");
          filter->draw_attr_texture_loc =
              gst_gl_shader_get_attribute_location (shader, "a_texCoord");
        }
      }
    }
    gst_gl_shader_use (shader);

    gl->ActiveTexture (GL_TEXTURE0);
    gl->Enable (GL_TEXTURE_2D);
    gl->BindTexture (GL_TEXTURE_2D, texture);

    gst_gl_shader_set_uniform_1i (shader, "tex", 0);
  }
#endif

  gst_gl_filter_draw_texture (filter, texture, width, height);
}

void
gst_gl_effects_identity (GstGLEffects * effects)
{
  GstGLFilter *filter = GST_GL_FILTER (effects);

  gst_gl_filter_render_to_target (filter, TRUE, effects->intexture,
      effects->outtexture, gst_gl_effects_identity_callback, effects);
}
