// ============================================================================
// ASYLYM // SCOPE-01 — Parametric Desktop Oscilloscope Enclosure
// Dimensions: 178 x 107 x 126 mm (110.5 mm with feet) | Parts: 16 | Supports: 0%
//
// Key features:
// - Inverted U-shaped translucent diffuser wrapping left, top, and right faces
// - 8x32 flexible WS2812B matrix (315 x 80 mm) wraps in a single layer across
//   3 flat-printed arch supports (outer arc length = 315.98 mm, 10 mm air gap)
// - Monolithic front & rear faceplates with integrated HUD paint grooves (0.65 mm),
//   speaker acoustic chamber, and angled ventilation louvers
// - Magnetic quick-release rear panel (4 pairs of Ø5x3 mm neodymium magnets)
// - Longitudinal ESP32 shield cradle (54.2 x 80.5 mm) with step clearance
// - Snap-fit KCD11 3-position rocker switch cutout (9.2 x 13.4 mm, 2.0 mm flange)
// - Fluted encoder knobs (Ø14.6 x 20.4 mm, T18 splined bore, extended skirt)
// ============================================================================

$fn = 96; // Increase to 256 before exporting white_diffuser for smooth curves

// --- VIEW MODE ---
// "assembly"     - Fully assembled device (fit and alignment check)
// "exploded"     - Exploded view of all internal and external components
// "print_layout" - All 16 parts laid flat at Z = 0.000 across 3 color plates
view_mode = "assembly";

// Highlight 0.65 mm acrylic paint-infill grooves in the OpenSCAD viewport
paint_preview = true;
paint_color   = "#ff6600";

// ============================================================================
// 1. CONCENTRIC ENCLOSURE DIMENSIONS (mm)
// Shared upper corner arc centers: X = ±62.0, Y = 74.0 (above base_tray)
// ============================================================================
base_t        = 6.0;    // Flat base tray thickness
foot_h        = 3.5;    // Height of the 4 glue-on corner feet
bevel         = 1.5;    // Unified 45-deg outer chamfer on base, front, and rear

// Print-friendly FDM clearances (0.4 mm nozzle, PLA Matte / Basic)
tol           = 0.35;   // General slot & alignment tolerance (+0.70 mm per slot)
diff_tol      = 0.38;   // Diffuser light-trap channel clearance (1.60 mm slot)
rib_tol       = 0.40;   // Clearance between accent arch ribs and white diffuser

// 3 separate flat-printed arch supports for the 8x32 LED matrix (315 x 80 mm)
// Outer path length: 2*(88-14) + (152-28) + pi*14 = 148 + 124 + 43.98 = 315.98 mm
arch_w        = 152.0;  // Outer arch width under LED matrix
arch_h        = 88.0;   // Outer arch height above base_tray
arch_r        = 14.0;   // Safe bend radius for flexible WS2812B PCB
arch_wall     = 4.5;    // Arch wall thickness (inner clearance 143.0 x 83.5 mm)
sup_d         = 10.0;   // Extrusion thickness of each support arch
lip_z         = 3.0;    // Matrix retention lips on front and rear supports

// Translucent U-shaped white diffuser (uniform 10.0 mm air gap on all sides)
air_gap       = 10.0;
diff_w        = arch_w + 2 * air_gap; // 172.0 mm
diff_h_vis    = arch_h + air_gap;     // 98.0 mm visible height
diff_r        = arch_r + air_gap;     // 24.0 mm
diff_t        = 0.84;                 // 0.84 mm wall (2 perimeters x 0.42 mm)
diff_vis_d    = 88.0;                 // Exposed diffuser depth
diff_ins_pkt  = 3.5;                  // Light-trap channel depth in frames
diff_ins_act  = 3.0;                  // Actual diffuser engagement (0.5 mm/end margin)
diff_d        = diff_vis_d + 2 * diff_ins_act; // 94.0 mm total print height
leg_drop_z    = 2.2;                  // Bottom tenon drop into base_tray
slot_depth_z  = 2.8;                  // Base slot depth (+0.6 mm vertical margin)

// Outer envelope of front & rear monolithic frames (+3.0 mm step over diffuser)
box_w         = diff_w + 6.0;         // 178.0 mm
box_h_upper   = diff_h_vis + 3.0;     // 101.0 mm (107.0 mm total enclosure height)
box_r         = diff_r + 3.0;         // 27.0 mm

front_d       = 24.0;                 // Front frame depth (20 mm open wiring bay)
rear_d        = 14.0;                 // Magnetic rear frame depth
total_d       = front_d + diff_vis_d + rear_d; // 126.0 mm total enclosure depth
front_wall    = 4.0;                  // Structural front faceplate thickness
rear_wall     = 3.6;                  // Structural rear faceplate thickness
paint_d       = 0.65;                 // Recessed groove depth for acrylic paint

// Inner cavity of front and rear frames
in_w          = box_w - 13.0;         // 165.0 mm (X = -82.5 .. +82.5)
in_h          = box_h_upper - 6.5;    // 94.5 mm
in_r          = box_r - 6.5;          // 20.5 mm (shared arc centers ±62.0, 74.0)

// Y-start coordinates of the 3 matrix supports (shared by base_tray & assembly)
// Matrix track width between lips: (112.0 - 3.0) - (24.4 + 3.0) = 81.6 mm
sup_front_y   = front_d + 0.4;                  // 24.4 mm (0.4 mm gap to front_frame)
sup_rear_y    = total_d - rear_d - sup_d;       // 102.0 mm (back face at 112.0 mm)
sup_mid_y     = (sup_front_y + sup_rear_y) / 2; // 63.2 mm
sup_y_list    = [sup_front_y, sup_mid_y, sup_rear_y];

