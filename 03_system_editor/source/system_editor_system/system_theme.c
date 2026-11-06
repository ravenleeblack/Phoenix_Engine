#include "system_globals.h"

/* ========================================================================= */
/* color palette. written as 0-255 rgba and stored as floats (0.0 - 1.0).     */
/* declared in system_theme.h.                                                */
/* ========================================================================= */
#define system_rgba(r, g, b, a) { (r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f }

/* Unified System Palette - Neutrals */
system_color system_color_pure_black   = system_rgba(  0,   0,   0, 255);
system_color system_color_flat_black   = system_rgba( 20,  20,  20, 255);

system_color system_color_less_flat_black   = system_rgba( 30,  30,  30, 255);
system_color system_color_charcoal     = system_rgba( 40,  40,  40, 255);

system_color system_color_dark_gray_1  = system_rgba( 50,  50,  50, 255);
system_color system_color_dark_gray_2  = system_rgba( 60,  60,  60, 255);

system_color system_color_mid_gray_1   = system_rgba( 80,  80,  80, 255);
system_color system_color_mid_gray_2   = system_rgba(100, 100, 108, 255);

system_color system_color_light_gray_1 = system_rgba(130, 130, 138, 255);
system_color system_color_light_gray_2 = system_rgba(162, 162, 170, 255);

system_color system_color_black        = system_rgba(  0,   0,   0, 255);
system_color system_color_dark_gray    = system_rgba( 38,  38,  38, 255);
system_color system_color_light_gray   = system_rgba(115, 115, 115, 255);
system_color system_color_white        = system_rgba(255, 255, 255, 255);
system_color system_color_near_white   = system_rgba(245, 245, 247, 255);

/* Unified System Palette - Original Colors Renamed */
system_color system_color_tea_green    = system_rgba(128, 128,   0, 255);
system_color system_color_french_gray  = system_rgba(213, 197, 200, 255);
system_color system_color_orange       = system_rgba(204, 102,   0, 255);
system_color system_color_teal         = system_rgba(  0, 128, 128, 255);
system_color system_color_slate        = system_rgba(113, 143, 148, 255);

/* Unified System Palette - Plums */
system_color system_color_darkest_plum  = system_rgba( 18,  12,  18, 255);
system_color system_color_dark_plum    = system_rgba( 32,  20,  30, 255);
system_color system_color_deep_plum    = system_rgba( 48,  30,  44, 255);
system_color system_color_mid_plum     = system_rgba( 76,  50,  70, 255);
system_color system_color_light_plum   = system_rgba(114,  84, 106, 255);
system_color system_color_med_plum     = system_rgba( 48,  33,  42, 255);
system_color system_color_muted_plum   = system_rgba(148, 118, 140, 255);

/* Unified System Palette - Teals */
system_color system_color_dark_teal    = system_rgba(  0,  72,  72, 255);
system_color system_color_teal_alt     = system_rgba(  0, 128, 128, 255);
system_color system_color_bright_teal  = system_rgba(  0, 185, 182, 255);
system_color system_color_cyan         = system_rgba(  0, 210, 220, 255);

/* Unified System Palette - Blues */
system_color system_color_navy         = system_rgba( 14,  28,  80, 255);
system_color system_color_dark_blue    = system_rgba( 20,  52, 124, 255);
system_color system_color_steel_blue   = system_rgba( 68, 110, 164, 255);
system_color system_color_sky_blue     = system_rgba(100, 160, 214, 255);
system_color system_color_ice_blue     = system_rgba(182, 212, 238, 255);

/* Unified System Palette - Slates */
system_color system_color_dark_slate   = system_rgba( 52,  72,  84, 255);
system_color system_color_slate_alt    = system_rgba( 88, 118, 134, 255);
system_color system_color_light_slate  = system_rgba(132, 160, 172, 255);

/* Unified System Palette - Greens */
system_color system_color_dark_green   = system_rgba(  0,  76,  40, 255);
system_color system_color_forest_green = system_rgba( 28, 110,  58, 255);
system_color system_color_olive        = system_rgba(100, 108,   0, 255);
system_color system_color_lime         = system_rgba( 78, 164,  38, 255);
system_color system_color_mint         = system_rgba(138, 202, 160, 255);

/* Unified System Palette - Oranges and Reds */
system_color system_color_dark_red     = system_rgba(120,  18,  18, 255);
system_color system_color_crimson      = system_rgba(184,  28,  40, 255);
system_color system_color_red          = system_rgba(222,  50,  50, 255);
system_color system_color_dark_orange  = system_rgba( 110,  50,   0, 255);
system_color system_color_orange_alt   = system_rgba(210, 106,   0, 255);
system_color system_color_amber        = system_rgba(224, 164,   0, 255);
system_color system_color_gold         = system_rgba(202, 172,  58, 255);

/* Unified System Palette - Pinks and Roses */
system_color system_color_dark_rose    = system_rgba(124,  38,  58, 255);
system_color system_color_rose         = system_rgba(182,  80, 102, 255);
system_color system_color_pink         = system_rgba(222, 132, 152, 255);
system_color system_color_silver       = system_rgba(196, 196, 200, 255);
system_color system_color_pure_white   = system_rgba(255, 255, 255, 255);





system_color system_color_edit_mode_active_face_hover   = system_rgba(200, 220, 255, 70);    /* pale blue-white, Maya style hover */
system_color system_color_edit_mode_face_selection   = system_rgba(255, 140, 0, 102);






/* ========================================================================= */
/* themed colors. _00 = maya (default), _01 = zbrush, _02 = plum, _03 = graphite. */
/* one block per role, one line per theme. change a number here and that theme follows. */
/* ========================================================================= */
system_color system_color_active_hover_00                    = system_rgba( 90,  90,  90, 255);   /* maya */
system_color system_color_active_hover_01                    = system_rgba( 84,  85,  90, 255);   /* zbrush */
system_color system_color_active_hover_02                    = system_rgba( 96,  64,  88, 255);   /* plum */
system_color system_color_active_hover_03                    = system_rgba(100, 100, 100, 255);   /* graphite */

system_color system_color_close_hover_00                     = system_rgba( 64,  19,  13, 255);   /* maya */
system_color system_color_close_hover_01                     = system_rgba( 92,  28,  20, 255);   /* zbrush */
system_color system_color_close_hover_02                     = system_rgba( 90,  24,  36, 255);   /* plum */
system_color system_color_close_hover_03                     = system_rgba( 96,  30,  20, 255);   /* graphite */

system_color system_color_text_00                            = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_text_01                            = system_rgba(200, 201, 205, 255);   /* zbrush */
system_color system_color_text_02                            = system_rgba(222, 208, 216, 255);   /* plum */
system_color system_color_text_03                            = system_rgba(196, 196, 200, 255);   /* graphite */

system_color system_color_active_text_00                     = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_active_text_01                     = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_active_text_02                     = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_active_text_03                     = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_border_00                          = system_rgba(  0,   0,   0, 255);   /* maya */
system_color system_color_border_01                          = system_rgba( 10,  10,  11, 255);   /* zbrush */
system_color system_color_border_02                          = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_border_03                          = system_rgba(  0,   0,   0, 255);   /* graphite */

system_color system_color_accent_00                          = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_accent_01                          = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_accent_02                          = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_accent_03                          = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_main_menu_bg_00                    = system_rgba( 43,  43,  43, 255);   /* maya */
system_color system_color_main_menu_bg_01                    = system_rgba( 34,  35,  38, 255);   /* zbrush */
system_color system_color_main_menu_bg_02                    = system_rgba( 32,  20,  30, 255);   /* plum */
system_color system_color_main_menu_bg_03                    = system_rgba( 36,  36,  36, 255);   /* graphite */

system_color system_color_main_menu_button_bg_00             = system_rgba( 68,  68,  68, 255);   /* maya */
system_color system_color_main_menu_button_bg_01             = system_rgba( 74,  75,  80, 255);   /* zbrush */
system_color system_color_main_menu_button_bg_02             = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_main_menu_button_bg_03             = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_main_menu_vert_sep_00              = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_main_menu_vert_sep_01              = system_rgba( 12,  12,  14, 255);   /* zbrush */
system_color system_color_main_menu_vert_sep_02              = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_main_menu_vert_sep_03              = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_main_menu_hort_sep_00              = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_main_menu_hort_sep_01              = system_rgba( 12,  12,  14, 255);   /* zbrush */
system_color system_color_main_menu_hort_sep_02              = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_main_menu_hort_sep_03              = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_main_menu_active_text_00           = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_main_menu_active_text_01           = system_rgba(255, 160,  50, 255);   /* zbrush */
system_color system_color_main_menu_active_text_02           = system_rgba(222, 132, 152, 255);   /* plum */
system_color system_color_main_menu_active_text_03           = system_rgba(236, 132,  20, 255);   /* graphite */

