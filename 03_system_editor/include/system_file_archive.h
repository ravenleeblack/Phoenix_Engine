#ifndef system_file_archive_h
#define system_file_archive_h

#include <stdbool.h>
#include <vulkan/vulkan.h>
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
/* file archive: the project's asset folder as a tree of folders and files.   */
/*                                                                           */
/*   model    system_file_archive.c       scans the real folder on disk,      */
/*                                        creates / deletes / selects         */
/*   window   draw_system_file_archive.c  draws it, same look as scene archive */
/*   menus    system_context_menu.c       right click menu for this window    */
/*            draw_system_menubar.c       File > New File / New Folder        */
/*                                                                           */
/*   the root is FILE_ARCHIVE_ROOT_FOLDER next to where the editor runs. it is */
/*   created if it does not exist. nothing outside it is ever touched.        */
/* ========================================================================= */

#define FILE_ARCHIVE_ROOT_FOLDER "assets"

/* extensions for the "new file" entries. change them here and every menu follows. */
#define FILE_ARCHIVE_EXT_TEXT    ".txt"
#define FILE_ARCHIVE_EXT_SCRIPT  ".script"
#define FILE_ARCHIVE_EXT_SCENE   ".scene"

enum {
    file_archive_max_nodes = 2048,
    file_archive_name_max  = 128,
    file_archive_path_max  = 260
};

typedef enum { file_node_folder, file_node_file } enum_file_node_kind;

/* nodes are linked by index (-1 = none), same shape as the scene archive: first_child + next_sibling. */
typedef struct {
    int                 parent;
    int                 first_child;
    int                 next_sibling;
    enum_file_node_kind kind;
    bool                is_open;                       /* folders only */
    char                name[file_archive_name_max];
    char                path[file_archive_path_max];    /* full path on disk */
} file_archive_node;

/* ---- model ---- */
void  file_archive_start(void);                 /* safe to call every frame: only the first call does work */
void  file_archive_refresh(void);               /* rescan the folder. open folders and the selection are kept */

int                      file_archive_node_count(void);
const file_archive_node* file_archive_node_at(int index);      /* NULL when out of range. indexes change on refresh. */
int                      file_archive_selected(void);          /* -1 = nothing obj_vert_selected */
void                     file_archive_select(int index);       /* -1 or out of range = deselect */
void                     file_archive_toggle_open(int index);
void                     file_archive_set_all_open(bool open); /* root always stays open */

/* new things go into the obj_vert_selected folder (or the folder holding the obj_vert_selected file, or the root).
   each returns the new node's index, already obj_vert_selected, or -1 if it failed. names are made unique: "New Folder 2". */
int   file_archive_create_folder(void);
int   file_archive_create_file(const char* base_name, const char* extension);

/* the obj_vert_selected entry goes to the recycle bin, so a wrong click can be undone. the root cannot be deleted. */
void  file_archive_delete_selected(void);
void  file_archive_reveal_selected(void);       /* opens it in Windows Explorer */

/* ---- mouse wheel: forwarded by the window proc. does not consume the message.
        in system_proc():   case WM_MOUSEWHEEL: file_archive_notify_wheel((short)HIWORD(wp)); break;   ---- */
void  file_archive_notify_wheel(int wheel_delta);

/* ---- tree guide lines, shared by the file archive and the scene archive ----
   draws the little lines that tie a row to its parent: a vertical line down the parent's column, a short tick into the row,
   and the continuing lines of every ancestor that still has siblings below.
   ancestor_continues[k] = the ancestor at indent level k has a next sibling. the caller fills it while it recurses. */
enum { tree_indent = 16, tree_guide_levels = 32 };
void draw_tree_guide_lines(VkCommandBuffer cmd, int x, int row_y, int row_h, int level,
                           const unsigned char* ancestor_continues, bool has_next_sibling, bool has_children);

#endif