// 2 external accent arch ribs dividing the diffuser into 3 equal bays (23.33 mm)
rib_d         = 9.0;
rib1_y        = front_d + (diff_vis_d - 2*rib_d)/3 + rib_d;     // 56.33 mm
rib2_y        = front_d + 2*(diff_vis_d - 2*rib_d)/3 + 2*rib_d; // 88.67 mm

// ============================================================================
// 2. ELECTRONICS & HARDWARE FIT (mm)
// ============================================================================
// ESP32 expansion shield (longitudinal: 53 mm port edge along X, 75 mm + step along Y)
shield_w        = 53.0;               // Port-side edge width (along X)
shield_l        = 75.0;               // Board length (along depth Y)
shield_step_ext = 4.5;                // Extra clearance for 3-4 mm far-edge step
shield_tol_w    = 0.60;               // Cradle inner width: 54.2 mm
shield_tol_l    = 0.50;               // Cradle inner length: 80.5 mm
ledge_h         = 4.2;                // Support ledge height (clears bottom solder pins)
cradle_h        = 7.8;                // Retaining wall height
shield_y_back   = total_d - rear_wall - 3.4; // 119.0 mm (1.0 mm gap to rear_frame)
shield_yc       = shield_y_back - (shield_l + shield_step_ext + 2*shield_tol_l)/2; // 78.75 mm

// 1.9" ST7789 IPS TFT display (upper-left; mount header pins pointing right/inward)
tft_x         = -46.5;
tft_y         = 58.0;
tft_win_w     = 50.0;                 // Visible screen cutout
tft_win_h     = 26.0;
tft_pcb_w     = 64.0;                 // Rear PCB pocket
tft_pcb_h     = 31.0;
tft_pkt_d     = 1.8;                  // Leaves 2.2 mm front bezel thickness

// KCD11 3-position mini rocker switch (lower-left, snap-fit directly into faceplate)
sw_x          = -60.5;
sw_y          = 22.0;
sw_w          = 9.2;                  // Through-cutout for switch body (~8.8 mm + clearance)
sw_h          = 13.4;                 // Through-cutout for switch body (~13.0 mm + clearance)
sw_bezel_w    = 10.6;                 // Outer switch bezel reference (for HUD brackets)
sw_bezel_h    = 15.6;
sw_pkt_w      = 13.0;                 // Rear relief pocket for expanded snap-fit clips
sw_pkt_h      = 18.0;
sw_pkt_d      = 2.0;                  // Leaves a 2.0 mm snap-fit flange (ideal for KCD11)

// 40 mm 4-Ohm 3W speaker (adjust only spk_d if your driver basket differs)
spk_d         = 40.0;                 // Actual speaker basket outer diameter
spk_x         = 16.5;
spk_y         = 47.0;
cup_id        = spk_d + 0.6;          // 40.6 mm sliding fit inside acoustic chamber
cup_od        = cup_id + 6.0;         // 46.6 mm outer diameter (3.0 mm wall)
spk_cone_d    = spk_d - 3.2;          // 36.8 mm cone clearance bore (1.9 mm mounting lip)
spk_cone_clr  = 1.2;                  // 1.2 mm excursion clearance behind grille
cup_depth     = 32.0;                 // Total chamber depth from front face

// Dual rotary encoders (right vertical column, 36 mm pitch)
enc_x         = 62.5;
enc1_y        = 63.0;
enc2_y        = 27.0;
enc_hole_d    = 7.8;                  // M7 threaded bushing clearance hole
enc_pcb_d     = 27.0;                 // Rear pocket for Ø24 mm / KY-040 module
enc_pkt_d     = 1.8;                  // Leaves 2.2 mm front flange for M7 nut engagement

// Rear I/O cutout for ESP32 shield USB Type-C & DC 6-12V barrel jack
io_port_w     = 72.0;
io_port_h     = 21.0;
io_port_y0    = 4.0;

// Magnetic rear cover latches (4 pairs of Ø5x3 mm neodymium magnets)
mag_d         = 5.35;
mag_h         = 3.35;
mag_boss_d    = 11.0;

// Shared magnet coordinates (0.000 mm coaxial alignment between rear support & rear_frame)
mag_pts = [
    [-65.5,  9.0],
    [ 65.5,  9.0],
    [-52.0, 77.0],
    [ 52.0, 77.0]
];

// ============================================================================
// 3. ENCODER KNOBS (Fluted Oscilloscope Style, T18 Splined Bore, Extended Skirt)
// ============================================================================
spline_teeth        = 18;     // Shaft spline count (18 for standard T18, rarely 20)
shaft_od            = 6.0;    // Shaft outer diameter across spline peaks
shaft_tol           = 0.25;   // Press-fit tolerance (tight -> 0.30, loose -> 0.20)
spline_depth        = 0.30;   // Spline tooth engagement depth
shaft_above_panel   = 18.8;   // Height from front face to shaft tip
bushing_above_panel = 8.8;    // Hidden lower zone (M7 nut + bushing + smooth shaft)

knob_d              = 14.6;   // Outer diameter (sits cleanly inside Ø14.8 mm C-scale)
knob_bevel_top      = 1.2;    // Top 45-deg chamfer
knob_cap_t          = 2.4;    // Solid blind top cap thickness (no paint grooves)
knob_panel_gap      = 0.8;    // Clearance above faceplate for push-button travel

// Derived knob dimensions
knob_h              = shaft_above_panel + knob_cap_t - knob_panel_gap;           // 20.4 mm
knob_nut_cav_d      = 12.2;                                                      // M7 nut cavity
knob_nut_cav_h      = max(2.2, bushing_above_panel + 0.6 - knob_panel_gap);      // 8.6 mm
knob_collar_h       = knob_nut_cav_h + 0.6;                                      // 9.2 mm skirt

