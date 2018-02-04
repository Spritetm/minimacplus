/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

//All measurements in mm

$fa=8; //segments/360 degrees
$fs=0.1; //min size of fragment
$fn=32; //override amount of facets


scale=50/300; //1-to-6

wall=2.5; //real mms

overlap=5; //real mms; overlap between halves 
overlapth=1.5; //real mms: thickness of material in overlap

//1 - cutout
//2 - full render
//3 - intersection case - pcb
//4 - front half / back half
//5 - front half / back half side-by-side
//6 - back
//7 - front
type=6;


if (type==1) {
    difference() {
        union() {
            mac_half_front(wall, scale, overlap, overlapth);
            translate([0,1,0]) mac_half_back(wall, scale, overlap, overlapth);
        }
        translate([125*scale, -500, -500]) cube([1000, 1000, 1000], false);
    }
    displaypcb(wall, scale);
    translate([0,1,0]) usbserpcb_mounted(wall);
} else if (type==2) {
    union() {
        mac_half_front(wall, scale, overlap, overlapth);
        mac_half_back(wall, scale, overlap, overlapth);
    }
    displaypcb(wall, scale);
    usbserpcb_mounted(wall);
} else if (type==3) {
    intersection() {
        union() {
            mac_half_front(wall, scale, overlap, overlapth);
            mac_half_back(wall, scale, overlap, overlapth);
        }
        displaypcb(wall, scale);
    }
} else if (type==4 || type==5) {
    translate([0, 10, 0]) union() {
        mac_half_back(wall, scale, overlap, overlapth);
        usbserpcb_mounted(wall);
    }
    rotate([0,0,type==5?180:0]) union () {
        mac_half_front(wall, scale, overlap, overlapth);
        *color("red") displaypcb(wall, scale);
    }
} else if (type==6) {
    mac_half_back(wall, scale, overlap, overlapth);
} else if (type==7) {
    mac_half_front(wall, scale, overlap, overlapth);
}


module mac_half_front(wall, scale, overlap, overlapth) {
    union() {
        scale(scale) union() {
            difference() {
                mac_hull(wall/scale);
                mac_crack_divider();
            }
            intersection() {
                mac_overlap_inner_edge(overlapth/scale);
                difference() {
                    mac_crack_divider_expanded(-0.8/scale);
                    mac_crack_divider_expanded(overlap/scale);
                }
            }
        }
        difference() {
            union() {
                //bottom screen tab
                translate([17+wall, -1, 8]) rotate([-20, 0, 0]) cube([10, 2, 3]);
                //top tab
                translate([14.5+wall, 1, 50]) cube([4, 5, 3]);
            }
            displaypcb(wall, scale);
        }
    }
}


module mac_half_back(wall, scale, overlap, overlapth) {
    union() {
    difference() {
        union() {
            scale(scale) difference() {
                intersection() {
                    mac_hull(wall/scale);
                    mac_crack_divider();
                }
                intersection() {
                    mac_overlap_inner_edge(overlapth/scale);
                    difference() {
                        mac_crack_divider();
                        mac_crack_divider_expanded(overlap/scale);
                    }
                }
            }
            //front tab for usb-ser conv
            translate([wall, 37.7-26-2, 0]) cube([16, 2, wall+1.7]);
            //back tabs for usb-ser conv
            translate([0, 37-5, wall+1.9]) cube([16+wall, 7, 3]); 
        }
        usb_hole(wall);
        mouse_hole(wall);
        usbserpcb_mounted(wall);        
    }
    //Enable for signatures in backplate
    //Disabled because 3d-printer doesn't really show these...
    //translate([wall, 38.2, 8+wall]) rotate([90, 0, 0]) scale(.2) linear_extrude(height = 3.3) import(file = "signatures.dxf");

    }
}

module usb_hole(wall) {
    w=3; //extra width for bevel
    i=0.2; //inset of bevel
    translate([16+wall,37.7,wall])  rotate([0,0,180]) union() {
        translate([4.5, -100, 1.6]) cube([7.55, 100 , 3.65]);
        translate([4.5-w/2, -wall+i-10, 1.6-w/2]) cube([7.55+w, 10 , 3.65+w]);
    }
}

module mouse_hole(wall) {
    translate([26+wall, 37.7, wall+3.2]) rotate([-90, 0, 0]) cylinder(d=2, 50);
}

module usbserpcb_mounted(wall) {
    translate([16+wall,37.7,wall]) rotate([0,0,180]) usbserpcb();
}

module usbserpcb() {
    cube([16, 26, 1.6]);
    translate([4.5, 0, 1.6]) cube([7.55, 9 , 3.65]);
}



//generates square hull you can cut the inner overlap out of
module mac_overlap_inner_edge(overlapth, isInner) {
    difference() {
        translate([overlapth, -200, overlapth]) cube([247.6-overlapth*2,500,334-overlapth*2]);
        translate([overlapth*2, -200, overlapth*2]) cube([247.6-overlapth*4,500,334-overlapth*4]);
    }
    
}


