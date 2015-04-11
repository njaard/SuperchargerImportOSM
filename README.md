This program imports all the superchargers from http://supercharger.info into
OSM.

Dependencies: Qt4, libjsoncpp

Typing "make run" compiles and runs the code.

* The input is the list of superchargers from supercharger.info (Downloaded
directly with wget).

* It also downloads a list of likely superchargers that are already in OSM with
wget and Overpass-Turbo's HTTP api. The file "overpass.post" is the Overpass
script that is run to get those candidates.

The output is result.osm, which can be loaded into JOSM and uploaded to the OSM
server.