system_color system_color_tool_menu_bg_00                    = system_rgba( 54,  54,  54, 255);   /* maya */
system_color system_color_tool_menu_bg_01                    = system_rgba( 44,  45,  49, 255);   /* zbrush */
system_color system_color_tool_menu_bg_02                    = system_rgba( 40,  25,  37, 255);   /* plum */
system_color system_color_tool_menu_bg_03                    = system_rgba( 46,  46,  46, 255);   /* graphite */

system_color system_color_tool_menu_button_bg_00             = system_rgba( 68,  68,  68, 255);   /* maya */
system_color system_color_tool_menu_button_bg_01             = system_rgba( 82,  83,  89, 255);   /* zbrush */
system_color system_color_tool_menu_button_bg_02             = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_tool_menu_button_bg_03             = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_tool_menu_stop_button_bg_00        = system_rgba(120,  30,  30, 255);   /* maya */
system_color system_color_tool_menu_stop_button_bg_01        = system_rgba(130,  40,  30, 255);   /* zbrush */
system_color system_color_tool_menu_stop_button_bg_02        = system_rgba(120,  30,  50, 255);   /* plum */
system_color system_color_tool_menu_stop_button_bg_03        = system_rgba(120,  34,  24, 255);   /* graphite */

system_color system_color_tool_menu_run_button_bg_00         = system_rgba( 68,  68,  68, 255);   /* maya */
system_color system_color_tool_menu_run_button_bg_01         = system_rgba( 82,  83,  89, 255);   /* zbrush */
system_color system_color_tool_menu_run_button_bg_02         = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_tool_menu_run_button_bg_03         = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_tool_menu_vert_sep_00              = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_tool_menu_vert_sep_01              = system_rgba( 12,  12,  14, 255);   /* zbrush */
system_color system_color_tool_menu_vert_sep_02              = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_tool_menu_vert_sep_03              = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_tool_menu_hort_sep_00              = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_tool_menu_hort_sep_01              = system_rgba( 12,  12,  14, 255);   /* zbrush */
system_color system_color_tool_menu_hort_sep_02              = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_tool_menu_hort_sep_03              = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_stop_status_menu_bg_00             = system_rgba( 38,  38,  38, 255);   /* maya */
system_color system_color_stop_status_menu_bg_01             = system_rgba( 28,  29,  32, 255);   /* zbrush */
system_color system_color_stop_status_menu_bg_02             = system_rgba( 24,  15,  23, 255);   /* plum */
system_color system_color_stop_status_menu_bg_03             = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_run_status_menu_bg_00              = system_rgba(110,  50,   0, 255);   /* maya */
system_color system_color_run_status_menu_bg_01              = system_rgba( 92,  44,   8, 255);   /* zbrush */
system_color system_color_run_status_menu_bg_02              = system_rgba( 96,  28,  44, 255);   /* plum */
system_color system_color_run_status_menu_bg_03              = system_rgba(110,  50,   0, 255);   /* graphite */

system_color system_color_stop_status_menu_text_00           = system_rgba(194,  91,   8, 255);   /* maya */
system_color system_color_stop_status_menu_text_01           = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_stop_status_menu_text_02           = system_rgba(222, 132, 152, 255);   /* plum */
system_color system_color_stop_status_menu_text_03           = system_rgba(236, 132,  20, 255);   /* graphite */

system_color system_color_run_status_menu_text_00            = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_run_status_menu_text_01            = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_run_status_menu_text_02            = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_run_status_menu_text_03            = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_mode_menu_bg_00                    = system_rgba( 54,  54,  54, 255);   /* maya */
system_color system_color_mode_menu_bg_01                    = system_rgba( 44,  45,  49, 255);   /* zbrush */
system_color system_color_mode_menu_bg_02                    = system_rgba( 40,  25,  37, 255);   /* plum */
system_color system_color_mode_menu_bg_03                    = system_rgba( 46,  46,  46, 255);   /* graphite */

system_color system_color_mode_menu_button_bg_00             = system_rgba( 68,  68,  68, 255);   /* maya */
system_color system_color_mode_menu_button_bg_01             = system_rgba( 82,  83,  89, 255);   /* zbrush */
system_color system_color_mode_menu_button_bg_02             = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_mode_menu_button_bg_03             = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_dropdown_outline_00                = system_rgba(  0,   0,   0, 255);   /* maya */
system_color system_color_dropdown_outline_01                = system_rgba( 10,  10,  11, 255);   /* zbrush */
system_color system_color_dropdown_outline_02                = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_dropdown_outline_03                = system_rgba(  0,   0,   0, 255);   /* graphite */

system_color system_color_menu_hover_00                      = system_rgba( 40,  40,  40, 255);   /* maya */
system_color system_color_menu_hover_01                      = system_rgba( 52,  53,  58, 255);   /* zbrush */
system_color system_color_menu_hover_02                      = system_rgba( 48,  30,  44, 255);   /* plum */
system_color system_color_menu_hover_03                      = system_rgba( 50,  50,  50, 255);   /* graphite */

system_color system_color_menu_open_00                       = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_menu_open_01                       = system_rgba( 66,  67,  73, 255);   /* zbrush */
system_color system_color_menu_open_02                       = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_menu_open_03                       = system_rgba( 80,  80,  80, 255);   /* graphite */

system_color system_color_menu_item_selected_00              = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_menu_item_selected_01              = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_menu_item_selected_02              = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_menu_item_selected_03              = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_menu_separator_00                  = system_rgba( 80,  80,  80, 255);   /* maya */
system_color system_color_menu_separator_01                  = system_rgba( 70,  71,  76, 255);   /* zbrush */
system_color system_color_menu_separator_02                  = system_rgba(114,  84, 106, 255);   /* plum */
system_color system_color_menu_separator_03                  = system_rgba(100, 100, 108, 255);   /* graphite */

system_color system_color_menu_text_00                       = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_menu_text_01                       = system_rgba(235, 236, 240, 255);   /* zbrush */
system_color system_color_menu_text_02                       = system_rgba(245, 245, 247, 255);   /* plum */
system_color system_color_menu_text_03                       = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_menu_header_text_00                = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_menu_header_text_01                = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_menu_header_text_02                = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_menu_header_text_03                = system_rgba(162, 162, 170, 255);   /* graphite */

system_color system_color_button_text_00                     = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_button_text_01                     = system_rgba(240, 240, 244, 255);   /* zbrush */
system_color system_color_button_text_02                     = system_rgba(245, 245, 247, 255);   /* plum */
system_color system_color_button_text_03                     = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_context_menu_button_00             = system_rgba( 80,  80,  80, 255);   /* maya */
system_color system_color_context_menu_button_01             = system_rgba( 74,  75,  80, 255);   /* zbrush */
system_color system_color_context_menu_button_02             = system_rgba(114,  84, 106, 255);   /* plum */
system_color system_color_context_menu_button_03             = system_rgba(100, 100, 108, 255);   /* graphite */

system_color system_color_context_menu_button_open_00        = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_context_menu_button_open_01        = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_context_menu_button_open_02        = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_context_menu_button_open_03        = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_context_menu_border_00             = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_context_menu_border_01             = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_context_menu_border_02             = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_context_menu_border_03             = system_rgba(162, 162, 170, 255);   /* graphite */

system_color system_color_context_menu_center_00             = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_context_menu_center_01             = system_rgba( 52,  53,  58, 255);   /* zbrush */
system_color system_color_context_menu_center_02             = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_context_menu_center_03             = system_rgba( 80,  80,  80, 255);   /* graphite */

system_color system_color_context_menu_center_ring_00        = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_context_menu_center_ring_01        = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_context_menu_center_ring_02        = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_context_menu_center_ring_03        = system_rgba(162, 162, 170, 255);   /* graphite */