module mac_hull(wall) {
    difference() {
        mac_hull_solid(0);
        union() {
            mac_hull_solid(wall);
             translate([0, -25.4, 44.5]) rotate([-5,0, 0]) union() {            //Cutout the screen in the back of the frontplate as well
                translate([247.6/2, 0, 185]) rotate([90, 0, 0]) scale(1.69) mac_screen_cutout(wall/1.69);
                //Cutout fdd slot
                translate([129, -5.2, 62.8]) rotate([90,0,0]) fdd_cutout();
             }
        }
    }
}




module displaypcb(wall, scale) {
    translate([wall+.6,-1.6,60*scale]) rotate([90-5,180,180]) translate([0, -42, wall-2.05]) union() {
        cube([35,42, 0.8]); //main pcb
        translate([1, 6.8, 0]) cube([16, 24, 3.6+0.8]); //wroom block
        translate([1.6, 0.4, -1.2]) difference() { //screen
            cube([32.2, 36.75, 1.2]);
            translate([1.5, 2.73, -10+0.7]) cube([28.9, 19.5, 10]);
        }
    }
}



module mac_hull_solid(shrink) {
    rnd=4.5; //rounding of outer case
    //ir=0.8; //rounding of outmost screen bezel
    //a=-6.18; //angle of front
    r=(rnd<shrink)?0.01:rnd-shrink;
    difference() {
        union() {
            difference() {
                //Cube that makes up bottom and back of mac
                translate([shrink, shrink, shrink]) roundedcube(247.6-shrink*2, 241.3-shrink*2, 310-shrink*2, r);
                //cut off bit that sticks through front panel           
                translate([-500, -1000+150, 150]) cube([1000, 1000, 1000]);
            }
            //add in rotated front bulk
            translate([0, -25.4, 44.5]) rotate([-5,0, 0]) mac_front_bulk(4.5, 2, shrink);
        }
        union() {
            //Cut off diagonal bit in top-back of case
            translate([-1, 241-shrink, 266]) rotate([30, 0, 0]) cube([300, 300, 300], false);
            //Handle handle
            translate([247.6/2,105-shrink,280]) rotate([90+5,0,180]) mac_handle_hole(shrink);
        }
    }
}

module mac_front_bulk(or, ir, shrink) {
    fp_th=5.2; //thickness of front plate)
    fp_extra=(shrink<fp_th)?0:shrink-fp_th;
    or=(or<shrink)?0.01:or-shrink;
    ir=(ir<shrink)?0.01:ir-shrink;
    difference() {
        union() {
            difference() {
                translate([0+shrink, -20+shrink, 0+shrink]) roundedcube(247.6-shrink*2, 233.9+20-shrink*2, 292.1-shrink*2, or);
                union() {
                    //Cut off front where faceplate goes
                    translate([-500, -1000+fp_extra, -500]) cube([1000, 1000, 1000]);
                }
            }
            if (shrink==0) {
                mac_front_faceplate(or, ir);
            } else {
                translate([247.6/2, 50, 185])  rotate([90,0,0])scale([1.69, 1.69, 100]) mac_screen_cutout_back();
                
            }
        }
    }
}



//negative: handle hole (as in: hole for the handle to carry the mac)
//needs to be bigger when shrink increases
module mac_handle_hole(shrink) {
    r=6.4;
    difference() {
        union() {
            hull() {
                translate([-97/2+r-shrink, r-shrink ,0]) cylinder(r=r, 200);
                translate([97/2-r+shrink, r-shrink ,0]) cylinder(r=r, 200);
                translate([-103/2+r-shrink, 46-r ,0]) cylinder(r=r, 200);
                translate([103/2-r+shrink, 46-r ,0]) cylinder(r=r, 200);
            }
            if (shrink==0) {
                difference() {
                    translate([-103/2-r, 46-r, 0]) cube([103+2*r, 200, 200]);
                    union() {
                        translate([-103/2-r, 46-r ,0]) cylinder(r=r, 200);
                        translate([103/2+r, 46-r ,0]) cylinder(r=r, 2200);                
                    }
                }
            }
        }
        if (shrink==0) {
            //thing where you hold the mac
            translate([-100, 35, 0]) rotate([90,0,90]) hull() {
                cylinder(d=14.6, 200);
                translate([0, 35, 0]) cylinder(d=14.6, 200);            
            }
        }
    }
}

//crack divider, but moved up/right to get the overlap edge
module mac_crack_divider_expanded(ex) {
    translate([0,ex,ex]) mac_crack_divider();
}

module mac_crack_divider() {
    difference() {
        rotate([-5, 0, 0]) translate([0, -15, 65]) union() {
            hull () {
                translate([0,0,0]) rotate([0,90,0]) cylinder(r=1.5, 1000);
                translate([0,0,1000]) rotate([0,90,0]) cylinder(r=1.5, 1000);
                translate([0,1000,0]) rotate([0,90,0]) cylinder(r=1.5, 1000);
            }
            translate([-500,20,-500]) cube([1000,1000,1000]);            
        }
        translate([0, 15, 0]) hull() {
            translate([-500, 0, -200]) rotate([0,90,0]) cylinder(r=4.5, 1000);
            translate([-500, 0, 58.4]) rotate([0,90,0]) cylinder(r=4.5, 1000);            
            translate([-500, -200, 58.4]) rotate([0,90,0]) cylinder(r=4.5, 1000);            
        }
    }
}


