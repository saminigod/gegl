/* This file is part of GEGL editor -- a gtk frontend for GEGL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2003, 2004, 2006 Øyvind Kolås
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include <babl/babl.h>

#include "gegl-types.h"
#include "gegl-utils.h"

#include "graph/gegl-node.h"

#include "gegl-cache.h"
#include "gegl-region.h"


enum
{
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_NODE,
};

enum
{
  INVALIDATED,
  COMPUTED,
  LAST_SIGNAL
};

static void            finalize     (GObject      *object);
static void            dispose      (GObject      *object);
static void            set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void            get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);

G_DEFINE_TYPE (GeglCache, gegl_cache, GEGL_TYPE_BUFFER);

guint gegl_cache_signals[LAST_SIGNAL] = { 0 };

static GObject *
gegl_cache_constructor (GType                  type,
                        guint                  n_params,
                        GObjectConstructParam *params)
{
  GObject   *object;
  GeglCache *self;

  object = G_OBJECT_CLASS (gegl_cache_parent_class)->constructor (type,
                                                                  n_params,
                                                                  params);
  self = GEGL_CACHE (object);

  self->valid_region = gegl_region_new ();
  self->format       = GEGL_BUFFER (self)->format;

  return object;
}


static void
gegl_cache_class_init (GeglCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor  = gegl_cache_constructor;
  gobject_class->finalize     = finalize;
  gobject_class->dispose      = dispose;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

  g_object_class_install_property (gobject_class, PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode",
                                                        "The GeglNode to cache results for",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  /* overriding pspecs for properties in parent class */
  g_object_class_install_property (gobject_class, PROP_X,
                                   g_param_spec_int ("x", "x",
                                                     "local origin's offset relative to source origin",
                                                     G_MININT, G_MAXINT, -4096,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class, PROP_Y,
                                   g_param_spec_int ("y", "y",
                                                     "local origin's offset relative to source origin",
                                                     G_MININT, G_MAXINT, -4096,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
                                   g_param_spec_int ("width", "width",
                                                     "pixel width of buffer",
                                                     -1, G_MAXINT, 10240 * 4,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
                                   g_param_spec_int ("height", "height",
                                                     "pixel height of buffer",
                                                     -1, G_MAXINT, 10240 * 4,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY));


  gegl_cache_signals[COMPUTED] =
    g_signal_new ("computed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  GEGL_TYPE_RECTANGLE);

  gegl_cache_signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  GEGL_TYPE_RECTANGLE);
}

static void
gegl_cache_init (GeglCache *self)
{
  self->node = NULL;

  /* thus providing a default value for GeglCache, that overrides the NULL
   * from GeglBuffer */
  GEGL_BUFFER (self)->format = (gpointer) babl_format ("R'G'B'A u8");
}

static void
dispose (GObject *gobject)
{
  GeglCache *self = GEGL_CACHE (gobject);

  while (g_idle_remove_by_data (gobject)) ;

  if (self->node)
    {
      gint handler = g_signal_handler_find (self->node, G_SIGNAL_MATCH_DATA,
                                            g_signal_lookup ("invalidated",
                                                             GEGL_TYPE_NODE),
                                            0, NULL, NULL, self);
      if (handler)
        {
          g_signal_handler_disconnect (self->node, handler);
        }
      self->node = NULL;
    }

  G_OBJECT_CLASS (gegl_cache_parent_class)->dispose (gobject);
}

static void
finalize (GObject *gobject)
{
  GeglCache *self = GEGL_CACHE (gobject);

  if (self->valid_region)
    gegl_region_destroy (self->valid_region);
  G_OBJECT_CLASS (gegl_cache_parent_class)->finalize (gobject);
}

static void
node_invalidated (GeglNode            *source,
                  const GeglRectangle *rect,
                  gpointer             data)
{
  GeglCache *cache = GEGL_CACHE (data);

  {
    GeglRegion *region;
    region = gegl_region_rectangle (rect);
    gegl_region_subtract (cache->valid_region, region);
    gegl_region_destroy (region);
  }

  g_signal_emit_by_name (cache, "invalidated", rect, NULL);
}

static void
set_property (GObject      *gobject,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GeglCache *self = GEGL_CACHE (gobject);

  switch (property_id)
    {
      case PROP_NODE:
        if (self->node)
          {
            gulong handler;
            handler = g_signal_handler_find (self->node, G_SIGNAL_MATCH_DATA,
                                             g_signal_lookup ("invalidated",
                                                              GEGL_TYPE_NODE),
                                             0, NULL, NULL, self);
            if (handler)
              {
                g_signal_handler_disconnect (self->node, handler);
              }
          }
        /* just getting the node, the cache holds no reference on the node,
         * it is the node that holds reference on the cache
         */
        self->node = GEGL_NODE (g_value_get_object (value));
        g_signal_connect (G_OBJECT (self->node), "invalidated",
                          G_CALLBACK (node_invalidated), self);
        break;

      case PROP_X:
        g_object_set_property (gobject, "GeglBuffer::x", value);
        break;

      case PROP_Y:
        g_object_set_property (gobject, "GeglBuffer::y", value);
        break;

      case PROP_WIDTH:
        g_object_set_property (gobject, "GeglBuffer::width", value);
        break;

      case PROP_HEIGHT:
        g_object_set_property (gobject, "GeglBuffer::height", value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static void
get_property (GObject    *gobject,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GeglCache *self = GEGL_CACHE (gobject);

  switch (property_id)
    {
      case PROP_NODE:
        g_value_set_object (value, self->node);
        break;

        /* For the rest, upchaining to the property implementation in GeglBuffer */
      case PROP_X:
        g_object_get_property (gobject, "GeglBuffer::x", value);
        break;

      case PROP_Y:
        g_object_get_property (gobject, "GeglBuffer::y", value);
        break;

      case PROP_WIDTH:
        g_object_get_property (gobject, "GeglBuffer::width", value);
        break;

      case PROP_HEIGHT:
        g_object_get_property (gobject, "GeglBuffer::height", value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

#if 0
static void
gegl_buffer_clear (GeglBuffer    *buffer,
                   GeglRectangle *rectangle)
{
  gint pixels = rectangle->width * rectangle->height;
  guchar *buf = g_malloc (pixels * 4);
  gint i;

  for (i=0;i<pixels;i++)
    {
      buf[i*4+0]=25;
      buf[i*4+1]=0;
      buf[i*4+2]=25;
      buf[i*4+3]=40;
    }
  gegl_buffer_set (buffer, rectangle, babl_format ("RGBA u8"), buf);
  g_free (buf);
}
#endif

void
gegl_cache_invalidate (GeglCache           *self,
                       const GeglRectangle *roi)
{
#if 1
  if (roi)
    {
      gegl_buffer_clear (GEGL_BUFFER (self), roi);
    }
  else
    {
      g_warning ("XXX: full invalidation of GeglCache NYI\n");
    }
#endif
  if (roi)
    {
      GeglRegion *temp_region;
      temp_region = gegl_region_rectangle (roi);
      gegl_region_subtract (self->valid_region, temp_region);
      gegl_region_destroy (temp_region);
      g_signal_emit (self, gegl_cache_signals[INVALIDATED], 0,
                     roi, NULL);
    }
  else
    {
      GeglRectangle rect = { 0, 0, 0, 0 }; /* should probably be the extent of the cache */
      if (self->valid_region)
        gegl_region_destroy (self->valid_region);
      self->valid_region = gegl_region_new ();
      g_signal_emit (self, gegl_cache_signals[INVALIDATED], 0,
                     &rect, NULL);
    }
}

void
gegl_cache_computed (GeglCache           *self,
                     const GeglRectangle *rect)
{
  g_return_if_fail (GEGL_IS_CACHE (self));
  g_return_if_fail (rect != NULL);

  gegl_region_union_with_rect (self->valid_region, rect);
  g_signal_emit (self, gegl_cache_signals[COMPUTED], 0, rect, NULL);
}