system_color system_color_context_menu_text_00               = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_context_menu_text_01               = system_rgba(240, 240, 244, 255);   /* zbrush */
system_color system_color_context_menu_text_02               = system_rgba(245, 245, 247, 255);   /* plum */
system_color system_color_context_menu_text_03               = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_context_menu_arrow_00              = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_context_menu_arrow_01              = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_context_menu_arrow_02              = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_context_menu_arrow_03              = system_rgba(162, 162, 170, 255);   /* graphite */

system_color system_color_context_menu_dropdown_00           = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_context_menu_dropdown_01           = system_rgba( 44,  45,  49, 255);   /* zbrush */
system_color system_color_context_menu_dropdown_02           = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_context_menu_dropdown_03           = system_rgba( 80,  80,  80, 255);   /* graphite */

system_color system_color_context_menu_item_hover_00         = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_context_menu_item_hover_01         = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_context_menu_item_hover_02         = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_context_menu_item_hover_03         = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_context_menu_item_text_00          = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_context_menu_item_text_01          = system_rgba(240, 240, 244, 255);   /* zbrush */
system_color system_color_context_menu_item_text_02          = system_rgba(245, 245, 247, 255);   /* plum */
system_color system_color_context_menu_item_text_03          = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_dock_frame_00                      = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_dock_frame_01                      = system_rgba( 18,  18,  20, 255);   /* zbrush */
system_color system_color_dock_frame_02                      = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_dock_frame_03                      = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_dock_splitter_00                   = system_rgba(  0,   0,   0, 255);   /* maya */
system_color system_color_dock_splitter_01                   = system_rgba(  8,   8,   9, 255);   /* zbrush */
system_color system_color_dock_splitter_02                   = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_dock_splitter_03                   = system_rgba(  0,   0,   0, 255);   /* graphite */

system_color system_color_tab_element_00                     = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_tab_element_01                     = system_rgba( 24,  25,  27, 255);   /* zbrush */
system_color system_color_tab_element_02                     = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_tab_element_03                     = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_tab_item_00                        = system_rgba( 40,  40,  40, 255);   /* maya */
system_color system_color_tab_item_01                        = system_rgba( 40,  41,  45, 255);   /* zbrush */
system_color system_color_tab_item_02                        = system_rgba( 48,  30,  44, 255);   /* plum */
system_color system_color_tab_item_03                        = system_rgba( 46,  46,  46, 255);   /* graphite */

system_color system_color_tab_item_active_00                 = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_tab_item_active_01                 = system_rgba( 62,  63,  69, 255);   /* zbrush */
system_color system_color_tab_item_active_02                 = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_tab_item_active_03                 = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_tab_item_accent_00                 = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_tab_item_accent_01                 = system_rgba( 62,  63,  69, 255);   /* zbrush */
system_color system_color_tab_item_accent_02                 = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_tab_item_accent_03                 = system_rgba( 70,  70,  70, 255);   /* graphite */

system_color system_color_tab_item_text_00                   = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_tab_item_text_01                   = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_tab_item_text_02                   = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_tab_item_text_03                   = system_rgba(196, 196, 200, 255);   /* graphite */

system_color system_color_tab_item_text_active_00            = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_tab_item_text_active_01            = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_tab_item_text_active_02            = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_tab_item_text_active_03            = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_tab_close_hover_00                 = system_rgba(184,  28,  40, 255);   /* maya */
system_color system_color_tab_close_hover_01                 = system_rgba(200,  60,  40, 255);   /* zbrush */
system_color system_color_tab_close_hover_02                 = system_rgba(184,  28,  40, 255);   /* plum */
system_color system_color_tab_close_hover_03                 = system_rgba(184,  28,  40, 255);   /* graphite */

system_color system_color_tab_close_text_00                  = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_tab_close_text_01                  = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_tab_close_text_02                  = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_tab_close_text_03                  = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_tab_sub_menubar_00                 = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_tab_sub_menubar_01                 = system_rgba( 52,  53,  58, 255);   /* zbrush */
system_color system_color_tab_sub_menubar_02                 = system_rgba( 48,  30,  44, 255);   /* plum */
system_color system_color_tab_sub_menubar_03                 = system_rgba( 60,  60,  60, 255);   /* graphite */

system_color system_color_tab_sub_menubar_divider_00         = system_rgba(  0,   0,   0, 255);   /* maya */
system_color system_color_tab_sub_menubar_divider_01         = system_rgba( 10,  10,  11, 255);   /* zbrush */
system_color system_color_tab_sub_menubar_divider_02         = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_tab_sub_menubar_divider_03         = system_rgba(  0,   0,   0, 255);   /* graphite */

system_color system_color_tab_sub_menubar_button_00          = system_rgba( 80,  80,  80, 255);   /* maya */
system_color system_color_tab_sub_menubar_button_01          = system_rgba( 74,  75,  80, 255);   /* zbrush */
system_color system_color_tab_sub_menubar_button_02          = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_tab_sub_menubar_button_03          = system_rgba( 80,  80,  80, 255);   /* graphite */

system_color system_color_tab_sub_menubar_button_active_00   = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_tab_sub_menubar_button_active_01   = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_tab_sub_menubar_button_active_02   = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_tab_sub_menubar_button_active_03   = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_tab_content_window_bg_00           = system_rgba( 50,  50,  50, 255);   /* maya */
system_color system_color_tab_content_window_bg_01           = system_rgba( 46,  47,  51, 255);   /* zbrush */
system_color system_color_tab_content_window_bg_02           = system_rgba( 44,  30,  41, 255);   /* plum */
system_color system_color_tab_content_window_bg_03           = system_rgba( 50,  50,  50, 255);   /* graphite */

system_color system_color_edit_mode_multicut_percent_bg_00   = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_edit_mode_multicut_percent_bg_01   = system_rgba( 24,  25,  27, 255);   /* zbrush */
system_color system_color_edit_mode_multicut_percent_bg_02   = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_edit_mode_multicut_percent_bg_03   = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_edit_mode_multicut_percent_text_00 = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_edit_mode_multicut_percent_text_01 = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_edit_mode_multicut_percent_text_02 = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_edit_mode_multicut_percent_text_03 = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_scene_manipulator_bg_00            = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_scene_manipulator_bg_01            = system_rgba( 24,  25,  27, 255);   /* zbrush */
system_color system_color_scene_manipulator_bg_02            = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_scene_manipulator_bg_03            = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_scene_manipulator_input_bg_00      = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_scene_manipulator_input_bg_01      = system_rgba( 14,  14,  16, 255);   /* zbrush */
system_color system_color_scene_manipulator_input_bg_02      = system_rgba( 32,  20,  30, 255);   /* plum */
system_color system_color_scene_manipulator_input_bg_03      = system_rgba( 20,  20,  20, 255);   /* graphite */

system_color system_color_scene_manipulator_text_00          = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_scene_manipulator_text_01          = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_scene_manipulator_text_02          = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_scene_manipulator_text_03          = system_rgba(196, 196, 200, 255);   /* graphite */

system_color system_color_scene_manipulator_input_text_00    = system_rgba(255, 255, 255, 255);   /* maya */
system_color system_color_scene_manipulator_input_text_01    = system_rgba(255, 255, 255, 255);   /* zbrush */
system_color system_color_scene_manipulator_input_text_02    = system_rgba(255, 255, 255, 255);   /* plum */
system_color system_color_scene_manipulator_input_text_03    = system_rgba(255, 255, 255, 255);   /* graphite */

system_color system_color_strip_window_body_00               = system_rgba( 20,  20,  20, 255);   /* maya */
system_color system_color_strip_window_body_01               = system_rgba( 24,  25,  27, 255);   /* zbrush */
system_color system_color_strip_window_body_02               = system_rgba( 18,  12,  18, 255);   /* plum */
system_color system_color_strip_window_body_03               = system_rgba( 30,  30,  30, 255);   /* graphite */

system_color system_color_strip_grip_00                      = system_rgba( 60,  60,  60, 255);   /* maya */
system_color system_color_strip_grip_01                      = system_rgba( 52,  53,  58, 255);   /* zbrush */
system_color system_color_strip_grip_02                      = system_rgba( 48,  30,  44, 255);   /* plum */
system_color system_color_strip_grip_03                      = system_rgba( 60,  60,  60, 255);   /* graphite */

system_color system_color_strip_grip_lines_00                = system_rgba(196, 196, 200, 255);   /* maya */
system_color system_color_strip_grip_lines_01                = system_rgba(170, 172, 178, 255);   /* zbrush */
system_color system_color_strip_grip_lines_02                = system_rgba(213, 197, 200, 255);   /* plum */
system_color system_color_strip_grip_lines_03                = system_rgba(162, 162, 170, 255);   /* graphite */