// ============================================================================
// HELPER 2D MODULES
// ============================================================================
module rect_rounded_2d(w, h, r) {
    offset(r = r) square([w - 2*r, h - 2*r], center = true);
}

// Chamfered rectangle for flush 45-deg corner transitions between base and frames
module chamfer_rect_2d(w, h, c) {
    polygon([
        [-w/2 + c, -h/2], [ w/2 - c, -h/2],
        [ w/2, -h/2 + c], [ w/2,  h/2 - c],
        [ w/2 - c,  h/2], [-w/2 + c,  h/2],
        [-w/2,  h/2 - c], [-w/2, -h/2 + c]
    ]);
}

// Solid inverted-U arch profile: X = [-w/2, +w/2], Y = [0, h]
module arch_solid_2d(w, h, r_top, r_bot = 0) {
    if (r_bot <= 0) {
        hull() {
            translate([-w/2, 0]) square([w, 0.1]);
            translate([-w/2 + r_top, h - r_top]) circle(r = r_top);
            translate([ w/2 - r_top, h - r_top]) circle(r = r_top);
        }
    } else {
        hull() {
            translate([-w/2 + r_bot, r_bot]) circle(r = r_bot);
            translate([ w/2 - r_bot, r_bot]) circle(r = r_bot);
            translate([-w/2 + r_top, h - r_top]) circle(r = r_top);
            translate([ w/2 - r_top, h - r_top]) circle(r = r_top);
        }
    }
}

// Hollow inverted-U arch strip of thickness `thick` with optional bottom tenons
module arch_strip_2d(w_out, h_out, r_out, thick, leg_ext = 0) {
    difference() {
        union() {
            arch_solid_2d(w_out, h_out, r_out);
            if (leg_ext > 0)
                translate([-w_out/2, -leg_ext]) square([w_out, leg_ext + 0.1]);
        }
        union() {
            arch_solid_2d(w_out - 2*thick, h_out - thick, r_out - thick);
            translate([-(w_out - 2*thick)/2, -leg_ext - 1.0])
                square([w_out - 2*thick, leg_ext + 1.2]);
        }
    }
}

// HUD corner bracket with outer corner at (0,0) and arms extending in -X and -Y
module corner_bracket_2d(arm_x, arm_y, thick) {
    union() {
        translate([-arm_x, -thick]) square([arm_x, thick]);
        translate([-thick, -arm_y]) square([thick, arm_y]);
    }
}

// Perimeter bezel groove for front and rear faceplates (concentric with ±62, 74)
module faceplate_border_2d(line_w = 1.2) {
    fw = 164.0;
    fy = 5.0;
    fh = 89.0;
    fr_top = 20.0;
    fr_bot = 3.5;

    difference() {
        translate([0, fy])
            arch_solid_2d(fw, fh, fr_top, fr_bot);
        translate([0, fy + line_w])
            arch_solid_2d(fw - 2*line_w, fh - 2*line_w, fr_top - line_w, fr_bot - line_w);
    }
}

// Star-shaped 2D bore profile for splined rotary encoder shafts (T18 / T20)
module spline_shaft_2d() {
    r_out = (shaft_od + shaft_tol) / 2;
    r_in  = r_out - spline_depth;

    polygon([
        for (i = [0 : spline_teeth * 2 - 1])
            let(
                ang = i * 180 / spline_teeth,
                rr  = (i % 2 == 0) ? r_out : r_in
            )
            [rr * cos(ang), rr * sin(ang)]
    ]);
}

// ============================================================================
// PART 0: GLUE-ON CORNER FOOT (corner_foot — print 4x, black PLA)
// ============================================================================
module corner_foot() {
    fw = 24.0;
    fr = 5.0;

    color("#1b1d22")
    union() {
        difference() {
            hull() {
                linear_extrude(0.8) rect_rounded_2d(fw - 2.4, fw - 2.4, fr - 1.0);
                translate([0, 0, 0.8])
                    linear_extrude(foot_h - 0.8) rect_rounded_2d(fw, fw, fr);
            }
            // Grip groove on bottom sole
            translate([0, 0, -0.05])
            linear_extrude(0.5)
            difference() {
                rect_rounded_2d(fw - 6.0, fw - 6.0, 2.5);
                rect_rounded_2d(fw - 9.0, fw - 9.0, 1.5);
            }
        }
        // Dual alignment pins along Y (0.15 mm body overlap for manifold mesh)
        for (dy = [-6.0, 6.0])
            translate([0, dy, foot_h - 0.15])
                cylinder(d1 = 4.6, d2 = 4.0, h = 2.15);
    }
}

// ============================================================================
// PART 1: BASE TRAY WITH LONGITUDINAL ESP32 CRADLE (base_tray — 178 x 126 x 6 mm)
// Prints flat on Z = 0.000 without supports.
// ============================================================================
module base_tray() {
    pw_in   = shield_w + 2 * shield_tol_w;                   // 54.2 mm (along X)
    pd_in   = shield_l + shield_step_ext + 2 * shield_tol_l; // 80.5 mm (along Y)
    frame_t = 2.4;
    ledge_w = 3.2;

