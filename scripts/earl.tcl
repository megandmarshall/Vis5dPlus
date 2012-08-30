# movie.tcl

# A Vis5d Tcl script for saving a whole time series of images
set base "frame."
set ext ".rgb"
set format VIS5D_RGB

set numtimes [ vis5d_get_dtx_numtimes $ctx ]

set segCount 0
set anglex 0
set anglez 0
set scale 1
set time 0
set segLength 60
vis5d_set_ortho_view $ctx VIS5D_TOP
vis5d_set_dtx_timestep $ctx [expr $time]
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglex [expr $anglex-1]
    vis5d_set_view $ctx $anglex 0.0 0.0 1.0 0.0 0.0 0.0
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
}
set segLength 60
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength 30
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglex [expr $anglex-0.5]
    vis5d_set_view $ctx $anglex 0.0 0.0 1.0 0.0 0.0 0.0
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength 40
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglex [expr $anglex+0.5]
    vis5d_set_view $ctx $anglex 0.0 0.0 1.0 0.0 0.0 0.0
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength 10
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglex [expr $anglex-0.5]
    vis5d_set_view $ctx $anglex 0.0 0.0 1.0 0.0 0.0 0.0
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength 45
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglez [expr $anglez-1]
    set anglex [expr $anglex-0.5]
    set scale [expr $scale+0.015]
    vis5d_set_view $ctx $anglex 0.0 $anglez $scale 0.0 0.0 0.0
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength 45
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    set anglez [expr $anglez+1]
    set anglex [expr $anglex+0.5]
    set scale [expr $scale-0.015]
    vis5d_set_view $ctx $anglex 0.0 $anglez $scale 0.0 0.0 0.0
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
set segLength [expr $numtimes - [expr $time + 1]]
for {set count 0} {$count<=$segLength} {set count [expr $count+1]} {
    vis5d_set_dtx_timestep $ctx [expr $time]
    vis5d_draw_frame $ctx
    set name $base$segCount$ext
    vis5d_save_window $name $format
    set segCount [expr $segCount + 1]
    set time [expr $time + 1]
}