system_color system_color_strip_button_00                    = system_rgba( 80,  80,  80, 255);   /* maya */
system_color system_color_strip_button_01                    = system_rgba( 74,  75,  80, 255);   /* zbrush */
system_color system_color_strip_button_02                    = system_rgba( 76,  50,  70, 255);   /* plum */
system_color system_color_strip_button_03                    = system_rgba( 80,  80,  80, 255);   /* graphite */

system_color system_color_strip_button_selected_00           = system_rgba( 82, 133, 166, 255);   /* maya */
system_color system_color_strip_button_selected_01           = system_rgba(232, 124,  24, 255);   /* zbrush */
system_color system_color_strip_button_selected_02           = system_rgba(182,  80, 102, 255);   /* plum */
system_color system_color_strip_button_selected_03           = system_rgba(204, 102,   0, 255);   /* graphite */

system_color system_color_tab_content_editor_bg_00           = system_rgba( 40,  40,  40, 255);   /* maya */
system_color system_color_tab_content_editor_bg_01           = system_rgba( 36,  37,  40, 255);   /* zbrush */
system_color system_color_tab_content_editor_bg_02           = system_rgba( 32,  20,  30, 255);   /* plum */
system_color system_color_tab_content_editor_bg_03           = system_rgba( 40,  40,  40, 255);   /* graphite */

/* ========================================================================= */
/* the themes. every theme sets EVERY field of system_theme by hand, one line   */
/* per field, so you can see exactly what each part of the editor uses.         */
/* no helper fills anything in for you. if you add a field to phoenix_system_theme */
/* in system_theme.h, add a line for it in every build_xxx() below.             */
/* a field you forget stays {0} = invisible. theme_set() logs a warning for that. */
/* ========================================================================= */
phoenix_system_theme system_theme;
static int s_theme_index = 0;

/* ---------------------------------------------------------- 0: default */
/* ---------------------------------------------------------- 0: maya */
static phoenix_system_theme build_maya(void)
{
    phoenix_system_theme t = {0};

    /* global theme */
    t.system_active_hover                     = system_color_active_hover_00;
    t.system_close_hover                      = system_color_close_hover_00;
    t.system_text                             = system_color_text_00;
    t.system_active_text                      = system_color_active_text_00;
    t.system_border                           = system_color_border_00;
    t.system_accent                           = system_color_accent_00;

    /* bars: menubar, toolbar, statusbar, mode menu */
    t.menubar_background                      = system_color_main_menu_bg_00;
    t.menubar_button                          = system_color_main_menu_button_bg_00;
    t.menu_vert_separator                     = system_color_main_menu_vert_sep_00;
    t.menu_hort_separator                     = system_color_main_menu_hort_sep_00;
    t.menu_active_text                        = system_color_main_menu_active_text_00;
    t.toolbar_background                      = system_color_tool_menu_bg_00;
    t.toolbar_icon_button                     = system_color_tool_menu_button_bg_00;
    t.toolbar_stop_button                     = system_color_tool_menu_stop_button_bg_00;
    t.toolbar_run_button                      = system_color_tool_menu_run_button_bg_00;
    t.tool_menu_vert_separator                = system_color_tool_menu_vert_sep_00;
    t.tool_menu_hort_separator                = system_color_tool_menu_hort_sep_00;
    t.statusbar_stop_background               = system_color_stop_status_menu_bg_00;
    t.statusbar_run_background                = system_color_run_status_menu_bg_00;
    t.statusbar_stop_text                     = system_color_stop_status_menu_text_00;
    t.statusbar_run_text                      = system_color_run_status_menu_text_00;
    t.mode_menu_background                    = system_color_mode_menu_bg_00;
    t.mode_menu_icon_button                   = system_color_mode_menu_button_bg_00;

    /* menus and buttons */
    t.dropdown_outline                        = system_color_dropdown_outline_00;
    t.menu_hover                              = system_color_menu_hover_00;
    t.menu_open                               = system_color_menu_open_00;
    t.menu_item_selected                      = system_color_menu_item_selected_00;
    t.menu_separator                          = system_color_menu_separator_00;
    t.menu_text                               = system_color_menu_text_00;
    t.menu_header_text                        = system_color_menu_header_text_00;
    t.button_text                             = system_color_button_text_00;

    /* right click radial menu */
    t.context_menu_button                     = system_color_context_menu_button_00;
    t.context_menu_button_open                = system_color_context_menu_button_open_00;
    t.context_menu_border                     = system_color_context_menu_border_00;
    t.context_menu_center                     = system_color_context_menu_center_00;
    t.context_menu_center_ring                = system_color_context_menu_center_ring_00;
    t.context_menu_text                       = system_color_context_menu_text_00;
    t.context_menu_arrow                      = system_color_context_menu_arrow_00;
    t.context_menu_dropdown                   = system_color_context_menu_dropdown_00;
    t.context_menu_item_hover                 = system_color_context_menu_item_hover_00;
    t.context_menu_item_text                  = system_color_context_menu_item_text_00;

    /* dock frame */
    t.dock_frame                              = system_color_dock_frame_00;
    t.dock_splitter                           = system_color_dock_splitter_00;

    /* tabbed window */
    t.tab_element                             = system_color_tab_element_00;
    t.tab_item                                = system_color_tab_item_00;
    t.tab_item_active                         = system_color_tab_item_active_00;
    t.tab_item_accent                         = system_color_tab_item_accent_00;
    t.tab_item_text                           = system_color_tab_item_text_00;
    t.tab_item_text_active                    = system_color_tab_item_text_active_00;
    t.tab_close_hover                         = system_color_tab_close_hover_00;
    t.tab_close_text                          = system_color_tab_close_text_00;
    t.tab_sub_menubar                         = system_color_tab_sub_menubar_00;
    t.tab_sub_menubar_divider                 = system_color_tab_sub_menubar_divider_00;
    t.tab_sub_menubar_button                  = system_color_tab_sub_menubar_button_00;
    t.tab_sub_menubar_button_active           = system_color_tab_sub_menubar_button_active_00;
    t.tab_content_window                      = system_color_tab_content_window_bg_00;
    t.tab_content_window_modeler              = system_color_tab_content_editor_bg_00;
    t.tab_content_window_gameview_00          = system_color_tab_content_editor_bg_00;
    t.tab_content_inspector                   = system_color_tab_content_editor_bg_00;
    t.tab_content_window_paint                = system_color_tab_content_editor_bg_00;
    t.tab_content_window_sculpt               = system_color_tab_content_editor_bg_00;
    t.tab_content_window_sceneview            = system_color_tab_content_editor_bg_00;
    t.tab_content_window_fileview             = system_color_tab_content_editor_bg_00;
    t.tab_content_window_textureshop          = system_color_tab_content_editor_bg_00;
    t.tab_content_window_textureshop_editor   = system_color_tab_content_editor_bg_00;
    t.tab_content_window_textureshop_node     = system_color_tab_content_editor_bg_00;
    t.tab_content_window_textureshop_skin     = system_color_tab_content_editor_bg_00;

    /* EDIT MODE MULTICUT PRECENT WINDOW AND TEXT */
    t.edit_mode_multicut_percent_bg           = system_color_edit_mode_multicut_percent_bg_00;
    t.edit_mode_multicut_percent_text         = system_color_edit_mode_multicut_percent_text_00;

    /* Scene Manipulator */
    t.scene_manipulator_background            = system_color_scene_manipulator_bg_00;
    t.scene_manipulator_input_background      = system_color_scene_manipulator_input_bg_00;
    t.scene_manipulator_text                  = system_color_scene_manipulator_text_00;
    t.scene_manipulator_input_text            = system_color_scene_manipulator_input_text_00;

    /* strip window (mode menu) */
    t.strip_window_body                       = system_color_strip_window_body_00;
    t.strip_grip                              = system_color_strip_grip_00;
    t.strip_grip_lines                        = system_color_strip_grip_lines_00;
    t.strip_button                            = system_color_strip_button_00;
    t.strip_button_selected                   = system_color_strip_button_selected_00;

    /* hover / press brightness steps */
    t.system_hover_delta        = 0.06f;
    t.button_hover_delta        = 0.06f;
    t.button_press_delta        = -0.06f;
    t.menu_hover_delta          = 0.06f;
    t.menu_header_hover_delta   = 0.06f;
    t.context_menu_hover_delta  = 0.06f;
    t.tab_hover_delta           = 0.06f;

    /* edit mode (shared by every theme) */
    t.system_color_edit_mode_target_weld                    = system_color_sky_blue;
    t.system_color_edit_mode_normal_color                   = system_color_bright_teal;
    t.system_color_edit_mode_vert_color                     = system_color_light_gray_1;
    t.system_color_edit_mode_vert_active_color              = system_color_pure_white;
    t.system_color_edit_mode_edge_color                     = system_color_mid_gray_2;
    t.system_color_edit_mode_edge_active_color              = system_color_pure_white;
    t.system_color_edit_mode_face_color                     = system_color_light_gray_1;

    t.system_color_edit_mode_face_active_color              = color_with_alpha(system_color_pure_white, 0.35f);
    t.system_color_edit_mode_extrude_face_color             = color_with_alpha(system_color_sky_blue, 0.30f);
    t.system_color_edit_mode_extrude_face_active_color      = color_with_alpha(system_color_ice_blue, 0.50f);

    t.system_color_edit_mode_vert_selection_color           = system_color_red;
    t.system_color_edit_mode_edge_selection_color           = system_color_amber;
    t.system_color_edit_mode_face_selection_color           = system_color_edit_mode_face_selection;
    t.system_color_edit_mode_hover_color                    = system_color_cyan;
    t.system_color_edit_mode_preview_color                  = system_color_amber;
    t.system_color_edit_mode_invalid_color                  = system_color_red;

    t.system_edit_mode_active_face_hover                    = system_color_edit_mode_active_face_hover;

    return t;
}