    union() {
        color("#3b4049")
        difference() {
            // Main plate with 1.5 mm 45-deg bottom and vertical corner chamfers
            hull() {
                translate([0, total_d/2, 0])
                    linear_extrude(0.1)
                    chamfer_rect_2d(box_w - 2*bevel, total_d - 2*bevel, bevel);
                translate([0, total_d/2, bevel])
                    linear_extrude(base_t - bevel)
                    chamfer_rect_2d(box_w, total_d, bevel);
            }

            // 1. Bottom alignment sockets for 4 corner feet (45-deg conical ceiling)
            for (sx = [-1, 1]) for (py = [22.0, total_d - 22.0]) {
                translate([sx * 62.0, py, 0])
                for (dy = [-6.0, 6.0]) {
                    translate([0, dy, -0.05]) {
                        cylinder(d = 5.3, h = 2.4);
                        translate([0, 0, 2.30]) cylinder(d1 = 5.3, d2 = 0.2, h = 2.45);
                    }
                }
            }

            // 2. Longitudinal light-trap slots for white_diffuser bottom edges
            groove_w   = diff_t + 2 * diff_tol; // 1.60 mm
            diff_mid_x = diff_w/2 - diff_t/2;   // 85.58 mm
            for (sx = [-1, 1]) {
                translate([
                    sx * diff_mid_x - groove_w/2,
                    front_d - diff_ins_act - 0.6,
                    base_t - slot_depth_z
                ])
                    cube([groove_w, diff_d + 1.2, slot_depth_z + 1.0]);
            }

            // 3. Four outer edge slots for the 2 accent arch ribs (diffuser_rib)
            rib_slot_x0 = diff_w/2 + diff_tol - 0.2;
            rib_slot_w  = box_w/2 - rib_slot_x0 + 0.5;
            for (sx = [-1, 1]) for (ry = [rib1_y, rib2_y]) {
                translate([
                    (sx < 0) ? (-box_w/2 - 0.5) : rib_slot_x0,
                    ry - rib_d - tol,
                    base_t - slot_depth_z
                ])
                    cube([rib_slot_w, rib_d + 2*tol, slot_depth_z + 1.0]);
            }

            // 4. Six tenon sockets for the 3 matrix support arches
            for (sx = [-1, 1]) for (sy = sup_y_list) {
                translate([
                    (sx < 0) ? -(arch_w/2 + tol) : (arch_w/2 - arch_wall - tol),
                    sy - tol,
                    base_t - slot_depth_z
                ])
                    cube([arch_wall + 2*tol, sup_d + 2*tol, slot_depth_z + 1.0]);
            }

            // 5. Bottom intake ventilation slots (routed cleanly around ESP32 cradle)
            for (ix = [-3 : 3]) {
                translate([ix * 11.0 - 2.0, 12.0, -0.5])
                    cube([4.0, 19.0, base_t + 1.0]);
            }
            for (sx = [-1, 1]) for (dx = [38.0, 48.0]) {
                translate([sx * dx - 2.0, 44.0, -0.5])
                    cube([4.0, 40.0, base_t + 1.0]);
            }
        }

        // 6. Two-tier longitudinal ESP32 shield cradle (54.2 x 80.5 mm)
        color("#2d323a")
        translate([0, shield_yc, base_t - 0.15])
        difference() {
            union() {
                // Continuous perimeter ledge at Z = 4.2 mm (clears underside solder joints)
                linear_extrude(ledge_h + 0.15)
                difference() {
                    rect_rounded_2d(pw_in + 2*frame_t, pd_in + 2*frame_t, 3.0);
                    square([pw_in - 2*ledge_w, pd_in - 2*ledge_w], center = true);
                }
                // Upper retaining rim
                translate([0, 0, ledge_h])
                linear_extrude(cradle_h - ledge_h + 0.15)
                difference() {
                    rect_rounded_2d(pw_in + 2*frame_t, pd_in + 2*frame_t, 3.0);
                    square([pw_in, pd_in], center = true);
                }
            }
            // Rear port edge (+Y): trimmed to 1.4 mm above ledge to hold PCB without
            // obstructing USB-C, Micro-USB, or DC barrel jacks near board corners
            translate([-(pw_in + 0.4)/2, pd_in/2 - 2.0, ledge_h + 1.55])
                cube([pw_in + 0.4, frame_t + 6.0, cradle_h]);

            // Front wiring opening (leaves 7.1 mm corner stops for stepped board edge)
            translate([-20.0, -pd_in/2 - frame_t - 1.0, -0.1])
                cube([40.0, frame_t + ledge_w + 2.0, cradle_h + 2.0]);

            // Side finger/wire notches in upper rim (lower support ledge stays continuous)
            for (sx = [-1, 1])
                translate([sx * (pw_in/2 + frame_t/2), 0, ledge_h + 0.10 + cradle_h/2])
                    cube([frame_t + 2.0, 34.0, cradle_h], center = true);
        }

        // 7. Alignment stops for gluing front_frame and seating magnetic rear_frame
        color("#3b4049") {
            translate([-38.0, front_wall + 0.4, base_t - 0.15])
                cube([76.0, 3.0, 2.95]);
            for (sx = [-1, 1])
                translate([(sx < 0) ? -(in_w/2 - 0.4) : (in_w/2 - 0.4 - 3.0), 8.0, base_t - 0.15])
                    cube([3.0, 11.0, 2.95]);
            for (sx = [-1, 1])
                translate([sx * 50.0 - 6.0, total_d - rear_wall - 0.4 - 3.0, base_t - 0.15])
                    cube([12.0, 3.0, 2.75]);
        }
    }
}

