JSONINCLUDE=$(HOME)/dev/jsoncpp/include
JSONLIB=$(HOME)/dev/jsoncpp/lib
QTINCLUDE=/usr/include/qt4

CXX=g++
CXXFLAGS=-g3 -std=c++11 -Wall -pedantic $(JSONLIB)/libjsoncpp.a -I$(JSONINCLUDE) -I$(QTINCLUDE) -lQtCore



all: json

json: json.cpp
	$(CXX)  -o json json.cpp $(CXXFLAGS)

run: json result.osm

osmtesla.osm: overpass.post
	wget -O osmtesla.osm --post-file overpass.post http://overpass-api.de/api/interpreter

locations.json:
	wget -O locations.json http://supercharge.info/service/supercharge/allSites

result.osm: json osmtesla.osm locations.json
	./json locations.json osmtesla.osm > result.osm


clean:
	rm -f json osmtesla.osm locations.json result.osm