/* ---------------------------------------------------------- 1: zbrush */
static phoenix_system_theme build_zbrush(void)
{
    phoenix_system_theme t = {0};

    /* global theme */
    t.system_active_hover                     = system_color_active_hover_01;
    t.system_close_hover                      = system_color_close_hover_01;
    t.system_text                             = system_color_text_01;
    t.system_active_text                      = system_color_active_text_01;
    t.system_border                           = system_color_border_01;
    t.system_accent                           = system_color_accent_01;

    /* bars: menubar, toolbar, statusbar, mode menu */
    t.menubar_background                      = system_color_main_menu_bg_01;
    t.menubar_button                          = system_color_main_menu_button_bg_01;
    t.menu_vert_separator                     = system_color_main_menu_vert_sep_01;
    t.menu_hort_separator                     = system_color_main_menu_hort_sep_01;
    t.menu_active_text                        = system_color_main_menu_active_text_01;
    t.toolbar_background                      = system_color_tool_menu_bg_01;
    t.toolbar_icon_button                     = system_color_tool_menu_button_bg_01;
    t.toolbar_stop_button                     = system_color_tool_menu_stop_button_bg_01;
    t.toolbar_run_button                      = system_color_tool_menu_run_button_bg_01;
    t.tool_menu_vert_separator                = system_color_tool_menu_vert_sep_01;
    t.tool_menu_hort_separator                = system_color_tool_menu_hort_sep_01;
    t.statusbar_stop_background               = system_color_stop_status_menu_bg_01;
    t.statusbar_run_background                = system_color_run_status_menu_bg_01;
    t.statusbar_stop_text                     = system_color_stop_status_menu_text_01;
    t.statusbar_run_text                      = system_color_run_status_menu_text_01;
    t.mode_menu_background                    = system_color_mode_menu_bg_01;
    t.mode_menu_icon_button                   = system_color_mode_menu_button_bg_01;

    /* menus and buttons */
    t.dropdown_outline                        = system_color_dropdown_outline_01;
    t.menu_hover                              = system_color_menu_hover_01;
    t.menu_open                               = system_color_menu_open_01;
    t.menu_item_selected                      = system_color_menu_item_selected_01;
    t.menu_separator                          = system_color_menu_separator_01;
    t.menu_text                               = system_color_menu_text_01;
    t.menu_header_text                        = system_color_menu_header_text_01;
    t.button_text                             = system_color_button_text_01;

    /* right click radial menu */
    t.context_menu_button                     = system_color_context_menu_button_01;
    t.context_menu_button_open                = system_color_context_menu_button_open_01;
    t.context_menu_border                     = system_color_context_menu_border_01;
    t.context_menu_center                     = system_color_context_menu_center_01;
    t.context_menu_center_ring                = system_color_context_menu_center_ring_01;
    t.context_menu_text                       = system_color_context_menu_text_01;
    t.context_menu_arrow                      = system_color_context_menu_arrow_01;
    t.context_menu_dropdown                   = system_color_context_menu_dropdown_01;
    t.context_menu_item_hover                 = system_color_context_menu_item_hover_01;
    t.context_menu_item_text                  = system_color_context_menu_item_text_01;

    /* dock frame */
    t.dock_frame                              = system_color_dock_frame_01;
    t.dock_splitter                           = system_color_dock_splitter_01;

    /* tabbed window */
    t.tab_element                             = system_color_tab_element_01;
    t.tab_item                                = system_color_tab_item_01;
    t.tab_item_active                         = system_color_tab_item_active_01;
    t.tab_item_accent                         = system_color_tab_item_accent_01;
    t.tab_item_text                           = system_color_tab_item_text_01;
    t.tab_item_text_active                    = system_color_tab_item_text_active_01;
    t.tab_close_hover                         = system_color_tab_close_hover_01;
    t.tab_close_text                          = system_color_tab_close_text_01;
    t.tab_sub_menubar                         = system_color_tab_sub_menubar_01;
    t.tab_sub_menubar_divider                 = system_color_tab_sub_menubar_divider_01;
    t.tab_sub_menubar_button                  = system_color_tab_sub_menubar_button_01;
    t.tab_sub_menubar_button_active           = system_color_tab_sub_menubar_button_active_01;
    t.tab_content_window                      = system_color_tab_content_window_bg_01;
    t.tab_content_window_modeler              = system_color_tab_content_editor_bg_01;
    t.tab_content_window_gameview_00          = system_color_tab_content_editor_bg_01;
    t.tab_content_inspector                   = system_color_tab_content_editor_bg_01;
    t.tab_content_window_paint                = system_color_tab_content_editor_bg_01;
    t.tab_content_window_sculpt               = system_color_tab_content_editor_bg_01;
    t.tab_content_window_sceneview            = system_color_tab_content_editor_bg_01;
    t.tab_content_window_fileview             = system_color_tab_content_editor_bg_01;
    t.tab_content_window_textureshop          = system_color_tab_content_editor_bg_01;
    t.tab_content_window_textureshop_editor   = system_color_tab_content_editor_bg_01;
    t.tab_content_window_textureshop_node     = system_color_tab_content_editor_bg_01;
    t.tab_content_window_textureshop_skin     = system_color_tab_content_editor_bg_01;

    /* EDIT MODE MULTICUT PRECENT WINDOW AND TEXT */
    t.edit_mode_multicut_percent_bg           = system_color_edit_mode_multicut_percent_bg_01;
    t.edit_mode_multicut_percent_text         = system_color_edit_mode_multicut_percent_text_01;

    /* Scene Manipulator */
    t.scene_manipulator_background            = system_color_scene_manipulator_bg_01;
    t.scene_manipulator_input_background      = system_color_scene_manipulator_input_bg_01;
    t.scene_manipulator_text                  = system_color_scene_manipulator_text_01;
    t.scene_manipulator_input_text            = system_color_scene_manipulator_input_text_01;

    /* strip window (mode menu) */
    t.strip_window_body                       = system_color_strip_window_body_01;
    t.strip_grip                              = system_color_strip_grip_01;
    t.strip_grip_lines                        = system_color_strip_grip_lines_01;
    t.strip_button                            = system_color_strip_button_01;
    t.strip_button_selected                   = system_color_strip_button_selected_01;

    /* hover / press brightness steps */
    t.system_hover_delta        = 0.06f;
    t.button_hover_delta        = 0.06f;
    t.button_press_delta        = -0.06f;
    t.menu_hover_delta          = 0.06f;
    t.menu_header_hover_delta   = 0.06f;
    t.context_menu_hover_delta  = 0.06f;
    t.tab_hover_delta           = 0.06f;

    /* edit mode (shared by every theme) */
    t.system_color_edit_mode_target_weld                    = system_color_sky_blue;
    t.system_color_edit_mode_normal_color                   = system_color_bright_teal;
    t.system_color_edit_mode_vert_color                     = system_color_light_gray_1;
    t.system_color_edit_mode_vert_active_color              = system_color_pure_white;
    t.system_color_edit_mode_edge_color                     = system_color_mid_gray_2;
    t.system_color_edit_mode_edge_active_color              = system_color_pure_white;
    t.system_color_edit_mode_face_color                     = system_color_light_gray_1;

    t.system_color_edit_mode_face_active_color              = color_with_alpha(system_color_pure_white, 0.35f);
    t.system_color_edit_mode_extrude_face_color             = color_with_alpha(system_color_sky_blue, 0.30f);
    t.system_color_edit_mode_extrude_face_active_color      = color_with_alpha(system_color_ice_blue, 0.50f);

    t.system_color_edit_mode_vert_selection_color           = system_color_red;
    t.system_color_edit_mode_edge_selection_color           = system_color_amber;
    t.system_color_edit_mode_face_selection_color           = system_color_edit_mode_face_selection;
    t.system_color_edit_mode_hover_color                    = system_color_cyan;
    t.system_color_edit_mode_preview_color                  = system_color_amber;
    t.system_color_edit_mode_invalid_color                  = system_color_red;

    t.system_edit_mode_active_face_hover                    = system_color_edit_mode_active_face_hover;

    return t;
}