// ============================================================================
// PART 2: MONOLITHIC FRONT FRAME WITH HUD DECOR & SPEAKER CHAMBER (front_frame)
// Local z = front_d (24.0) is the outer face (prints face-down at Z = 0.000).
// ============================================================================
module front_frame() {
    difference() {
        color("#3b4049")
        union() {
            // Main arch shell with 45-deg 1.5 mm front chamfer
            linear_extrude(front_d - bevel + 0.1)
                arch_solid_2d(box_w, box_h_upper, box_r);
            hull() {
                translate([0, 0, front_d - bevel - 0.05])
                    linear_extrude(0.1) arch_solid_2d(box_w, box_h_upper, box_r);
                translate([0, 0, front_d - 0.05])
                    linear_extrude(0.05)
                    arch_solid_2d(box_w - 2*bevel, box_h_upper - bevel, box_r - bevel);
            }

            // Integrated acoustic chamber for Ø40 mm speaker (extends 32 mm inward)
            translate([spk_x, spk_y, front_d - cup_depth])
                cylinder(d = cup_od, h = cup_depth - 0.5);
        }

        // --- INTERNAL CAVITY & HARDWARE CUTOUTS ---
        color("#3b4049") {
            // 1. Hollow wiring bay (20.0 mm deep behind faceplate, preserves speaker cup)
            difference() {
                translate([0, 0, -1.0])
                linear_extrude(front_d - front_wall + 1.0) {
                    arch_solid_2d(in_w, in_h, in_r);
                    translate([-in_w/2, -2.0]) square([in_w, 3.0]);
                }
                translate([spk_x, spk_y, front_d - cup_depth - 2.0])
                    cylinder(d = cup_od, h = cup_depth + 2.0);
            }

            // 2. U-shaped light-trap groove (3.5 mm deep) for white_diffuser front edge
            translate([0, 0, -0.1])
                linear_extrude(diff_ins_pkt + 0.1)
                arch_strip_2d(
                    diff_w + 2*diff_tol,
                    diff_h_vis + diff_tol,
                    diff_r + diff_tol,
                    diff_t + 2*diff_tol,
                    leg_ext = 1.0
                );

            // 3. 1.9" ST7789 display: 50x26 mm window + 64x31 mm rear PCB pocket
            translate([tft_x - tft_win_w/2, tft_y - tft_win_h/2, front_d - front_wall - 1.0])
                cube([tft_win_w, tft_win_h, front_wall + 2.0]);
            translate([tft_x - tft_pcb_w/2, tft_y - tft_pcb_h/2, front_d - front_wall - 0.5])
                cube([tft_pcb_w, tft_pcb_h, tft_pkt_d + 0.5]);

            // 4. KCD11 3-pos switch: 9.2x13.4 mm body cutout + 13x18 mm rear clip relief
            //    (leaves a 2.0 mm snap-fit flange; outer bezel seats cleanly on front face)
            translate([sw_x - sw_w/2, sw_y - sw_h/2, front_d - front_wall - 1.0])
                cube([sw_w, sw_h, front_wall + 2.0]);
            translate([sw_x - sw_pkt_w/2, sw_y - sw_pkt_h/2, front_d - front_wall - 0.5])
                cube([sw_pkt_w, sw_pkt_h, sw_pkt_d + 0.5]);

            // 5. Dual rotary encoders: Ø7.8 mm shaft holes + Ø27 mm rear module pockets
            for (ey = [enc1_y, enc2_y]) {
                translate([enc_x, ey, front_d - front_wall - 1.0])
                    cylinder(d = enc_hole_d, h = front_wall + 2.0);
                translate([enc_x, ey, front_d - front_wall - 0.5])
                    cylinder(d = enc_pcb_d, h = enc_pkt_d + 0.5);
            }

            // 6. Speaker bore (Ø40.6 mm) + Ø36.8 x 1.2 mm front cone excursion recess
            //    (provides a 1.9 mm mounting lip and zero-support face-down printing)
            translate([spk_x, spk_y, front_d - cup_depth - 1.0])
                cylinder(d = cup_id, h = cup_depth - front_wall + 1.0);
            translate([spk_x, spk_y, front_d - front_wall - 0.2])
                cylinder(d = spk_cone_d, h = spk_cone_clr + 0.2);

            // 7. Integrated radial turbine speaker grille (24 through-slots)
            translate([spk_x, spk_y, front_d - front_wall - 0.5])
            for (r_ring = [7.5, 13.5]) {
                n_s = (r_ring == 7.5) ? 8 : 16;
                for (i = [0 : n_s - 1]) {
                    rotate([0, 0, i * (360 / n_s)])
                    translate([r_ring, 0, 0])
                    hull() {
                        cylinder(d = 2.6, h = front_wall + 1.0);
                        translate([2.6, 0, 0]) cylinder(d = 2.6, h = front_wall + 1.0);
                    }
                }
            }
        }

        // --- INTEGRATED FRONT HUD GROOVES FOR ACRYLIC PAINT INFILL (0.65 mm) ---
        color(paint_preview ? paint_color : "#3b4049")
        translate([0, 0, front_d - paint_d]) {
            // Outer bezel line
            linear_extrude(paint_d + 0.1)
                faceplate_border_2d(1.2);

            // Corner HUD brackets around ST7789 display
            for (sx = [-1, 1]) for (sy = [-1, 1]) {
                translate([tft_x + sx*(tft_win_w/2 + 3.2), tft_y + sy*(tft_win_h/2 + 3.2), 0])
                scale([sx, sy, 1])
                    linear_extrude(paint_d + 0.1)
                    corner_bracket_2d(9.0, 7.0, 1.3);
            }

            // Telemetry data bus and 5 chevrons under display
            translate([tft_x - 25.5, 37.5, 0])
                cube([31.0, 1.3, paint_d + 0.1]);
            for (i = [0 : 4]) {
                translate([tft_x + 8.5 + i * 3.4, 36.8, 0])
                linear_extrude(paint_d + 0.1)
                    polygon([[0, 0], [1.6, 0], [2.8, 2.6], [1.2, 2.6]]);
            }

            // Corner brackets framing the switch outer bezel (10.6 x 15.6 mm)
            for (sx = [-1, 1]) for (sy = [-1, 1]) {
                translate([sw_x + sx*(sw_bezel_w/2 + 2.8), sw_y + sy*(sw_bezel_h/2 + 2.6), 0])
                scale([sx, sy, 1])
                    linear_extrude(paint_d + 0.1)
                    corner_bracket_2d(3.5, 4.5, 1.2);
            }

            // Brand wordmark and underline bar
            translate([-46.5, 21.0, 0])
            linear_extrude(paint_d + 0.1)
                text("ASYLYM", size = 5.4, font = "Liberation Sans:style=Bold", spacing = 1.12);
            translate([-46.5, 17.2, 0])
                cube([32.0, 1.2, paint_d + 0.1]);

            // Segmented compass ring around speaker grille
            translate([spk_x, spk_y, 0]) {
                linear_extrude(paint_d + 0.1)
                difference() {
                    circle(d = 8.4);
                    circle(d = 5.8);
                }
                linear_extrude(paint_d + 0.1)
                difference() {
                    circle(d = 43.0);
                    circle(d = 40.4);
                    for (a = [0, 90, 180, 270]) rotate([0, 0, a])
                        square([46.0, 5.0], center = true);
                }
            }

            // Instrument C-scales around both encoders (knobs sit cleanly inside R=7.4 mm)
            for (ey = [enc1_y, enc2_y]) {
                translate([enc_x, ey, 0]) {
                    linear_extrude(paint_d + 0.1)
                    difference() {
                        circle(r = 8.6);
                        circle(r = 7.4);
                        translate([0, -6.5]) square([11.0, 7.0], center = true);
                    }
                    for (ang = [-135 : 45 : 135]) {
                        rotate([0, 0, ang])
                        translate([-0.55, 8.2, 0])
                            cube([1.1, 1.3, paint_d + 0.1]);
                    }
                }
            }
        }
    }
}

