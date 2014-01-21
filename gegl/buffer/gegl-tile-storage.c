/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006, 2007 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gstdio.h>

#include "gegl.h"
#include "gegl-buffer-types.h"
#include "gegl-tile-storage.h"
#include "gegl-tile.h"
#include "gegl-tile-backend-file.h"
#include "gegl-tile-backend-ram.h"
#include "gegl-tile-backend-tiledir.h"
#include "gegl-tile-handler-empty.h"
#include "gegl-tile-handler-zoom.h"
#include "gegl-tile-handler-cache.h"
#include "gegl-tile-handler-log.h"
#include "gegl-tile-handler-private.h"
#include "gegl-types-internal.h"
#include "gegl-utils.h"
#include "gegl-config.h"
#include "gegl-buffer-private.h"


G_DEFINE_TYPE (GeglTileStorage, gegl_tile_storage, GEGL_TYPE_TILE_HANDLER_CHAIN)

static GObjectClass * parent_class = NULL;

enum
{
  CHANGED,
  LAST_SIGNAL
};

guint gegl_tile_storage_signals[LAST_SIGNAL] = { 0 };

GeglTileBackend * gegl_buffer_backend (GeglBuffer *buffer);

GeglTileStorage *
gegl_tile_storage_new (GeglTileBackend *backend)
{
  GeglTileStorage *tile_storage = g_object_new (GEGL_TYPE_TILE_STORAGE, NULL);
  GeglTileHandlerChain  *tile_handler_chain;
  GeglTileHandler       *handler;
  GeglTileHandler       *cache = NULL;
  GeglTileHandler       *empty = NULL;
  GeglTileHandler       *zoom = NULL;

  tile_storage->seen_zoom = 0;
  g_mutex_init (&tile_storage->mutex);

  tile_handler_chain = GEGL_TILE_HANDLER_CHAIN (tile_storage);
  handler  = GEGL_TILE_HANDLER (tile_storage);

  tile_storage->tile_width  = backend->priv->tile_width;
  tile_storage->tile_height = backend->priv->tile_height;
  tile_storage->px_size     = backend->priv->px_size;
  tile_storage->format      = gegl_tile_backend_get_format (backend);
  tile_storage->tile_size   = gegl_tile_backend_get_tile_size (backend);

  gegl_tile_handler_set_source (handler, (void*)backend);

  { /* should perhaps be a.. method on gegl_tile_handler_chain_set_source
       wrapping handler_set_source() and this*/
    GeglTileHandlerChain *tile_handler_chain2 = GEGL_TILE_HANDLER_CHAIN (handler);
    GSList         *iter   = (void *) tile_handler_chain2->chain;
    while (iter && iter->next)
      iter = iter->next;
    if (iter)
      {
        gegl_tile_handler_set_source (GEGL_TILE_HANDLER (iter->data), handler->source);
      }
  }

  backend = GEGL_TILE_BACKEND (handler->source);

#if 0
  if (g_getenv("GEGL_LOG_TILE_BACKEND")||
      g_getenv("GEGL_TILE_LOG"))
    gegl_tile_handler_chain_add (tile_handler_chain,
                                 g_object_new (GEGL_TYPE_TILE_HANDLER_LOG, NULL));
#endif

  cache = gegl_tile_handler_cache_new ();
  empty = gegl_tile_handler_empty_new (backend);
  zoom = gegl_tile_handler_zoom_new (backend);

  /* set storage and cache on ourself so chain_add() can use it */
  _gegl_tile_handler_set_tile_storage (handler, tile_storage);
  _gegl_tile_handler_set_cache (handler, (GeglTileHandlerCache *) cache);

  gegl_tile_handler_chain_add (tile_handler_chain, cache);
  gegl_tile_handler_chain_add (tile_handler_chain, zoom);
  gegl_tile_handler_chain_add (tile_handler_chain, empty);

  g_object_unref (cache);
  g_object_unref (zoom);
  g_object_unref (empty);

#if 0
  if (g_getenv("GEGL_LOG_TILE_CACHE"))
    gegl_tile_handler_chain_add (tile_handler_chain,
                                 g_object_new (GEGL_TYPE_TILE_HANDLER_LOG, NULL));
#endif

  tile_storage->cache = (GeglTileHandlerCache *) cache;
  ((GeglTileHandlerCache *) cache)->tile_storage = tile_storage;
  gegl_tile_handler_chain_bind (tile_handler_chain);

  ((GeglTileBackend *)gegl_buffer_backend2 ((void*)tile_storage))->priv->storage = (gpointer)
                                              tile_storage;

  return tile_storage;
}

static void
gegl_tile_storage_finalize (GObject *object)
{
  GeglTileStorage *self = GEGL_TILE_STORAGE (object);

  g_mutex_clear (&self->mutex);

  (*G_OBJECT_CLASS (parent_class)->finalize)(object);
}

static void
gegl_tile_storage_class_init (GeglTileStorageClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class                = g_type_class_peek_parent (class);
  gobject_class->finalize     = gegl_tile_storage_finalize;

  gegl_tile_storage_signals[CHANGED] =
        g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  GEGL_TYPE_RECTANGLE);
}

static void
gegl_tile_storage_init (GeglTileStorage *buffer)
{
}