/* ---------------------------------------------------------- 2: plum */
static phoenix_system_theme build_plum(void)
{
    phoenix_system_theme t = {0};

    /* global theme */
    t.system_active_hover                     = system_color_active_hover_02;
    t.system_close_hover                      = system_color_close_hover_02;
    t.system_text                             = system_color_text_02;
    t.system_active_text                      = system_color_active_text_02;
    t.system_border                           = system_color_border_02;
    t.system_accent                           = system_color_accent_02;

    /* bars: menubar, toolbar, statusbar, mode menu */
    t.menubar_background                      = system_color_main_menu_bg_02;
    t.menubar_button                          = system_color_main_menu_button_bg_02;
    t.menu_vert_separator                     = system_color_main_menu_vert_sep_02;
    t.menu_hort_separator                     = system_color_main_menu_hort_sep_02;
    t.menu_active_text                        = system_color_main_menu_active_text_02;
    t.toolbar_background                      = system_color_tool_menu_bg_02;
    t.toolbar_icon_button                     = system_color_tool_menu_button_bg_02;
    t.toolbar_stop_button                     = system_color_tool_menu_stop_button_bg_02;
    t.toolbar_run_button                      = system_color_tool_menu_run_button_bg_02;
    t.tool_menu_vert_separator                = system_color_tool_menu_vert_sep_02;
    t.tool_menu_hort_separator                = system_color_tool_menu_hort_sep_02;
    t.statusbar_stop_background               = system_color_stop_status_menu_bg_02;
    t.statusbar_run_background                = system_color_run_status_menu_bg_02;
    t.statusbar_stop_text                     = system_color_stop_status_menu_text_02;
    t.statusbar_run_text                      = system_color_run_status_menu_text_02;
    t.mode_menu_background                    = system_color_mode_menu_bg_02;
    t.mode_menu_icon_button                   = system_color_mode_menu_button_bg_02;

    /* menus and buttons */
    t.dropdown_outline                        = system_color_dropdown_outline_02;
    t.menu_hover                              = system_color_menu_hover_02;
    t.menu_open                               = system_color_menu_open_02;
    t.menu_item_selected                      = system_color_menu_item_selected_02;
    t.menu_separator                          = system_color_menu_separator_02;
    t.menu_text                               = system_color_menu_text_02;
    t.menu_header_text                        = system_color_menu_header_text_02;
    t.button_text                             = system_color_button_text_02;

    /* right click radial menu */
    t.context_menu_button                     = system_color_context_menu_button_02;
    t.context_menu_button_open                = system_color_context_menu_button_open_02;
    t.context_menu_border                     = system_color_context_menu_border_02;
    t.context_menu_center                     = system_color_context_menu_center_02;
    t.context_menu_center_ring                = system_color_context_menu_center_ring_02;
    t.context_menu_text                       = system_color_context_menu_text_02;
    t.context_menu_arrow                      = system_color_context_menu_arrow_02;
    t.context_menu_dropdown                   = system_color_context_menu_dropdown_02;
    t.context_menu_item_hover                 = system_color_context_menu_item_hover_02;
    t.context_menu_item_text                  = system_color_context_menu_item_text_02;

    /* dock frame */
    t.dock_frame                              = system_color_dock_frame_02;
    t.dock_splitter                           = system_color_dock_splitter_02;

    /* tabbed window */
    t.tab_element                             = system_color_tab_element_02;
    t.tab_item                                = system_color_tab_item_02;
    t.tab_item_active                         = system_color_tab_item_active_02;
    t.tab_item_accent                         = system_color_tab_item_accent_02;
    t.tab_item_text                           = system_color_tab_item_text_02;
    t.tab_item_text_active                    = system_color_tab_item_text_active_02;
    t.tab_close_hover                         = system_color_tab_close_hover_02;
    t.tab_close_text                          = system_color_tab_close_text_02;
    t.tab_sub_menubar                         = system_color_tab_sub_menubar_02;
    t.tab_sub_menubar_divider                 = system_color_tab_sub_menubar_divider_02;
    t.tab_sub_menubar_button                  = system_color_tab_sub_menubar_button_02;
    t.tab_sub_menubar_button_active           = system_color_tab_sub_menubar_button_active_02;
    t.tab_content_window                      = system_color_tab_content_window_bg_02;
    t.tab_content_window_modeler              = system_color_tab_content_editor_bg_02;
    t.tab_content_window_gameview_00          = system_color_tab_content_editor_bg_02;
    t.tab_content_inspector                   = system_color_tab_content_editor_bg_02;
    t.tab_content_window_paint                = system_color_tab_content_editor_bg_02;
    t.tab_content_window_sculpt               = system_color_tab_content_editor_bg_02;
    t.tab_content_window_sceneview            = system_color_tab_content_editor_bg_02;
    t.tab_content_window_fileview             = system_color_tab_content_editor_bg_02;
    t.tab_content_window_textureshop          = system_color_tab_content_editor_bg_02;
    t.tab_content_window_textureshop_editor   = system_color_tab_content_editor_bg_02;
    t.tab_content_window_textureshop_node     = system_color_tab_content_editor_bg_02;
    t.tab_content_window_textureshop_skin     = system_color_tab_content_editor_bg_02;

    /* EDIT MODE MULTICUT PRECENT WINDOW AND TEXT */
    t.edit_mode_multicut_percent_bg           = system_color_edit_mode_multicut_percent_bg_02;
    t.edit_mode_multicut_percent_text         = system_color_edit_mode_multicut_percent_text_02;

    /* Scene Manipulator */
    t.scene_manipulator_background            = system_color_scene_manipulator_bg_02;
    t.scene_manipulator_input_background      = system_color_scene_manipulator_input_bg_02;
    t.scene_manipulator_text                  = system_color_scene_manipulator_text_02;
    t.scene_manipulator_input_text            = system_color_scene_manipulator_input_text_02;

    /* strip window (mode menu) */
    t.strip_window_body                       = system_color_strip_window_body_02;
    t.strip_grip                              = system_color_strip_grip_02;
    t.strip_grip_lines                        = system_color_strip_grip_lines_02;
    t.strip_button                            = system_color_strip_button_02;
    t.strip_button_selected                   = system_color_strip_button_selected_02;

    /* hover / press brightness steps */
    t.system_hover_delta        = 0.06f;
    t.button_hover_delta        = 0.06f;
    t.button_press_delta        = -0.06f;
    t.menu_hover_delta          = 0.06f;
    t.menu_header_hover_delta   = 0.06f;
    t.context_menu_hover_delta  = 0.06f;
    t.tab_hover_delta           = 0.06f;

    /* edit mode (shared by every theme) */
    t.system_color_edit_mode_target_weld                    = system_color_sky_blue;
    t.system_color_edit_mode_normal_color                   = system_color_bright_teal;
    t.system_color_edit_mode_vert_color                     = system_color_light_gray_1;
    t.system_color_edit_mode_vert_active_color              = system_color_pure_white;
    t.system_color_edit_mode_edge_color                     = system_color_mid_gray_2;
    t.system_color_edit_mode_edge_active_color              = system_color_pure_white;
    t.system_color_edit_mode_face_color                     = system_color_light_gray_1;

    t.system_color_edit_mode_face_active_color              = color_with_alpha(system_color_pure_white, 0.35f);
    t.system_color_edit_mode_extrude_face_color             = color_with_alpha(system_color_sky_blue, 0.30f);
    t.system_color_edit_mode_extrude_face_active_color      = color_with_alpha(system_color_ice_blue, 0.50f);

    t.system_color_edit_mode_vert_selection_color           = system_color_red;
    t.system_color_edit_mode_edge_selection_color           = system_color_amber;
    t.system_color_edit_mode_face_selection_color           = system_color_edit_mode_face_selection;
    t.system_color_edit_mode_hover_color                    = system_color_cyan;
    t.system_color_edit_mode_preview_color                  = system_color_amber;
    t.system_color_edit_mode_invalid_color                  = system_color_red;

    t.system_edit_mode_active_face_hover                    = system_color_edit_mode_active_face_hover;

    return t;
}