// Rear acoustic sealing cap for the speaker chamber
module speaker_cap() {
    color("#3b4049")
    difference() {
        union() {
            cylinder(d = cup_od, h = 2.4);
            // Tapered plug (0.3 mm/side slip fit inside Ø40.6 mm chamber)
            translate([0, 0, 2.2])
                cylinder(d1 = cup_id - 0.6, d2 = cup_id - 1.2, h = 3.6);
        }
        // Speaker wire pass-through hole (seal with a drop of hot glue / B-7000)
        translate([0, 0, -1.0])
            cylinder(d = 6.0, h = 8.0);
    }
}

// ============================================================================
// PART 3: MONOLITHIC MAGNETIC REAR FRAME WITH LOUVERS & HUD (rear_frame)
// Local z = 0.000 is the outer rear face (prints face-down directly on PEI).
// ============================================================================
module rear_frame() {
    difference() {
        color("#3b4049")
        union() {
            hull() {
                linear_extrude(0.05)
                    arch_solid_2d(box_w - 2*bevel, box_h_upper - bevel, box_r - bevel);
                translate([0, 0, bevel - 0.05])
                    linear_extrude(0.1) arch_solid_2d(box_w, box_h_upper, box_r);
            }
            translate([0, 0, bevel - 0.1])
                linear_extrude(rear_d - bevel + 0.1)
                arch_solid_2d(box_w, box_h_upper, box_r);
        }

        // --- INTERNAL CAVITY, MAGNETS, I/O PORT & LOUVERS ---
        color("#3b4049") {
            // 1. Inner cavity (preserving 4 coaxial Ø11 mm magnet columns)
            difference() {
                translate([0, 0, rear_wall])
                linear_extrude(rear_d - rear_wall + 1.0) {
                    arch_solid_2d(in_w, in_h, in_r);
                    translate([-in_w/2, -2.0]) square([in_w, 3.0]);
                }
                for (pt = mag_pts) {
                    translate([pt[0], pt[1], rear_wall - 0.5])
                        cylinder(d = mag_boss_d, h = rear_d - rear_wall + 3.0);
                }
            }

            // 2. U-shaped light-trap groove (3.5 mm deep) for white_diffuser rear edge
            translate([0, 0, rear_d - diff_ins_pkt])
                linear_extrude(diff_ins_pkt + 0.2)
                arch_strip_2d(
                    diff_w + 2*diff_tol,
                    diff_h_vis + diff_tol,
                    diff_r + diff_tol,
                    diff_t + 2*diff_tol,
                    leg_ext = 1.0
                );

            // 3. Four chamfered pockets for Ø5x3 mm neodymium magnets
            for (pt = mag_pts) {
                translate([pt[0], pt[1], rear_d - mag_h])
                    cylinder(d = mag_d, h = mag_h + 0.2);
                translate([pt[0], pt[1], rear_d - 0.40])
                    cylinder(d1 = mag_d, d2 = mag_d + 0.7, h = 0.50);
            }

            // 4. Wide I/O cutout (72 x 21 mm) for ESP32 shield USB-C & DC barrel jack
            translate([-io_port_w/2, io_port_y0, -1.0])
                cube([io_port_w, io_port_h, rear_wall + 2.0]);

            // 5. Dual banks of angled exhaust louvers (6 slots per bank)
            for (sx = [-33.0, 33.0]) {
                for (iy = [37.0 : 5.2 : 63.0]) {
                    translate([sx, iy, -0.5])
                    linear_extrude(rear_wall + 1.0)
                        polygon([
                            [-13.5, -1.1], [13.5, 1.1],
                            [ 13.5,  3.5], [-13.5, 1.3]
                        ]);
                }
            }
        }

        // --- INTEGRATED REAR HUD GROOVES FOR ACRYLIC PAINT INFILL ---
        color(paint_preview ? paint_color : "#3b4049")
        translate([0, 0, -0.05]) {
            linear_extrude(paint_d + 0.05)
            difference() {
                faceplate_border_2d(1.2);
                translate([-43.0, 0]) square([86.0, 12.0]);
            }

            for (sx = [-1, 1]) {
                translate([sx * (io_port_w/2 + 3.5), io_port_y0 + io_port_h + 3.5, 0])
                scale([sx, 1, 1])
                    linear_extrude(paint_d + 0.05)
                    corner_bracket_2d(8.0, 7.0, 1.3);
            }

            for (sx = [-1, 1]) for (sy = [-1, 1]) {
                translate([sx * 53.5, 51.25 + sy * 19.75, 0])
                scale([sx, sy, 1])
                    linear_extrude(paint_d + 0.05)
                    corner_bracket_2d(8.0, 7.0, 1.3);
            }

            // Asymmetric telemetry bar-graph between louver banks
            translate([-3.0, 36.5, 0])
                cube([1.2, 30.0, paint_d + 0.05]);
            for (i = [0 : 5]) {
                blen = (i == 0 || i == 3 || i == 5) ? 6.0 : 3.6;
                translate([-3.0, 36.5 + i * 5.7, 0])
                    cube([blen, 1.3, paint_d + 0.05]);
            }
        }
    }
}

