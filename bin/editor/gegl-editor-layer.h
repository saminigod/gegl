#ifndef __EDITORLAYER_H__
#define __EDITORLAYER_H__

#include	"gegl-node-widget.h"
#include <gegl.h>
#include <glib.h>

/*	
Creates and removes connections between pads in the Gegl graph 
as they are created and removed by the user in the editor widget
Note: only one layer can be safely used per editor widget
The user should not link, unlink, add, or remove any operations outside of the layer interface
*/
typedef struct _GeglEditorLayer GeglEditorLayer;

typedef struct _node_id_pair
{
  GeglNode*	node;
  gint		id;
} node_id_pair;

struct _GeglEditorLayer
{
  GeglEditor	*editor;
  GeglNode	*gegl;
  GSList	*pairs;
};

/* 
Editor and gegl graph should both be empty, but properly initialized 
*/
GeglEditorLayer*	layer_create(GeglEditor* editor, GeglNode* gegl);
void			layer_add_gegl_node(GeglEditorLayer* layer, GeglNode* node);
//void layer_remove_gegl_node(GeglNode* node);
//link, unlink

#endif
