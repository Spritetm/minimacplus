$fa=8; //segments/360 degrees
$fs=0.1; //min size of fragment
$fn=32; //override amount of facets


//scale=50/300; //1-to-6
scale=1/2.5;

cheat=1.17; //cheat: scale the height up slightly to fit the sensor. 1=no cheating


wall=2.5;

btn_lip_outer=1.3;
btn_toler=.1;
btn_ex=.2; //how much the button 'sticks out'

mouse_lip_h=2;
mouse_lip_th=1.7;

//1 - presentation view / cutaway
//2 - bottom plate
//3 - top plate
//4 - button
type=5;

if (type==1) {
    difference() {
        union() {
            macmouse_lower(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
            translate([0,0,5]) macmouse_upper(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
        }
        //Cut off 2nd half of mouse
        translate([30*scale, 0, -10]) cube([1000, 1000, 1000]);
    }

    //Add sensor
    translate([(59*scale)/2, 65*scale, 1.3]) sensor(false);

    //Add button
    translate([0,0,5]) difference() {
        mousebtn(scale, wall, btn_lip_outer-btn_toler, btn_toler, btn_ex, cheat);
        translate([30*scale, 0, -10]) cube([1000, 1000, 1000]);
    }
} else if (type==2) {
    difference() {
        macmouse_lower(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
        translate([(59*scale)/2, 65*scale, 1.3]) sensor(true);
    }
} else if (type==3) {
    macmouse_upper(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
} else if (type==4) {
    mousebtn(scale, wall, btn_lip_outer-btn_toler, btn_toler, btn_ex, cheat);
} else if (type==5) {
    union() {
        translate([0,0,5]) macmouse_upper(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
    //Add button
    translate([0,0,5]) {
        mousebtn(scale, wall, btn_lip_outer-btn_toler, btn_toler, btn_ex, cheat);
    }
    }

        macmouse_lower(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th);
    

}


module macmouse_lower(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th) {
    difference() {
        union() {
            intersection() {
                macmouse(scale, wall, cheat, btn_lip_outer);
                cube([1000, 1000, 5.4*scale]);
            }
            intersection() {
                scale(scale, scale, scale*cheat) translate([0, 0, 5.4]) difference() {
                    macmouse_solid(mouse_lip_th/scale);
                    macmouse_solid(mouse_lip_th*2/scale);
                }
                translate([0,0,5.4*scale]) cube([1000, 1000, mouse_lip_h]);
            }
        }
        cord_hole(scale);
    }
}

module macmouse_upper(scale, wall, cheat, btn_lip_outer, mouse_lip_h, mouse_lip_th) {
    difference() {
        union() {
            difference() {
                macmouse(scale, wall, cheat, btn_lip_outer);
                cube([1000, 1000, 5.4*scale+mouse_lip_h]);
            }
            intersection() {
                macmouse(scale, wall-(mouse_lip_th/2), cheat, btn_lip_outer);
                translate([0,0,5.4*scale]) cube([1000, 1000, mouse_lip_h]);
            }
        }
        cord_hole(scale);
    }
}

module cord_hole(scale) {
    translate([59/2*scale, 30, wall+1.5]) rotate([-90,0,0]) cylinder(d=2, 100);
}

module macmouse(scale, wall, cheat, btn_lip_outer) {
    scale([1, 1, cheat]) difference() {
        macmouse_hull(scale, wall);
        union() {
            //button hole
            scale(scale) macmouse_btn_hole(0);
            //lip
            scale(scale) intersection() {
                macmouse_btn_hole(-btn_lip_outer/scale);
                translate([0, 0, 5.4+(btn_lip_outer/scale)]) macmouse_solid(wall/scale);
            }
        }
    }
}

module mousebtn(scale, wall, lipsz, toler, btn_ex, cheat) {
    difference() {
        scale([scale, scale, scale*cheat]) union() {
            intersection() {
                //mouse button itself
                translate([0, 0, 5.4]) difference() {
                    translate([0,0,btn_ex/scale]) macmouse_solid(0);
                    macmouse_solid(wall/scale);
                }
                macmouse_btn_hole(toler/scale);
            }
            intersection() {
                //lip
                translate([0, 0, 5.4]) difference() {
                    translate([0,0,lipsz/scale]) macmouse_solid(wall/scale);
                    macmouse_solid(wall/scale);
                }
                macmouse_btn_hole(-lipsz/scale);
            }
                
        }
        translate([(59*scale)/2, 65*scale+2.1, 1.3+11.35-(2.6/2)]) cube([5, 5, 2.6], true);  
    }
}

module sensor(negative) {
    union() {
        difference() {
            //inner oval on bottom
            flatroundedcube(9.3, 11.6, 1.5, 2, true);
            flatroundedcube(8.0, 10.3, 1.5, 2-0.3, true);
        }
        difference() {
            //2nd oval on bottom
            flatroundedcube(14.1, 16.3, 1.5, 4.5, true);
            flatroundedcube(12.9, 14.5, 1.5, 4.5-0.3, true);
        }
        if (negative) {
            translate([0,0,-20]) flatroundedcube(8.0, 10.3, 40, 2-0.3, true);
        }
        translate([0,0,1]) flatroundedcube(14.1, 16.3, 1, 4.5, true);
        translate([0,-1,2+(6.5/2)]) cube([8.6, 12.8, 6.5], true);
        translate([0,0.3,6.5+(2/2)]) cube([14.3, 15.6, 2], true);

        translate([0, 2.1, 8.5+(2.6/2)]) cube([3.4, 4.2, 2.6], true);  
    }
}


module macmouse_hull(scale, wall) {
    scale(scale) translate([0, 0, 5.4]) difference() {
        macmouse_solid(0);
        macmouse_solid(wall/scale);
    }
}


module macmouse_btn_hole(shrink) {
    translate([59/2, 73, 10]) flatroundedcube(35-shrink, 25-shrink, 100, 1.6, true);
}

module macmouse_solid(shrink) {
    union() {
        hull() {
            macmouse_topbulk(shrink);
            translate([0, 95.3, 0]) rotate([5, 0,  0]) translate([(59-47.6)/2, -92+16, 22-shrink]) cube([47.6, 60.3, 6]);
        }
        macmouse_bottombulk(shrink);
    }    
}

module macmouse_bottombulk(shrink) {
    r=5-shrink;
    v=0.4; //difference in rounding of corners of mouse vs bottom side of mouse
    difference() {
        translate([shrink, shrink, -5.4+shrink]) scale([1,1,v]) roundedcube(59-(shrink*2), 95.4-(shrink*2), 20/v, r);
        translate([-500, -500, 0]) cube([1000, 1000, 1000]);
    }
}

module macmouse_topbulk(shrink) {
    r=5-shrink;
    difference() {
        union() {
            //bulk... erm... bulk
            translate([shrink, 95.3+shrink, 0]) rotate([5, 0,  0]) translate([0, -92, 0]) flatroundedcube(59-shrink*2, 92-shrink*2, 30, r);
            //to add the corner near the bottom of the mouse
            translate([shrink, shrink, 0]) rotate([-5, 0, 0]) flatroundedcube(59-shrink*2, 20, 20, r);
        }
        union() {
            //cut off bottom so it's flat
            translate([-500, -500, -1000]) cube([1000, 1000, 1000]);
            //cut off top so it's flat
            rotate([5, 0, 0]) translate([-500, -500, 14.28-shrink*0.7]) cube([1000, 1000, 1000]);
        }
    }
}



//Cube with 4 edges rounded; front/back are flat
module flatroundedcube(x,y,z,r,center) {
    if(r<=0) {
        cube([x,y,z], center);
    } else {
        translate([center?-x/2:0, center?-y/2:0, 0]) hull(){
            translate([0+r, 0+r, 0]) cylinder(z, r=r);
            translate([0+r, y-r, 0]) cylinder(z, r=r);
            translate([x-r, 0+r, 0]) cylinder(z, r=r);
            translate([x-r, y-r, 0]) cylinder(z, r=r);
        }
    }
}


//Cube with all edges rounded
module roundedcube(x, y, z, r, center) {
    if(r<=0) {
        cube([x,y,z], center);
    } else {
        translate([center?-x/2:0, center?-y/2:0, 0]) hull() {
            translate([0+r,0+r,0+r]) sphere(r);
            translate([x-r,0+r,0+r]) sphere(r);
            translate([0+r,y-r,0+r]) sphere(r);
            translate([x-r,y-r,0+r]) sphere(r);
            translate([0+r,0+r,z-r]) sphere(r);
            translate([x-r,0+r,z-r]) sphere(r);
            translate([0+r,y-r,z-r]) sphere(r);
            translate([x-r,y-r,z-r]) sphere(r);
        }
    }
}