// ============================================================================
// PART 4: THREE FLAT-PRINTED ARCH SUPPORTS FOR 8x32 LED MATRIX (dark PLA)
// ============================================================================
module matrix_support_front() {
    color("#20232a")
    union() {
        linear_extrude(sup_d)
            arch_strip_2d(arch_w, arch_h, arch_r, arch_wall, leg_ext = leg_drop_z);
        linear_extrude(lip_z)
            arch_strip_2d(arch_w + 3.6, arch_h + 1.8, arch_r + 1.8, arch_wall + 1.8, leg_ext = 0);
    }
}

module matrix_support_mid() {
    color("#20232a")
    linear_extrude(sup_d)
        arch_strip_2d(arch_w, arch_h, arch_r, arch_wall, leg_ext = leg_drop_z);
}

module matrix_support_rear() {
    color("#20232a")
    difference() {
        union() {
            linear_extrude(sup_d)
                arch_strip_2d(arch_w, arch_h, arch_r, arch_wall, leg_ext = leg_drop_z);
            linear_extrude(lip_z)
                arch_strip_2d(arch_w + 3.6, arch_h + 1.8, arch_r + 1.8, arch_wall + 1.8, leg_ext = 0);

            // 4 full-depth internal magnet bosses (prints flat on Z = 0 without overhangs)
            for (pt = mag_pts) {
                hull() {
                    translate([pt[0], pt[1], 0])
                        cylinder(d = mag_boss_d, h = sup_d);
                    if (pt[1] < 40) {
                        translate([(pt[0] > 0) ? (arch_w/2 - arch_wall) : -(arch_w/2 - arch_wall + 2.0), pt[1] - 5.5, 0])
                            cube([2.0, mag_boss_d, sup_d]);
                    } else {
                        translate([pt[0] - 5.5, arch_h - arch_wall, 0])
                            cube([mag_boss_d, 2.0, sup_d]);
                    }
                }
            }
        }

        // 4 magnet pockets on Z = 0 with lead-in chamfer and 45-deg conical ceiling
        for (pt = mag_pts) {
            translate([pt[0], pt[1], -0.05]) {
                cylinder(d1 = mag_d + 0.7, d2 = mag_d, h = 0.45);
                cylinder(d = mag_d, h = mag_h + 0.1);
                translate([0, 0, mag_h])
                    cylinder(d1 = mag_d, d2 = 0.4, h = 2.6);
            }
        }
    }
}

// ============================================================================
// PART 5: TRANSLUCENT U-SHAPED DIFFUSER (white_diffuser — matte white PLA)
// Print upright at Z = 0 with Arachne, 2 walls x 0.42 mm, 0% infill, 5 mm Brim.
// ============================================================================
module white_diffuser() {
    color("#f4f6fa", 0.86)
    linear_extrude(diff_d)
        arch_strip_2d(diff_w, diff_h_vis, diff_r, diff_t, leg_ext = leg_drop_z);
}

// ============================================================================
// PART 6: EXTERNAL ACCENT ARCH RIB (diffuser_rib — print 2x, black PLA)
// Transverse notches extrude vertically along print Z (zero bridges/overhangs).
// ============================================================================
module diffuser_rib() {
    rib_t  = (box_w - (diff_w + 2*rib_tol)) / 2; // 2.60 mm
    tick_w = 1.5;
    tick_d = 0.65;

    difference() {
        color("#1b1d22")
        linear_extrude(rib_d)
            arch_strip_2d(box_w, box_h_upper, box_r, rib_t, leg_ext = leg_drop_z);

        color(paint_preview ? paint_color : "#1b1d22")
        translate([0, 0, -0.1])
        linear_extrude(rib_d + 0.2) {
            for (sx = [-1, 1]) for (ty = [16, 28, 40, 52, 64]) {
                translate([sx * box_w/2, ty])
                    square([tick_d * 2, tick_w], center = true);
            }
            for (tx = [-48 : 12 : 48]) {
                translate([tx, box_h_upper])
                    square([tick_w, tick_d * 2], center = true);
            }
            for (sx = [-1, 1]) for (ang = [22.5, 45.0, 67.5]) {
                translate([sx * (box_w/2 - box_r), box_h_upper - box_r])
                rotate([0, 0, (sx > 0) ? ang : (180 - ang)])
                translate([box_r, 0])
                    square([tick_d * 2, tick_w], center = true);
            }
        }
    }
}

// ============================================================================
// PART 7: FLUTED ENCODER KNOB (knob_scope_fluted — print 2x, black PLA)
// Blind top cap (no paint grooves), T18 splined bore, extended 9.2 mm skirt.
// Prints cap-down on Textured PEI at Z = 0.000 without supports.
// ============================================================================
module knob_scope_fluted() {
    n_flutes = 12;
    flute_d  = 2.2;
    flute_r  = knob_d / 2 + 0.35;
    z_fade   = knob_h - knob_collar_h - 1.1; // Flutes blend smoothly at 45 deg into skirt
    z_mouth  = knob_h - knob_nut_cav_h;      // Transition from splined bore to skirt cavity

