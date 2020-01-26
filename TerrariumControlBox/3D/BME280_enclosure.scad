$fn=60;

dia=18;     //outer semisphere diameter
th=2;       //wall thickness
lng=40;     //outer length (exc. cable housing) 
off=14;     //bottom cut-off length
dia2=4;     //cable diameter (single)
dia3=2;     //vent hole diameter

dst=lng-dia;

translate([0,-10,0]) bme280_enclosure();
translate([0,10,0]) bme280_enclosure(double=true);


module bme280_enclosure(double=false) {
    translate([-dst/2,0,0]) difference() {
    
        union() {
            hull() {
                sphere(d=dia);
                translate([dst,0,0]) sphere(d=dia);
            }
            if (double)
                cable_hole_double(housing=true);
            else
                cable_hole_single(housing=true);
        }
        
        translate([dst/2,0,-dia/4]) cube([lng, dia, dia/2], center=true);
        
        translate([0, 0, 0]) {
            difference() {
                hull() {
                    sphere(d=dia-th*2);
                    translate([dst,0,0]) sphere(d=dia-th*2);
                }
                
                translate([dst/2+off,0,-dia/4+th/2]) cube([lng, dia, dia/2], center=true);
            }    
        }    
        
        translate([0.9,0,0]) vent_holes();
        
        if (double)
            cable_hole_double(housing=false);
        else
            cable_hole_single(housing=false);
    }    
}

module vent_holes() {
    translate([0,0,dia/2-th/2]) rotate([0,-45,0]) cylinder(d=dia3, h=th*4, center=true);
    translate([dst/2,0,dia/2-th/2]) rotate([0,-45,0]) cylinder(d=dia3, h=th*4, center=true);
    translate([dst,0,dia/2-th/2]) rotate([0,-45,0]) cylinder(d=dia3, h=th*4, center=true);
}


module cable_hole_single(housing) {
    holeD= housing ? dia2+th : dia2;
    holeH= housing ? th*2-0.1 : th*2;
    
    hull() {
        translate([-dia/2+th, 0, dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
        translate([-dia/2+th, 0, -dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
    }    
}

module cable_hole_double(housing) {    
    holeD= housing ? dia2+th : dia2;
    holeH= housing ? th*2-0.1 : th*2;
        
    hull() {
        translate([-dia/2+th, -dia2/2, -dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
        translate([-dia/2+th, dia2/2, -dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
        translate([-dia/2+th, dia2/2, dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
        translate([-dia/2+th, -dia2/2, dia2/2]) rotate([0,90,0]) cylinder(d=holeD, h=holeH, center=true);
    }                   
}