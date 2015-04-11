#include <json/json.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <Qt/qregexp.h>
#include <Qt/qfile.h>
#include "xmltree.h"
#include <unordered_map>

static bool icompare(const std::string& a, const std::string& b)
{
	return std::lexicographical_compare(
		a.begin(), a.end(),
		b.begin(), b.end(),
		[] (char a, char b) { return std::tolower(a) == std::tolower(b); }
	);
}

static void string_replace(std::string& str, const std::string& from, const std::string& to)
{
	size_t start_pos=0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}
static std::string escape_xml(const std::string &s)
{
	std::string o;
	o.reserve(s.size());
	for (size_t i=0; i < s.size(); i++)
		if (s[i] == '\'')
			o += "&apos;";
		else if (s[i] == '<')
			o += "&lt;";
		else if (s[i] == '>')
			o += "&gt;";
		else if (s[i] == '&')
			o += "&amp;";
		else if (s[i] == '"')
			o += "&quot;";
		else
			o += s[i];
	return o;
}

static std::string pad2(int i)
{
	std::string x = std::to_string(i);
	if (x.size()==1)
		return "0"+x;
	else
		return x;
}

static std::string fix_opening_hours(std::string in)
{
	string_replace(in, "MO", "Mo");
	string_replace(in, "TU", "Tu");
	string_replace(in, "WE", "We");
	string_replace(in, "TH", "Th");
	string_replace(in, "FR", "Fr");
	string_replace(in, "SA", "Sa");
	string_replace(in, "SU", "Su");
	string_replace(in, "Mon", "Mo");
	string_replace(in, "Tue", "Tu");
	string_replace(in, "Wed", "We");
	string_replace(in, "Thu", "Th");
	string_replace(in, "Fri", "Fr");
	string_replace(in, "Sat", "Sa");
	string_replace(in, "Sun", "Su");
	string_replace(in, " - ", "-");
	string_replace(in, "M-F", "Mo-Fr");
	
	for (int i=1; i<=12; i++)
	{
		std::string p=pad2(i);
		string_replace(in, std::to_string(i) + "am", p+":00");
		string_replace(in, std::to_string(i) + ":30am", p+":30");
		string_replace(in, std::to_string(i) + ".30am", p+":30");
	}
	for (int i=1; i<=12; i++)
	{
		std::string p=pad2(i);
		string_replace(in, std::to_string(i) + "pm", p+":00");
		string_replace(in, std::to_string(i) + ":30pm", p+":30");
		string_replace(in, std::to_string(i) + ".30pm", p+":30");
	}
	
	string_replace(in, ",", ";");
	return in;
}

static std::string remove_suffix(std::string s, const std::string &suffix)
{
	const size_t at = s.rfind(suffix);
	if (at != std::string::npos && at == s.length()-suffix.length())
	{
		s.erase(at);
	}
	return s;
}

struct supercharger
{
	std::int64_t id=0;
	unsigned version=0;
	std::unordered_map<std::string, std::string> tags;
	std::string lat, lon;
	
	bool operator==(const supercharger &o) const
	{
		return id == o.id && version==o.version && tags == o.tags && lat == o.lat && lon==o.lon;
	}
};
class xmlnode
{
	QXmlStreamReader * const node;
public:
	xmlnode(QXmlStreamReader* n)
		: node(n)
	{ }

	std::string value(const std::string &name) const
	{
		return node->attributes().value(name.c_str()).toUtf8().constData();
	}

	std::string node_name() const
	{
		return node->name().toUtf8().constData();
	}
};