/* ---------------------------------------------------------- 3: graphite */
static phoenix_system_theme build_graphite(void)
{
    phoenix_system_theme t = {0};

    /* global theme */
    t.system_active_hover                     = system_color_active_hover_03;
    t.system_close_hover                      = system_color_close_hover_03;
    t.system_text                             = system_color_text_03;
    t.system_active_text                      = system_color_active_text_03;
    t.system_border                           = system_color_border_03;
    t.system_accent                           = system_color_accent_03;

    /* bars: menubar, toolbar, statusbar, mode menu */
    t.menubar_background                      = system_color_main_menu_bg_03;
    t.menubar_button                          = system_color_main_menu_button_bg_03;
    t.menu_vert_separator                     = system_color_main_menu_vert_sep_03;
    t.menu_hort_separator                     = system_color_main_menu_hort_sep_03;
    t.menu_active_text                        = system_color_main_menu_active_text_03;
    t.toolbar_background                      = system_color_tool_menu_bg_03;
    t.toolbar_icon_button                     = system_color_tool_menu_button_bg_03;
    t.toolbar_stop_button                     = system_color_tool_menu_stop_button_bg_03;
    t.toolbar_run_button                      = system_color_tool_menu_run_button_bg_03;
    t.tool_menu_vert_separator                = system_color_tool_menu_vert_sep_03;
    t.tool_menu_hort_separator                = system_color_tool_menu_hort_sep_03;
    t.statusbar_stop_background               = system_color_stop_status_menu_bg_03;
    t.statusbar_run_background                = system_color_run_status_menu_bg_03;
    t.statusbar_stop_text                     = system_color_stop_status_menu_text_03;
    t.statusbar_run_text                      = system_color_run_status_menu_text_03;
    t.mode_menu_background                    = system_color_mode_menu_bg_03;
    t.mode_menu_icon_button                   = system_color_mode_menu_button_bg_03;

    /* menus and buttons */
    t.dropdown_outline                        = system_color_dropdown_outline_03;
    t.menu_hover                              = system_color_menu_hover_03;
    t.menu_open                               = system_color_menu_open_03;
    t.menu_item_selected                      = system_color_menu_item_selected_03;
    t.menu_separator                          = system_color_menu_separator_03;
    t.menu_text                               = system_color_menu_text_03;
    t.menu_header_text                        = system_color_menu_header_text_03;
    t.button_text                             = system_color_button_text_03;

    /* right click radial menu */
    t.context_menu_button                     = system_color_context_menu_button_03;
    t.context_menu_button_open                = system_color_context_menu_button_open_03;
    t.context_menu_border                     = system_color_context_menu_border_03;
    t.context_menu_center                     = system_color_context_menu_center_03;
    t.context_menu_center_ring                = system_color_context_menu_center_ring_03;
    t.context_menu_text                       = system_color_context_menu_text_03;
    t.context_menu_arrow                      = system_color_context_menu_arrow_03;
    t.context_menu_dropdown                   = system_color_context_menu_dropdown_03;
    t.context_menu_item_hover                 = system_color_context_menu_item_hover_03;
    t.context_menu_item_text                  = system_color_context_menu_item_text_03;

    /* dock frame */
    t.dock_frame                              = system_color_dock_frame_03;
    t.dock_splitter                           = system_color_dock_splitter_03;

    /* tabbed window */
    t.tab_element                             = system_color_tab_element_03;
    t.tab_item                                = system_color_tab_item_03;
    t.tab_item_active                         = system_color_tab_item_active_03;
    t.tab_item_accent                         = system_color_tab_item_accent_03;
    t.tab_item_text                           = system_color_tab_item_text_03;
    t.tab_item_text_active                    = system_color_tab_item_text_active_03;
    t.tab_close_hover                         = system_color_tab_close_hover_03;
    t.tab_close_text                          = system_color_tab_close_text_03;
    t.tab_sub_menubar                         = system_color_tab_sub_menubar_03;
    t.tab_sub_menubar_divider                 = system_color_tab_sub_menubar_divider_03;
    t.tab_sub_menubar_button                  = system_color_tab_sub_menubar_button_03;
    t.tab_sub_menubar_button_active           = system_color_tab_sub_menubar_button_active_03;
    t.tab_content_window                      = system_color_tab_content_window_bg_03;
    t.tab_content_window_modeler              = system_color_tab_content_editor_bg_03;
    t.tab_content_window_gameview_00          = system_color_tab_content_editor_bg_03;
    t.tab_content_inspector                   = system_color_tab_content_editor_bg_03;
    t.tab_content_window_paint                = system_color_tab_content_editor_bg_03;
    t.tab_content_window_sculpt               = system_color_tab_content_editor_bg_03;
    t.tab_content_window_sceneview            = system_color_tab_content_editor_bg_03;
    t.tab_content_window_fileview             = system_color_tab_content_editor_bg_03;
    t.tab_content_window_textureshop          = system_color_tab_content_editor_bg_03;
    t.tab_content_window_textureshop_editor   = system_color_tab_content_editor_bg_03;
    t.tab_content_window_textureshop_node     = system_color_tab_content_editor_bg_03;
    t.tab_content_window_textureshop_skin     = system_color_tab_content_editor_bg_03;

    /* EDIT MODE MULTICUT PRECENT WINDOW AND TEXT */
    t.edit_mode_multicut_percent_bg           = system_color_edit_mode_multicut_percent_bg_03;
    t.edit_mode_multicut_percent_text         = system_color_edit_mode_multicut_percent_text_03;

    /* Scene Manipulator */
    t.scene_manipulator_background            = system_color_scene_manipulator_bg_03;
    t.scene_manipulator_input_background      = system_color_scene_manipulator_input_bg_03;
    t.scene_manipulator_text                  = system_color_scene_manipulator_text_03;
    t.scene_manipulator_input_text            = system_color_scene_manipulator_input_text_03;

    /* strip window (mode menu) */
    t.strip_window_body                       = system_color_strip_window_body_03;
    t.strip_grip                              = system_color_strip_grip_03;
    t.strip_grip_lines                        = system_color_strip_grip_lines_03;
    t.strip_button                            = system_color_strip_button_03;
    t.strip_button_selected                   = system_color_strip_button_selected_03;

    /* hover / press brightness steps */
    t.system_hover_delta        = 0.06f;
    t.button_hover_delta        = 0.06f;
    t.button_press_delta        = -0.06f;
    t.menu_hover_delta          = 0.06f;
    t.menu_header_hover_delta   = 0.06f;
    t.context_menu_hover_delta  = 0.06f;
    t.tab_hover_delta           = 0.06f;

    /* edit mode (shared by every theme) */
    t.system_color_edit_mode_target_weld                    = system_color_sky_blue;
    t.system_color_edit_mode_normal_color                   = system_color_bright_teal;
    t.system_color_edit_mode_vert_color                     = system_color_light_gray_1;
    t.system_color_edit_mode_vert_active_color              = system_color_pure_white;
    t.system_color_edit_mode_edge_color                     = system_color_mid_gray_2;
    t.system_color_edit_mode_edge_active_color              = system_color_pure_white;
    t.system_color_edit_mode_face_color                     = system_color_light_gray_1;

    t.system_color_edit_mode_face_active_color              = color_with_alpha(system_color_pure_white, 0.35f);
    t.system_color_edit_mode_extrude_face_color             = color_with_alpha(system_color_sky_blue, 0.30f);
    t.system_color_edit_mode_extrude_face_active_color      = color_with_alpha(system_color_ice_blue, 0.50f);

    t.system_color_edit_mode_vert_selection_color           = system_color_red;
    t.system_color_edit_mode_edge_selection_color           = system_color_amber;
    t.system_color_edit_mode_face_selection_color           = system_color_edit_mode_face_selection;
    t.system_color_edit_mode_hover_color                    = system_color_cyan;
    t.system_color_edit_mode_preview_color                  = system_color_amber;
    t.system_color_edit_mode_invalid_color                  = system_color_red;

    t.system_edit_mode_active_face_hover                    = system_color_edit_mode_active_face_hover;

    return t;
}

