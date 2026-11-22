#ifndef phoenix_system_edit_manager_h
#define phoenix_system_edit_manager_h

#include "engine_edit_manager.h"
#include "system_element.h"
#include "system_scene_objects.h"
#include "system_objects.h"

typedef struct
{
    uint32_t vert_to_edge_array[4];
    int face_type;
    float uv[4][2];
    uint8_t obj_vert_selected;
} num_of_faces;

typedef struct
{
    num_of_faces* f;
    uint32_t n;
    uint32_t cap;
} num_of_faces_list;

#define edit_mode_selection_ring_max 36000
typedef struct
{
    uint32_t edge_to_face_array[edit_mode_selection_ring_max];
    uint32_t a[edit_mode_selection_ring_max];
    uint32_t b[edit_mode_selection_ring_max];
    uint32_t face[edit_mode_selection_ring_max];
    uint32_t n;
    bool closed;
    bool clean;
} edit_mode_selection_ring;

bool nl_push(num_of_faces_list* list, const num_of_faces* face);
bool nl_push_existing(num_of_faces_list* list, const edit_mode_mesh* mesh, uint32_t face_index);
bool nl_push_quad(num_of_faces_list* list, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
bool em_rebuild(edit_mode_mesh* mesh, const num_of_faces* faces, uint32_t face_count, enum_mesh_selection basis);
bool edit_weld_with_map(edit_mode_mesh* mesh, const uint32_t* map, uint32_t keep_selected_vert);
edit_mode_mesh* em_registry_get_or_create(system_scene_object* obj);
edit_mode_mesh* em_registry_get(uint32_t object_id);
void em_mesh_free(edit_mode_mesh* mesh);
void em_mesh_clear(edit_mode_mesh* mesh);
void em_make_grid(edit_mode_mesh* mesh, uint32_t nx, uint32_t nz);
void em_registry_remove(uint32_t object_id);
uint32_t em_add_vert(edit_mode_mesh* mesh, editor_vec3 position, const float color[4]);
uint32_t em_add_face(edit_mode_mesh* mesh, const uint32_t* verts, int face_type);
void em_sync_selection(edit_mode_mesh* mesh, enum_mesh_selection basis);
void em_gpu_flush(void);
void em_gpu_tick(void);
bool em_registry_adopt_triangles(uint32_t object_id, const editor_vertex* vertices, uint32_t vertex_count,
                                 const uint32_t* indices, uint32_t index_count);
void em_clear_selection(edit_mode_mesh* mesh);
void em_select_element(edit_mode_mesh* mesh, enum_mesh_selection kind, uint32_t index, bool additive);
void em_set_active_element(edit_mode_mesh* mesh, enum_mesh_selection kind, uint32_t index);
void em_selection_counts(const edit_mode_mesh* mesh, uint32_t* vertex_count, uint32_t* edge_count, uint32_t* face_count);
editor_vec3 edit_face_normal(const edit_mode_mesh* mesh, const edit_mode_face* face);

void edit_msg(const char* format, ...);
void edit_msg_draw(VkCommandBuffer cmd, system_element_context* ui_element);
uint32_t edit_pick_vert(const edit_mode_mesh* mesh, float mouse_x, float mouse_y, uint32_t ignore);
uint32_t edit_pick_edge(const edit_mode_mesh* mesh, float mouse_x, float mouse_y);
const cam_screen* edit_screen_points(void);
void edit_multicut_cancel(system_element_context* ui_element);
void edit_insert_loop_cancel(system_element_context* ui_element);
void edit_line2d(VkCommandBuffer cmd, float ax, float ay, float bx, float by, float width, system_color color);
void edit_box2d(VkCommandBuffer cmd, float x, float y, float radius, system_color color);
bool edit_ring_build(const edit_mode_mesh* mesh, uint32_t edge, edit_mode_selection_ring* ring);
uint32_t edit_ring_face_count(const edit_mode_selection_ring* ring);
bool edit_op_cut_ring(edit_mode_mesh* mesh, const edit_mode_selection_ring* ring, int count, float slide, const char** err);
void edit_target_weld_cancel(system_element_context* ui_element);
void edit_insert_loop_cancel(system_element_context* ui_element);
void edit_multicut_cancel(system_element_context* ui_element);
void edit_target_weld_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_mesh* mesh);
void edit_insert_loop_draw(VkCommandBuffer cmd, const edit_mode_camera_context* camera, const edit_mode_mesh* mesh);
void edit_multicut_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_camera_context* camera, const edit_mode_mesh* mesh);
bool edit_target_weld_toggle(system_element_context* ui_element);
bool edit_insert_loop_toggle(system_element_context* ui_element);
bool edit_multicut_toggle(system_element_context* ui_element);
bool edit_target_weld_dragging(void);
bool edit_insert_loop_dragging(void);
bool edit_multicut_dragging(void);
bool edit_op_extrude(edit_mode_mesh* mesh, const char** err);
bool edit_op_terrain_extrude(edit_mode_mesh* mesh, const char** err);   /* terrain: pull the selected control grid vertices / faces up by the extrude distance */
bool edit_op_bridge(edit_mode_mesh* mesh, const char** err);
bool edit_op_merge_selected(edit_mode_mesh* mesh, const char** err);
bool edit_op_delete_faces(edit_mode_mesh* mesh, const char** err);
bool edit_op_flip_normals(edit_mode_mesh* mesh, const char** err);

#endif