int main(int argc, char **argv)
{
	std::ostringstream ss;
	{
		std::ifstream findus(argv[1]);
		ss << findus.rdbuf();
	}
	
	Json::Value root;
	Json::Reader reader;
	if (!reader.parse( ss.str(), root ))
	{
		throw std::runtime_error(
				"Failed to parse token response: " + reader.getFormattedErrorMessages()
		);
	}
	
	struct table_of_specials
	{
		std::string num_superchargers;
		std::string hours;
	};

	const std::unordered_map<std::string, table_of_specials> specials
	{
		{ "564", { "2", "" } }
/*		{ "hamburg", { 2, "Mo-Fr 09:00-18:00" } },
		{ "hopewellcentresupercharger", { 6, "24/7" } },
		{ "kaitaksupercharger", { 4, "07:00-23:00" } },
		{ "parkmeadows", { 6, "24/7" } },
		{ "berlin", { 2, "Mo-Fr 09:00-17:30" } },
		{ "tilburgatlasstraat", { 1, "Mo-Fr 09:00-17:30; Sa 10:00-17:00" } },
		{ "montrealferrier", { 1, "24/7" } },
		{ "leegardenssupercharger", { 2, "07:30-00:30" } },
		{ "losangelessupercharger", { 12, "24/7" } } */
	};
	
	QFile f;
	f.setFileName(argv[2]);
	f.open(QFile::ReadOnly);
	QXmlStreamReader xml(&f);

	std::list<supercharger> created_superchargers;
	std::vector<supercharger> osm_superchargers;
	
	XmlTree tree(&xml);
	
	int64_t negative_ids=-1;

	tree.element("osm", [&] (const xmlnode &e)
	{
		tree.element("node", [&] (const xmlnode &e)
		{
			supercharger sc;
			sc.id = std::stoll(e.value("id"));
			sc.version = std::stoll(e.value("version"));
			sc.lat = e.value("lat");
			sc.lon = e.value("lon");
			
			tree.element("tag", [&] (const xmlnode &e)
			{
				sc.tags[e.value("k")] = e.value("v");
			});
			
			osm_superchargers.push_back(sc);
		});
	});
	
	std::cout << "<?xml version='1.0' encoding='UTF-8'?>\n<osm version='0.6' upload='true' generator='tesla'>\n";

	for (Json::ArrayIndex i=0; i < root.size(); i++)
	{
		const Json::Value& charger = root[i];
		//const std::string location_type = charger.get("location_type", "").asString();
		{
			const std::string status = charger.get("status", "").asString();
			if (!icompare(status, "OPEN"))
				continue;
		}
		
		
		
		const std::string id =
			charger["locationId"].isString()
				? charger.get("locationId", "").asString()
				: std::to_string(charger["locationId"].asInt());
		
		try
		{
			const auto is_special = specials.find(id);
			
			const std::string chargers
				= is_special != specials.end()
					? is_special->second.num_superchargers
					: charger.get("stallCount", "").asString();
			
			std::string name = charger.get("name", "").asString();
			const std::string hours
				= is_special != specials.end()
					? is_special->second.hours
					: fix_opening_hours(charger.get("hours", "24/7").asString());
			const std::string lat = charger["gps"].get("latitude", "").asString();
			const std::string lon = charger["gps"].get("longitude", "").asString();
			
			std::list<supercharger>::iterator sc;
			
			{
				// search superchargers_by_ref by location,
				// find the supercharger closest to me
				const supercharger *closest=nullptr;
				double dclosest = std::numeric_limits<double>::max();
				for (const supercharger &maybesc : osm_superchargers)
				{
					double latdiff = std::fabs(std::stod(maybesc.lat) - std::stod(lat));
					double londiff = std::fabs(std::stod(maybesc.lon) - std::stod(lon));
					double dist = std::sqrt(latdiff*latdiff + londiff*londiff);
					if (dist < 0.035 && dist < dclosest)
					{
						closest = &maybesc;
						dclosest = dist;
					}
				}
				if (closest)
				{
					std::cerr << "Connected id " << id << " with node " << closest->id
					//	<< " " << closest->tags["name"] << ", " << closest->tags["addr:city"]
						<< std::endl;
					sc = created_superchargers.insert(created_superchargers.end(), *closest);
				}
				else
				{
					sc = created_superchargers.insert(created_superchargers.end(), supercharger());
				}
				sc->id = negative_ids--;
			}
			
			// the supercharger that is already in OSM (or one with a lot of empty strings)
			const supercharger old_sc = *sc;
			
			sc->lat = lat;
			sc->lon = lon;
			if (!hours.empty())
				sc->tags["opening_hours"] = hours;
			sc->tags["socket:tesla_supercharger"] = chargers;
			sc->tags["capacity"] = chargers;
			sc->tags["operator"] = "Tesla Motors Inc.";
			sc->tags["amenity"] = "charging_station";
			
			
			
			{
				const Json::Value& lt = charger["address"];
				sc->tags["addr:city"] = lt.get("city", "").asString();
				if (!lt.get("zip", "").asString().empty())
					sc->tags["addr:postcode"] = lt.get("zip", "").asString();
				
				name = remove_suffix(name, ", " + lt.get("country","").asString());
				name = remove_suffix(name, ", " + lt.get("state","").asString());
			}
			
			sc->tags["name"] = "Tesla Supercharger " + name;

			// we haven't modified this entry at all, so don't
			// update it
			if (old_sc == *sc)
			{
				created_superchargers.erase(sc);
			}
			else
			{
				sc->version++;
			}
			
			//std::cout << nid << "\t" << title << "\t" << chargers << "\t" << city << "\t" << chargers << "\t" << hours << std::endl;
		}
		catch (std::exception &e)
		{
			throw std::runtime_error("While processing " + id + ": " + e.what());
		}
	}
	
	for (const supercharger &sc: created_superchargers)
	{
		std::cout << "\t<node version='" << sc.version << "' id='" << sc.id << "' lat='" << sc.lat << "' lon='" << sc.lon << "'>\n";
		for (const std::pair<std::string, std::string> &tag : sc.tags)
		{
			std::cout << "\t\t<tag k='" << escape_xml(tag.first) << "' v='" << escape_xml(tag.second) << "' />\n";
		}
		std::cout << "\t</node>\n";
		
	}
	
	std::cout << "</osm>\n";
}