/* ---------------------------------------------------------- the list. add a theme = add a row. */
typedef struct {
    const char*          name;     /* keep it 8 characters or fewer: it is the button label in preferences */
    phoenix_system_theme (*build)(void);
} theme_entry;

static const theme_entry k_themes[] = {
    { "Maya",     build_maya     },
    { "ZBrush",   build_zbrush   },
    { "Plum",     build_plum     },
    { "Graphite", build_graphite },
};
#define THEME_COUNT ((int)(sizeof(k_themes) / sizeof(k_themes[0])))

int         theme_count(void)          { return THEME_COUNT; }
int         theme_get_index(void)      { return s_theme_index; }
const char* theme_name(int index)      { return (index >= 0 && index < THEME_COUNT) ? k_themes[index].name : ""; }

/* every color field a builder forgets stays {0,0,0,0}: fully transparent. say so in the log instead of silently drawing nothing. */
#define THEME_CHECK(field) do { if (t->field.alpha <= 0.0f) phoenix_warn("THEME", "theme '%s': %s is not set (alpha 0)", name, #field); } while (0)
static void theme_check_complete(const char* name, const phoenix_system_theme* t)
{
    THEME_CHECK(system_active_hover);
    THEME_CHECK(system_close_hover);
    THEME_CHECK(system_text);
    THEME_CHECK(system_active_text);
    THEME_CHECK(system_border);
    THEME_CHECK(system_accent);

    THEME_CHECK(menubar_background);
    THEME_CHECK(menubar_button);

    THEME_CHECK(menu_vert_separator);
    THEME_CHECK(menu_hort_separator);

    THEME_CHECK(menu_active_text);

    THEME_CHECK(toolbar_background);
    THEME_CHECK(toolbar_icon_button);
    THEME_CHECK(toolbar_stop_button);
    THEME_CHECK(toolbar_run_button);

    THEME_CHECK(tool_menu_vert_separator);
    THEME_CHECK(tool_menu_hort_separator);

    THEME_CHECK(statusbar_stop_background);
    THEME_CHECK(statusbar_run_background);
    THEME_CHECK(statusbar_stop_text);
    THEME_CHECK(statusbar_run_text);

    THEME_CHECK(mode_menu_background);
    THEME_CHECK(mode_menu_icon_button);

    THEME_CHECK(dropdown_outline);
    THEME_CHECK(menu_hover);
    THEME_CHECK(menu_open);
    THEME_CHECK(menu_item_selected);
    THEME_CHECK(menu_separator);
    THEME_CHECK(menu_text);
    THEME_CHECK(menu_header_text);
    THEME_CHECK(button_text);
    THEME_CHECK(context_menu_button);
    THEME_CHECK(context_menu_button_open);
    THEME_CHECK(context_menu_border);
    THEME_CHECK(context_menu_center);
    THEME_CHECK(context_menu_center_ring);
    THEME_CHECK(context_menu_text);
    THEME_CHECK(context_menu_arrow);
    THEME_CHECK(context_menu_dropdown);
    THEME_CHECK(context_menu_item_hover);
    THEME_CHECK(context_menu_item_text);
    THEME_CHECK(dock_frame);
    THEME_CHECK(dock_splitter);

    THEME_CHECK(tab_element);
    THEME_CHECK(tab_item);
    THEME_CHECK(tab_item_active);
    THEME_CHECK(tab_item_accent);
    THEME_CHECK(tab_item_text);
    THEME_CHECK(tab_item_text_active);
    THEME_CHECK(tab_close_hover);
    THEME_CHECK(tab_close_text);
    THEME_CHECK(tab_sub_menubar);
    THEME_CHECK(tab_sub_menubar_divider);
    THEME_CHECK(tab_sub_menubar_button);
    THEME_CHECK(tab_sub_menubar_button_active);
    THEME_CHECK(tab_content_window);
    THEME_CHECK(tab_content_window_modeler);
    THEME_CHECK(tab_content_window_gameview_00);
    THEME_CHECK(tab_content_inspector);
    THEME_CHECK(tab_content_window_paint);
    THEME_CHECK(tab_content_window_sculpt);
    THEME_CHECK(tab_content_window_sceneview);
    THEME_CHECK(tab_content_window_fileview);
    THEME_CHECK(tab_content_window_textureshop);
    THEME_CHECK(tab_content_window_textureshop_editor);
    THEME_CHECK(tab_content_window_textureshop_node);
    THEME_CHECK(tab_content_window_textureshop_skin);

    THEME_CHECK(edit_mode_multicut_percent_bg);
    THEME_CHECK(edit_mode_multicut_percent_text);

    THEME_CHECK(scene_manipulator_background);
    THEME_CHECK(scene_manipulator_input_background);
    THEME_CHECK(scene_manipulator_text);
    THEME_CHECK(scene_manipulator_input_text);

    THEME_CHECK(strip_window_body);
    THEME_CHECK(strip_grip);
    THEME_CHECK(strip_grip_lines);
    THEME_CHECK(strip_button);
    THEME_CHECK(strip_button_selected);
    THEME_CHECK(system_color_edit_mode_target_weld);
    THEME_CHECK(system_color_edit_mode_normal_color);
    THEME_CHECK(system_color_edit_mode_vert_color);
    THEME_CHECK(system_color_edit_mode_vert_active_color);
    THEME_CHECK(system_color_edit_mode_edge_color);
    THEME_CHECK(system_color_edit_mode_edge_active_color);
    THEME_CHECK(system_color_edit_mode_face_color);
    THEME_CHECK(system_color_edit_mode_face_active_color);
    THEME_CHECK(system_color_edit_mode_extrude_face_color);
    THEME_CHECK(system_color_edit_mode_extrude_face_active_color);
    THEME_CHECK(system_color_edit_mode_vert_selection_color);
    THEME_CHECK(system_color_edit_mode_edge_selection_color);
    THEME_CHECK(system_color_edit_mode_face_selection_color);
    THEME_CHECK(system_color_edit_mode_hover_color);
    THEME_CHECK(system_color_edit_mode_preview_color);
    THEME_CHECK(system_color_edit_mode_invalid_color);
    THEME_CHECK(system_edit_mode_active_face_hover);
}

void theme_set(int index)
{
    if (index < 0 || index >= THEME_COUNT) index = 0;
    s_theme_index = index;
    system_theme  = k_themes[index].build();
    theme_check_complete(k_themes[index].name, &system_theme);
}

void theme_apply_current(void) { theme_set(s_theme_index); }
void theme_load_default(void)  { theme_set(0); }

/* ========================================================================= */
/* theme functions                                                            */
/* ========================================================================= */
system_color process_theme_color(struct system_color raw_color, float brightness_modifier)
{
    system_color processed;
    
    // Values are already natively formatted as floats, simply apply modifier bounds cleanly
    processed.red   = raw_color.red   + brightness_modifier;
    processed.green = raw_color.green + brightness_modifier;
    processed.blue  = raw_color.blue  + brightness_modifier;
    processed.alpha = raw_color.alpha;

    if (processed.red > 1.0f) processed.red = 1.0f;
    if (processed.red < 0.0f) processed.red = 0.0f;
    if (processed.green > 1.0f) processed.green = 1.0f;
    if (processed.green < 0.0f) processed.green = 0.0f;
    if (processed.blue > 1.0f) processed.blue = 1.0f;
    if (processed.blue < 0.0f) processed.blue = 0.0f;

    return processed;
}

system_color color_with_alpha(system_color color, float alpha)
{
    color.alpha = alpha;
    return color;
}

system_color get_theme_hint_color(void)     { return process_theme_color(system_theme.system_text, -0.30f); }
system_color get_theme_disabled_color(void) { return process_theme_color(system_theme.system_text, -0.35f); }