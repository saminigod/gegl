/* This file is part of GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Copyright 2003 Calvin Williamson
 *           2006 Øyvind Kolås
 */

#include "config.h"

#include <glib-object.h>

#include "gegl-types.h"

#include "gegl-eval-mgr.h"
#include "gegl-eval-visitor.h"
#include "gegl-debug-rect-visitor.h"
#include "gegl-cr-visitor.h"
#include "gegl-have-visitor.h"
#include "gegl-need-visitor.h"
#include "gegl-instrument.h"
#include "gegl-node.h"
#include "gegl-operation.h"
#include "gegl-prepare-visitor.h"
#include "gegl-finish-visitor.h"
#include "gegl-pad.h"
#include "gegl-visitable.h"
#include <stdlib.h>


static void gegl_eval_mgr_class_init (GeglEvalMgrClass *klass);
static void gegl_eval_mgr_init       (GeglEvalMgr      *self);


G_DEFINE_TYPE(GeglEvalMgr, gegl_eval_mgr, GEGL_TYPE_OBJECT)


static void
gegl_eval_mgr_class_init (GeglEvalMgrClass * klass)
{
}

static void
gegl_eval_mgr_init (GeglEvalMgr *self)
{
  GeglRect roi={0,0,-1,-1};
  self->roi = roi;
}

/**
 * gegl_eval_mgr_apply:
 * @self: a #GeglEvalMgr.
 * @root:
 * @property_name:
 *
 * Update this property.
 **/
GeglBuffer *
gegl_eval_mgr_apply (GeglEvalMgr *self,
                     GeglNode    *root,
                     const gchar *pad_name)
{
  GeglBuffer   *buffer;
  GeglVisitor  *prepare_visitor;
  GeglVisitor  *have_visitor;
  GeglVisitor  *need_visitor;
  GeglVisitor  *cr_visitor;
  GeglVisitor  *eval_visitor;
  GeglVisitor  *finish_visitor;
  GeglPad      *pad;
  gint          i;
  glong         time = gegl_ticks ();
  gpointer      dynamic_id = self;

  g_assert (GEGL_IS_EVAL_MGR (self));
  g_assert (GEGL_IS_NODE (root));

  gegl_instrument ("gegl", "process", 0);

  if (pad_name == NULL)
    pad_name = "output";
  pad = gegl_node_get_pad (root, pad_name);

  /* Use the redirect output NOP of a graph instead of a graph if a traversal
   * is attempted directly on a graph */
  if (pad->node != root)
    root = pad->node;
  g_object_ref (root);

  for (i=0;i<2;i++)
    {
      prepare_visitor = g_object_new (GEGL_TYPE_PREPARE_VISITOR, "id", dynamic_id, NULL);
      gegl_visitor_dfs_traverse (prepare_visitor, GEGL_VISITABLE(root));
      g_object_unref (prepare_visitor);
    }

  have_visitor = g_object_new (GEGL_TYPE_HAVE_VISITOR, "id", dynamic_id, NULL);
  gegl_visitor_dfs_traverse (have_visitor, GEGL_VISITABLE(root));
  g_object_unref (have_visitor);

  if (self->roi.w==-1 &&
      self->roi.h==-1)
    {
      GeglRect *root_have_rect = gegl_node_get_have_rect (root);
      g_assert (root_have_rect);
      self->roi = *root_have_rect;
    }

  gegl_node_set_need_rect (root, dynamic_id, self->roi.x, self->roi.y,
                                             self->roi.w, self->roi.h);
  root->is_root = TRUE;

  need_visitor = g_object_new (GEGL_TYPE_NEED_VISITOR, "id", dynamic_id, NULL);
  gegl_visitor_bfs_traverse (need_visitor, GEGL_VISITABLE(root));
  g_object_unref (need_visitor);

  cr_visitor = g_object_new (GEGL_TYPE_CR_VISITOR, "id", dynamic_id, NULL);
  gegl_visitor_bfs_traverse (cr_visitor, GEGL_VISITABLE(root));
  g_object_unref (cr_visitor);

  if(getenv("GEGL_DEBUG_RECTS")!=NULL)
    {
      GeglVisitor  *debug_rect_visitor;

      debug_rect_visitor = g_object_new (GEGL_TYPE_DEBUG_RECT_VISITOR, "id", dynamic_id, NULL);
        gegl_visitor_dfs_traverse (debug_rect_visitor, GEGL_VISITABLE(root));
      g_object_unref (debug_rect_visitor);
    }

  eval_visitor = g_object_new (GEGL_TYPE_EVAL_VISITOR, "id", dynamic_id, NULL);
  gegl_visitor_dfs_traverse (eval_visitor, GEGL_VISITABLE(pad));
  g_object_unref (eval_visitor);

  root->is_root = FALSE;
  if(getenv("GEGL_DEBUG_RECTS")!=NULL)
    {
      GeglVisitor  *debug_rect_visitor;

      g_warning ("---------------------");

      debug_rect_visitor = g_object_new (GEGL_TYPE_DEBUG_RECT_VISITOR, "id", dynamic_id, NULL);
        gegl_visitor_dfs_traverse (debug_rect_visitor, GEGL_VISITABLE(root));
      g_object_unref (debug_rect_visitor);
    }

 {
   /* extract return buffer before running finish visitor */
   GValue value={0,};
   g_value_init (&value, G_TYPE_OBJECT);
   gegl_node_dynamic_get_property (gegl_node_get_dynamic (root, dynamic_id),
      "output", &value);
   buffer = g_value_get_object (&value);
   g_object_ref (buffer); /* salvage buffer from finalization */
   g_value_unset (&value);
 }

  finish_visitor = g_object_new (GEGL_TYPE_FINISH_VISITOR, "id", dynamic_id, NULL);
  gegl_visitor_dfs_traverse (finish_visitor, GEGL_VISITABLE(root));
  g_object_unref (finish_visitor);


  g_object_unref (root);
  time = gegl_ticks () - time;
  gegl_instrument ("gegl", "process", time);

  if (!G_IS_OBJECT (buffer))
    g_warning ("foo %p %i", buffer, ((GObject*)buffer)->ref_count);
  return buffer;
}
