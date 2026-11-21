#ifndef phoenix_system_undo_h
#define phoenix_system_undo_h

/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

/* ========================================================================= */
/* Edit > Undo / Redo   (Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z)                       */
/*                                                                           */
/* the editor keeps the last state every object transform was settled in.    */
/* when the mouse is up and no number box is active and a transform differs,  */
/* the old state goes on the undo stack. so one gizmo drag, one scrub of a    */
/* number box or one typed value is ONE step.                                 */
/* covered: position, rotation and scale of scene objects (lights included).  */
/* creating or deleting an object starts a new history.                       */
/* ========================================================================= */

#define undo_max_steps 64

typedef struct {
    uint32_t    id;
    editor_vec3 position;
    editor_vec3 rotation;
    editor_vec3 scale;
} undo_object_state;

typedef struct {
    uint32_t           count;
    undo_object_state* objects;
} undo_snapshot;

/* Edit, Sculpt and Paint mode keep their own history: a copy of the whole edit mesh (vertices, edges, faces) of the object being edited.
   one finished drag / stroke / operation is one step. Undo puts the copy back and re-uploads the mesh (a terrain writes its heights back).
   the history is dropped when you leave those modes or edit another object. */
#define undo_edit_max_steps 24
#define undo_edit_max_bytes (192u * 1024u * 1024u)

typedef struct {
    uint32_t        object_id;
    uint32_t        vert_count;
    uint32_t        edge_count;
    uint32_t        face_count;
    edit_mode_vert* verts;
    edit_mode_edge* edges;
    edit_mode_face* faces;
    size_t          bytes;
} edit_undo_snapshot;

/* once per frame, after the mouse state for this frame is final */
void update_system_undo(system_element_context* ui_ctx);

bool perform_system_undo(void);
bool perform_system_redo(void);
bool can_system_undo(void);
bool can_system_redo(void);

#endif