    difference() {
        // 1. Main cylindrical body with 45-deg top chamfer
        color("#1b1d22")
        union() {
            cylinder(d1 = knob_d - 2 * knob_bevel_top, d2 = knob_d, h = knob_bevel_top);
            translate([0, 0, knob_bevel_top - 0.05])
                cylinder(d = knob_d, h = knob_h - knob_bevel_top + 0.05);
        }

        // 2. 12 concave flutes tapering cleanly at 45 deg into the lower skirt
        color("#1b1d22")
        for (i = [0 : n_flutes - 1]) {
            rotate([0, 0, i * (360 / n_flutes)])
            translate([flute_r, 0, 0]) {
                translate([0, 0, -0.1])
                    cylinder(d = flute_d, h = z_fade + 0.1, $fn = 32);
                translate([0, 0, z_fade - 0.05])
                    cylinder(d1 = flute_d, d2 = 0.1, h = 1.15, $fn = 32);
            }
        }

        // 3. Splined shaft bore + lead-in funnel + deep M7 nut & shaft skirt cavity
        color("#1b1d22")
        union() {
            translate([0, 0, knob_cap_t])
                linear_extrude(z_mouth - knob_cap_t + 0.2)
                spline_shaft_2d();

            // 45-deg lead-in funnel for easy spline engagement
            translate([0, 0, z_mouth - 0.70])
                cylinder(
                    d1 = shaft_od + shaft_tol - 2 * spline_depth,
                    d2 = shaft_od + shaft_tol + 1.1,
                    h  = 0.75
                );

            // Deep skirt cavity concealing M7 nut, bushing, and lower shaft
            translate([0, 0, z_mouth])
                cylinder(d = knob_nut_cav_d, h = knob_nut_cav_h + 0.2);

            // Bottom rim inner chamfer
            translate([0, 0, knob_h - 0.35])
                cylinder(d1 = knob_nut_cav_d, d2 = knob_nut_cav_d + 0.7, h = 0.45);
        }
    }
}

// ============================================================================
// ASSEMBLY / EXPLODED VIEW / PRINT BED LAYOUT (16 PARTS TOTAL)
// ============================================================================
if (view_mode == "assembly" || view_mode == "exploded") {
    e = (view_mode == "exploded") ? 32.0 : 0.0;
    z_base = foot_h + base_t;

    // 0. Four corner feet
    for (sx = [-1, 1]) for (py = [22.0, total_d - 22.0]) {
        translate([sx * 62.0, py, -e * 0.4])
            corner_foot();
    }

    // 1. Base tray with longitudinal ESP32 shield cradle
    translate([0, 0, foot_h])
        base_tray();

    // 2. Monolithic front frame
    translate([0, front_d - e * 0.9, z_base])
        rotate([90, 0, 0])
        front_frame();

    // 2b. Speaker acoustic chamber rear cap
    translate([spk_x, cup_depth + 2.4 - e * 0.4, z_base + spk_y])
        rotate([90, 0, 0])
        speaker_cap();

    // 2c. Two fluted encoder knobs (0.8 mm push-click clearance in front of faceplate)
    for (ey = [enc1_y, enc2_y]) {
        translate([enc_x, -(knob_h + knob_panel_gap) - e * 1.5, z_base + ey])
            rotate([-90, 0, 0])
            knob_scope_fluted();
    }

    // 3. Three matrix arch supports (front, mid, rear with magnets)
    translate([0, sup_front_y, z_base + e * 0.45])
        rotate([90, 0, 180])
        matrix_support_front();

    translate([0, sup_mid_y, z_base + e * 0.45])
        rotate([90, 0, 180])
        matrix_support_mid();

    translate([0, sup_rear_y + sup_d, z_base + e * 0.45])
        rotate([90, 0, 0])
        matrix_support_rear();

    // 4. Translucent white diffuser
    translate([0, total_d - rear_d + diff_ins_act, z_base + e * 1.15])
        rotate([90, 0, 0])
        white_diffuser();

    // 5. Two black accent arch ribs
    for (ry = [rib1_y, rib2_y]) {
        translate([0, ry, z_base + e * 1.55])
            rotate([90, 0, 0])
            diffuser_rib();
    }

    // 6. Monolithic magnetic rear frame
    translate([0, total_d + e * 0.9, z_base])
        rotate([90, 0, 0])
        rear_frame();

} else if (view_mode == "print_layout") {
    // ========================================================================
    // PRINT BED LAYOUT: ALL 16 PARTS FLAT AT Z = 0.000 WITH 0% SUPPORTS
    // ========================================================================

    // --- PLATE 1: MAIN ENCLOSURE (4 parts, face-down / flat on Textured PEI) ---
    base_tray();
    translate([205, 0, front_d]) rotate([180, 0, 0]) translate([0, -box_h_upper, 0]) front_frame();
    translate([205, 125, 0]) rear_frame();
    translate([0, 165, 0]) speaker_cap();

    // --- PLATE 2: WHITE DIFFUSER (1 part) + MATRIX ARCH SUPPORTS (3 parts) ---
    translate([-215, 10, 0]) white_diffuser();
    translate([-215, 130, 0]) matrix_support_front();
    translate([-215, 240, 0]) matrix_support_mid();
    translate([-215, 350, 0]) matrix_support_rear();

    // --- PLATE 3: BLACK ACCENTS (2 arch ribs + 4 corner feet + 2 encoder knobs) ---
    translate([0, -120, 0]) diffuser_rib();
    translate([205, -120, 0]) diffuser_rib();

    for (i = [0 : 3]) {
        translate([-54 + i * 36, -155, 0]) corner_foot();
    }

    for (i = [0 : 1]) {
        translate([160 + i * 26, -155, 0]) knob_scope_fluted();
    }
}