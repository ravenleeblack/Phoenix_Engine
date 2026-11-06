#include "system_globals.h"

/* ========================================================================= */
/* RING: the strip of quads that one edge_to_face_array crosses  (rows, loop cut)          */
/* Used by Insert Loop and by the double click row select.                   */
/* ========================================================================= */

/* from quad f entered through edge_to_face_array a-b: the corners across (na, nb) and the edge_to_face_array that joins them */
static bool quad_step(const edit_mode_mesh* mesh, uint32_t f, uint32_t a, uint32_t b, uint32_t* na, uint32_t* nb, uint32_t* next_edge)
{
    const edit_mode_face* F = &mesh->faces[f];
    if (F->face_type != 4) return false;
    int ia = -1, ib = -1;
    for (int k = 0; k < 4; ++k) { if (F->vert_to_edge_array[k] == a) ia = k; if (F->vert_to_edge_array[k] == b) ib = k; }
    if (ia < 0 || ib < 0) return false;
    if ((ia + 1) % 4 != ib && (ib + 1) % 4 != ia) return false;
    const int ja = ((ia + 1) % 4 == ib) ? (ia + 3) % 4 : (ia + 1) % 4;
    const int jb = ((ib + 1) % 4 == ia) ? (ib + 3) % 4 : (ib + 1) % 4;
    *na = F->vert_to_edge_array[ja]; *nb = F->vert_to_edge_array[jb];
    for (int k = 0; k < 4; ++k)
        if ((k == ja && (k + 1) % 4 == jb) || (k == jb && (k + 1) % 4 == ja)) { *next_edge = F->edge_to_face_array[k]; return true; }
    return false;
}

bool edit_ring_build(const edit_mode_mesh* mesh, uint32_t e0, edit_mode_selection_ring* R)
{
    static uint32_t be[edit_mode_selection_ring_max], ba[edit_mode_selection_ring_max], bb[edit_mode_selection_ring_max], bf[edit_mode_selection_ring_max];
    static uint32_t fe[edit_mode_selection_ring_max], fa[edit_mode_selection_ring_max], fb[edit_mode_selection_ring_max], ff[edit_mode_selection_ring_max];
    uint32_t bn = 0, fn = 0;
    bool closed = false, clean = true;
    const edit_mode_edge* E0 = &mesh->edges[e0];

    for (int dir = 0; dir < 2 && !closed; ++dir) {
        uint32_t a = E0->vert_to_edge_array[0], b = E0->vert_to_edge_array[1];
        uint32_t f = E0->face[dir];
        while (f != EM_NONE) {
            uint32_t cnt = (dir == 0) ? fn : bn;
            if (cnt >= edit_mode_selection_ring_max - 2) { clean = false; break; }
            uint32_t na, nb, ne;
            if (!quad_step(mesh, f, a, b, &na, &nb, &ne)) { clean = false; break; }
            if (ne == e0) { closed = true; ff[fn] = f; break; }          /* came back round: closing face */
            if (dir == 0) { fe[fn] = ne; fa[fn] = na; fb[fn] = nb; ff[fn] = f; ++fn; }
            else          { be[bn] = ne; ba[bn] = na; bb[bn] = nb; bf[bn] = f; ++bn; }
            a = na; b = nb;
            const edit_mode_edge* E = &mesh->edges[ne];
            f = (E->face[0] == f) ? E->face[1] : E->face[0];             /* EM_NONE on a border edge_to_face_array = clean end */
        }
    }

    uint32_t n = 0, nf = 0;
    for (int j = (int)bn - 1; j >= 0; --j) { R->edge_to_face_array[n] = be[j]; R->a[n] = ba[j]; R->b[n] = bb[j]; ++n; R->face[nf++] = bf[j]; }
    R->edge_to_face_array[n] = e0; R->a[n] = E0->vert_to_edge_array[0]; R->b[n] = E0->vert_to_edge_array[1]; ++n;
    for (uint32_t j = 0; j < fn; ++j) { R->edge_to_face_array[n] = fe[j]; R->a[n] = fa[j]; R->b[n] = fb[j]; ++n; }
    for (uint32_t j = 0; j < fn; ++j) R->face[nf++] = ff[j];
    if (closed) R->face[nf++] = ff[fn];

    R->n = n; R->closed = closed; R->clean = clean && (n >= 2);
    return n >= 2;
}

uint32_t edit_ring_face_count(const edit_mode_selection_ring* R) { return R->closed ? R->n : R->n - 1; }