module mac_front_faceplate() {
    mac_front_faceplate_outer(4.5, 2);
}

module mac_front_faceplate_outer(or, ir) {
    hull() {
        //connection to bulk of case
        translate([0+or, 0, 0+or]) rotate([90,0,0]) cylinder(1, or);
        translate([0+or, 0, 292.1-or]) rotate([90,0,0]) cylinder(1, or);
        translate([247.6-or, 0, 0+or]) rotate([90,0,0]) cylinder(1, or);
        translate([247.6-or, 0, 292.1-or]) rotate([90,0,0]) cylinder(1, or);
        //face of mac
        translate([0+3.4+ir, 1-5.2, 0+8+ir]) rotate([90,0,0]) cylinder(1, ir);
        translate([0+3.4+ir, 1-5.2, 292.1-10-ir]) rotate([90,0,0]) cylinder(1, ir);
        translate([247.6-3.4-ir, 1-5.2, 0+8+ir]) rotate([90,0,0]) cylinder(1, ir);
        translate([247.6-3.4-ir, 1-5.2, 292.1-10-ir]) rotate([90,0,0]) cylinder(1, ir);
    }
}


//WARNING: Everything inhere is adjusted using the wrong scale because I didn't pay attention. I'm too lazy to fix it all up, so if you use it scale(1.69) it first.
module mac_screen_cutout(depth) {
    or=2.5;
    ow=123-or*2;
    oh=94-or*2;
    hull() {
        //back cutout; fakes CRT curves
        translate([0,0,2.5-depth]) mac_screen_cutout_back();
        //very front of bezel
        translate([ow/2, oh/2, 5.2-1]) cylinder(1, r=or);
        translate([ow/2, -oh/2, 5.2-1]) cylinder(1, r=or);
        translate([-ow/2, oh/2, 5.2-1]) cylinder(1, r=or);
        translate([-ow/2, -oh/2, 5.2-1]) cylinder(1, r=or);
    }
}

//Same scale(1.69) is required here.
module mac_screen_cutout_back() {
    intersection() {
        intersection() {
            union() {
                translate([0, 23-(46-42), 0]) scale([246, 46, 1]) cylinder(10, d=1);
                translate([0, -23+(46-42), 0]) scale([246, 46, 1]) cylinder(10, d=1);
            }
            union() {
                translate([-56/2, 0, 0]) scale([56, 300, 1]) cylinder(1, d=1);
                translate([0, 0, 0]) scale([56, 300, 1]) cylinder(1, d=1);
                translate([56/2, 0, 0]) scale([56, 300, 1]) cylinder(1, d=1);
                
            }
        }
        
        union() {
            dr=6.2;
            w=111-dr*2;
            h=82-dr*2;
            translate([w/2, h/2, 0]) cylinder(10, r=6.2);
            translate([w/2, -h/2, 0]) cylinder(10, r=6.2);
            translate([-w/2, h/2, 0]) cylinder(10, r=6.2);
            translate([-w/2, -h/2, 0]) cylinder(10, r=6.2);
            translate([-w/2, -500, 0]) cube([w, 1000, 10]);
            translate([-500, -h/2, 0]) cube([1000, h, 10]);
        }
    }
}


//Negative space of FDD.
module fdd_cutout() {
        union() {
            fdd_cutout_inner_slot();
            fdd_cutout_inner_right();
            translate([0,0,0]) fdd_cutout_bezel();
            //fdd force eject button
            translate([98, -5,-50]) cylinder(100, r=1);
        }
}


module fdd_cutout_bezel() {
    thickness=1;
    union() {
        hull() {
            translate([0,0,5-thickness]) fdd_cutout_inner_slot();
            translate([-10.4, -11.5/2, 0]) flatroundedcube(112.3, 11.5, 10, 1);
        }
        hull() {
            translate([0,0,5-thickness]) fdd_cutout_inner_right();
            translate([57.7,-24/2 , 0]) flatroundedcube(44.5, 24, 10, 1);
        }
    }
}


module fdd_cutout_inner_slot() {
    translate([0, -2.6, -5]) cube([93.4, 5.2, 10]);
}

module fdd_cutout_inner_right() {
    translate([65.7, -8.9, -5]) flatroundedcube(27.7, 17.8, 10, 2.1); 
}

//Cube with 4 edges rounded; front/back are flat
module flatroundedcube(x,y,z,r) {
    hull(){
        translate([0+r, 0+r, 0]) cylinder(z, r=r);
        translate([0+r, y-r, 0]) cylinder(z, r=r);
        translate([x-r, 0+r, 0]) cylinder(z, r=r);
        translate([x-r, y-r, 0]) cylinder(z, r=r);
    }
}


//Cube with all edges rounded
module roundedcube(x, y, z, r) {
    if(0) {
        cube([x,y,z], false);
    } else {
        hull() {
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

