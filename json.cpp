#include <json/json.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <iomanip>

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
		string_replace(in, std::to_string(i) + ".30am", p+":30");
		string_replace(in, std::to_string(i) + ":30am", p+":30");
		string_replace(in, std::to_string(i) + "am", p+":00");
	}
	for (int i=1; i<=12; i++)
	{
		std::string p=pad2(i);
		string_replace(in, std::to_string(i) + ":30pm", p+":30");
		string_replace(in, std::to_string(i) + ".30pm", p+":30");
		string_replace(in, std::to_string(i) + "pm", p+":00");
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
	std::unordered_map<std::string, std::string> old_tags;
	
	std::string lat, lon, old_lat, old_lon;
	
	bool operator==(const supercharger &o) const
	{
		return id == o.id && version==o.version && tags == o.tags && lat == o.lat && lon==o.lon;
	}
};

struct osm_supercharger : public supercharger
{
	bool was_found=false;
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
	
	std::cout << std::setprecision(9);
	
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
		std::string osmid;
		std::string num_superchargers;
		std::string hours;
		std::string lat, lon;
	};

	const std::unordered_map<std::string, table_of_specials> specials
	{
		{ "564", { "", "2", "", "48.145652", "11.741631" } },
		{ "499", { "", "2", "24/7", "39.9498530", "116.2757593" } },
		{ "248", { "2953762536", "4", "24/7", "52.3625391", "7.2686097" } }, // emsbüren
		{ "246", { "", "6", "24/7", "59.3241785", "10.9545865" } }, // solli
		{ "318", { "3069937108", "6", "24/7", "30.2689809", "120.1392943" } }, // Hangzhou Dragon Hotel
		{ "290", { "3069967743", "8", "24/7", "30.3129368", "120.1594345" } }, // Hangzhou Xiacheng
		{ "280", { "3069987039", "4", "24/7", "39.7995368", "116.5122578" } }, // Beijing Yizhuang Harmony
		{ "342", { "3070065554", "2", "24/7", "39.9498530", "116.2757593" } }, // Beijing Haidian
		{ "357", { "3070069509", "2", "24/7", "30.5957295", "104.0642048" } }, // Chengdu Jinjiang
		{ "434", { "3070333381", "6", "24/7", "22.6218447", "114.0495169" } }, // Shenzhen
		{ "291", { "3070364986", "2", "24/7", "31.0718616", "121.4204477" } }, // Shanghai Minhang
		{ "320", { "3070377725", "2", "24/7", "30.2752468", "120.1328150" } },// Hangzhou Euro America Center
		{ "319", { "3070391169", "4", "24/7", "30.1827337", "120.2137973" } }, // Hangzhou Tesla Service Center
		{ "358", { "3092049532", "6", "24/7", "29.6106718", "106.5055304" } }, // Chongqing
		{ "356", { "3092055783", "4", "24/7", "30.6547232", "104.0642157" } }, // Chengdu Yanlord Landmark
		{ "345", { "3107025744", "2", "24/7", "49.2164971", "6.1709650" } }, // Metz
		{ "366", { "3105116844", "2", "24/7", "55.9456290", "-3.3653235" } } // Edinburgh Airport

		/*		{ "218", { "8", "24/7", "50.8169140", "8.0859139" } },
		{ "265", { "6", "24/7", "48.2725199", "12.5499163" } },
		{ "227", { "8", "24/7", "63.0114454", "7.9758376" } },
		
		{ "288", { "8", "24/7", "52.5325265","6.1579275" } },
		{ "313", { "2", "24/7", "51.5157251", "-0.1574010" } },
		{ "254", { "6", "24/7", "59.3814746", "13.4673721" } },
		{ "", { "", "24/7", "", "" } },
		{ "", { "", "24/7", "", "" } },
		{ "", { "", "24/7", "", "" } },
		{ "", { "", "24/7", "", "" } },
		{ "", { "", "24/7", "", "" } },
		{ "", { "", "24/7", "", "" } },*/
		
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
	std::vector<osm_supercharger> osm_superchargers;
	
	XmlTree tree(&xml);
	
	int64_t negative_ids=-1;

	tree.element("osm", [&] (const xmlnode &e)
	{
		tree.element("node", [&] (const xmlnode &e)
		{
			osm_supercharger sc;
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
			charger["id"].isString()
				? charger.get("id", "").asString()
				: std::to_string(charger["id"].asInt());
		
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
			const std::string lat
				= is_special != specials.end()
					? is_special->second.lat
					: charger["gps"].get("latitude", "").asString();
			const std::string lon
				= is_special != specials.end()
					? is_special->second.lon
					: charger["gps"].get("longitude", "").asString();
			
			std::list<supercharger>::iterator sc;
			
			if (is_special != specials.end() && !is_special->second.osmid.empty())
			{
				// search osm_superchargers by id
				for (osm_supercharger &maybesc : osm_superchargers)
				{
					if (std::to_string(maybesc.id) == is_special->second.osmid)
					{
						std::cerr << "Hardcoded connection of id " << id << " with node " << maybesc.id << std::endl;
						sc = created_superchargers.insert(created_superchargers.end(), maybesc);
						maybesc.was_found=true;
						break;
					}
				}
			}
			else
			{
				// search osm_superchargers by location,
				// find the supercharger closest to me
				osm_supercharger *closest=nullptr;
				double dclosest = std::numeric_limits<double>::max();
				for (osm_supercharger &maybesc : osm_superchargers)
				{
					double latdiff = std::fabs(std::stod(maybesc.lat) - std::stod(lat));
					double londiff = std::fabs(std::stod(maybesc.lon) - std::stod(lon));
					double dist = std::sqrt(latdiff*latdiff + londiff*londiff);
					if (dist < 0.002 && dist < dclosest)
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
					closest->was_found=true;
				}
				else
				{
					sc = created_superchargers.insert(created_superchargers.end(), supercharger());
					sc->id = negative_ids--;
				}
			}
			
			// the supercharger that is already in OSM (or one with a lot of empty strings)
			const supercharger old_sc = *sc;
			
			sc->old_tags = sc->tags;
			if (!sc->lat.empty()  && !sc->lon.empty())
			{
				sc->old_lat = sc->lat;
				sc->old_lon = sc->lon;
			}
			
			if (sc->lat.empty())
				sc->lat = lat;
			if (sc->lon.empty())
				sc->lon = lon;
			if (!hours.empty())
				sc->tags["opening_hours"] = hours;
			sc->tags["socket:tesla_supercharger"] = chargers;
			sc->tags["capacity"] = chargers;
			sc->tags["operator"] = "Tesla Motors Inc.";
			sc->tags["amenity"] = "charging_station";
			
			
			{
				const Json::Value& lt = charger["address"];
				if (sc->tags["addr:city"].empty())
					sc->tags["addr:city"] = lt.get("city", "").asString();
				if (sc->tags["addr:postcode"].empty() && !lt.get("zip", "").asString().empty())
					sc->tags["addr:postcode"] = lt.get("zip", "").asString();
				
				name = remove_suffix(name, ", " + lt.get("country","").asString());
				name = remove_suffix(name, ", " + lt.get("state","").asString());
				name = remove_suffix(name, ", UK");
			}
			
			sc->tags["name"] = "Tesla Supercharger " + name;
			if (sc->tags["name"]  == "Tesla Supercharger Beijing-Yizhuang")
				sc->tags["name"] = "Tesla Supercharger Beijing-Yizhuang Harmony";
			else if (sc->tags["name"]  == "Tesla Supercharger Hopewell Centre")
				sc->tags["name"] = "Tesla Supercharger Hopewell Centre Hong Kong";
			else if (sc->tags["name"]  == "Tesla Supercharger Hangzhou Huanglong")
				sc->tags["name"] = "Tesla Supercharger Hangzhou Dragon Hotel";
			else if (sc->tags["name"]  == "Tesla Supercharger Hangzhou-Binjiang")
				sc->tags["name"] = "Tesla Supercharger Hangzhou-Binjiang Tesla Service Center";
			else if (sc->tags["name"]  == "Tesla Supercharger Vienna")
			{
				sc->tags["name"] = "Tesla Supercharger Wien-Tech Park";
			}
			else if (sc->tags["name"]  == "Tesla Supercharger Busdorf")
				sc->tags["name"] = "Tesla Supercharger Busdorf Wikingerland";
			else if (sc->tags["name"]  == "Tesla Supercharger Berlin (SC)")
				sc->tags["name"] = "Tesla Supercharger Berlin";
			else if (sc->tags["name"]  == "Tesla Supercharger Zevenaar")
				sc->tags["addr:postcode"] = "6902PL";
			else if (sc->tags["name"]  == "Tesla Supercharger Royal Victoria Docks")
				sc->tags["name"] = "Tesla Supercharger London - The Royal Victoria Docks";
			else if (sc->tags["name"]  == "Tesla Supercharger Stockholm Infracity (SC)")
				sc->tags["name"] = "Tesla Supercharger Stockholm-Kanalvägen";
			else if (sc->tags["name"]  == "Tesla Supercharger Aix en Provence")
				sc->tags["name"] = "Tesla Supercharger Aix-en-Provence";
			

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
		// Dorno, Italy's supercharger is on two sides of the highway, and OSM's treatment is
		// better, so keep that as it is
		if (sc.id == 3081365539LL)
			continue;
			
		std::cout << "\t<node version='" << sc.version << "' id='" << sc.id << "' lat='" << sc.lat << "' lon='" << sc.lon << "'>\n";
		
		std::cout << "\t\t<!-- old lat=" << sc.old_lat << " lon=" << sc.old_lon << " -->\n";
		
		
		for (const std::pair<std::string, std::string> &tag : sc.tags)
		{
		
			std::cout << "\t\t<tag k='" << escape_xml(tag.first) << "' v='" << escape_xml(tag.second) << "' />";
			const auto o = sc.old_tags.find(tag.first);
			if (o != sc.old_tags.end() && o->second != tag.second)
			{
				std::cout << "<!-- old value='" << escape_xml(o->second) << "'-->";
			}
			std::cout << std::endl;
		}
		std::cout << "\t</node>\n";
	}
	
	std::cerr << "Preexisting superchargers not found in new data: " << std::endl;
	for (osm_supercharger &s : osm_superchargers)
	{
		if (s.was_found) continue;
		std::cerr << s.id << ": " << s.tags["name"] << std::endl;
	}
	
	std::cout << "</osm>\n";
}

