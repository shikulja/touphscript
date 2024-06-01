#include "touphScript.exe.h"
#include "lgp.h"
#include "lzs.h"
#include "field.h"
#include "ffstring.h"
#include "tutorial.h"
#include "world.h"
#include "scene.h"
#include "kernel.h"
#include "kernel2.h"
#include "ff7exe.h"
#include "windowBin.h"
#include "field_ids.h"
#include <array>
#include <exception>
#include <map>
#include <Shlwapi.h>

ts::config ts::conf;
std::string const ts::delimiter = "----------";
ts::byteVec ts::spacing;

int main(int argc, char* argv[]) {
	std::cout << '\n';
	char mode;
	
	if (argc < 2) {
		std::cout
			<< "touphScript v1.5.0 --- a FFVII text editor\n\n"
			<< "(d)ump or (e)ncode?\n";
		std::cin >> mode;
		
	} else
		mode = argv[1][0];
		
	try {

		ts::logger log;
		ts::conf = ts::config("touphScript.ini", ts::getFFPath());

		
		if (!ts::conf.path("char_file").empty())
			ts::ffString::loadCharmap(ts::conf.path("char_file").c_str());

		ts::spacing = ts::getSpacingTable();

		if (mode == 'd') {
			ts::makeDir(ts::conf.path("text"));
			
			if (ts::conf["dump_flevel"]) {
				std::cout << "Dumping flevel.lgp\n";
				ts::dumpFlevel();
			}
			
			if (ts::conf["dump_world"]) {
				std::cout << "\nDumping world_us.lgp\n";
				ts::dumpWorld();
			}

			if (ts::conf["dump_scene"]) {
				std::cout << "Dumping scene.bin\n";
				ts::dumpScene();
			}

			if (ts::conf["dump_kernel"]) {
				std::cout << "Dumping KERNEL.BIN\n";
				ts::dumpKernel();
			}

			if (ts::conf["dump_kernel2"]) {
				std::cout << "Dumping kernel2.bin\n";
				ts::dumpKernel2();
			}

			if (ts::conf["dump_exe"]) {
				std::cout << "Dumping ff7.exe\n";
				ts::dumpExe();
			}

		}	else if (mode == 'e') {
			std::cout << "Encoding flevel.lgp\n";
			ts::encodeFlevel();
			
			std::cout << "\nEncoding world_us.lgp\n";
			ts::encodeWorld();
			
			std::cout << "Encoding scene.bin\n";
			ts::encodeScene();
			
			std::cout << "Encoding KERNEL.BIN\n";
			ts::encodeKernel();
			
			std::cout << "Encoding kernel2.bin\n";
			ts::encodeKernel2();
			
			std::cout << "Encoding ff7.exe\n";
			ts::encodeExe();
			
		} else
			throw ts::runErr("Unknown option");
			
	} catch (std::ios::failure const& e) {
		std::cout << "File read / write error, check permissions and read-only flag\n";
		return -1;
	} catch (std::exception const& e) {
		std::cout << "Fatal error: " << e.what() << '\n';
		return -1;
	}
	return 0;
}

namespace ts {

void dumpFlevel() {
	try {
	
		ilgp lin(conf.path("flevel"));
		cout << "    / " << lin.size();

		for (u16 i = 0; i < lin.size(); ++i) {
		
			string const name(lin.getName(i));
			cout << '\r' << i + 1;		
			
			try {
			
				if (tutorial::isTut(name)) {
					byteVec t = lin[i];
					vecPatch(t);
					dumpTutorial(name, t);
					continue;
				}
				
				if (!field::isField(name)) continue;
				
				byteVec2d f(field::toField(lzs::decompress(lin[i])));
				vecPatch(f[0]);
				
				script::section0 s(f[0]);
				if (conf["script_strip"]) script::removeUnused r(s);
				if (conf["window_patch"]) script::windowPatch w(s);
				
				writeText(s, name);
				
			} catch (std::exception const& e) {
				clog << "Error in " << name << ": " << e.what() << '\n';
			}
		}
	} catch (std::ios_base::failure const& e) {
		clog << "Cannot read / error reading flevel\n";
	}
}

void writeText(script::section0& s, string const& name) {
	byteVec2d const& text = s.getText();
	script::getWin g(s);

	char id[5]{};
	sprintf(id, "%04d", field_ids.at(name));
	
	ofstream out(conf.path("text") + id + "_" + name + ".txt");
	out.exceptions(out.failbit | out.badbit | out.eofbit);
	addBOM(out);

	for (u16 t = 0; t < text.size(); ++t) {
	
		if (!g.windows.count(t) && conf["text_strip"]) continue;
		
		auto const& windows = g.windows[t];
		
		for (auto& win : windows.p) {
		
			std::stringstream params("#", params.in | params.out | params.ate);
			std::stringstream vals;
			
			bool special = win.sx != g.null && (conf["dump_sp"] || text[t].size() > 1);
			
			if (conf["dump_x"] || special) {
				params << "x";
				vals << win.x << ' ';
			}
			if (conf["dump_y"] || special) {
				params << "y";
				vals << win.y << ' ';
			}
			if (conf["dump_w"] || special) {
				params << "w";
				vals << win.w << ' ';
			}
			if (conf["dump_h"] || special) {
				params << "h";
				vals << win.h << ' ';
			}
			if (special) {
				params << "lt";
				vals << win.sx << ' ' << win.sh;
			}
			if (vals.tellp()) out << params.str() << ' ' << vals.str() << '\n';
		}
		/*if (conf["dump_question"] && windows.first != g.null)
			out << "#o " << windows.first << ' ' << windows.last << '\n';*/

		out << ffString::toString(text[t]) << '\n'
				<< delimiter << delimiter << delimiter << '\n';
	}
}

void dumpTutorial(string const& name, byteVec const& buf) {
	try {
		string filename;

		if (name == "mds7_w.tut")
			filename = "0149_mds7_w2.tut.txt";
		else if (name == "mds7pb.tut")
			filename = "0154_mds7pb_1.tut.txt";
		else
			filename = "0378_junpb_2.tut.txt";

		sVec const str = tutorial::toString(buf);
		ofstream out(conf.path("text") + filename);
		addBOM(out);
		
		for (string const& s : str)
			out << s << '\n'
					<< delimiter << delimiter << delimiter << '\n';
					
	} catch (runErr const& e) {
		clog << "Couldn't dump tutorial " << name << ": " << e.what() << '\n';
	}
}

void dumpWorld() {
	try {
		ilgp lin(conf.path("world"));
		
		byteVec2d text(world::toText(lin["mes"]));
		world::event globe(lin["wm0.ev"]);
		world::event glacier(lin["wm3.ev"]);

		vector<world::event::window> windows(globe.getWindows());
		vector<world::event::window> const gwindows(glacier.getWindows());
		windows.insert(windows.end(), gwindows.begin(), gwindows.end());

		ofstream out(conf.path("text") + "World.txt");
		out.exceptions(out.failbit | out.badbit | out.eofbit);
		addBOM(out);

		for (u16 t = 0; t < text.size(); ++t) {
		
			auto win = find_if(windows.cbegin(), windows.cend(),
			[t](world::event::window const& w) {
				return w.id == t;
			});
			
			if (win < windows.cend()) {
			
				std::stringstream params("#", params.in | params.out | params.ate);
				std::stringstream vals;
				
				if (conf["dump_x"]) {
					params << 'x';
					vals << win->x << ' ';
				}
				if (conf["dump_y"]) {
					params << 'y';
					vals << win->y << ' ';
				}
				if (conf["dump_w"]) {
					params << 'w';
					vals << win->w << ' ';
				}
				if (conf["dump_h"]) {
					params << 'h';
					vals << win->h << ' ';
				}
				/*if (conf["dump_question"] && win->first != globe.null) {
					params << 'o';
					vals <<  win->first << ' ' << win->last;
				}*/
				if (vals.tellp())
					out << params.str() << ' ' << vals.str() << '\n';
			}
			out << ffString::toString(text[t]) << '\n'
				<< delimiter << delimiter << delimiter << '\n';
		}
	} catch (runErr const& e) {
		clog << "Couldn't dump world_us: " << e.what() << '\n';
	}
}

void dumpScene() {
	try {
		byteVec2d const s(scene::toScene(conf.path("scene")));
		ofstream o(conf.path("text") + "Scene.txt");
		addBOM(o);

		for (u16 i = 0; i < s.size(); ++i) {
		
			scene::sceneFile const f(s[i]);			
			byteVec2d const text = f.getText();
			
			o << "# File " << i << "\n\n";
			
			for (u8 j = 0; j < text.size(); ++j) {	
			
				if (j == 3 || j == 35) o << '\n';
				
				o << ffString::toSceneString(text[j]) << '\n';
			}
			o << delimiter << delimiter << delimiter << '\n';
		}
	} catch (runErr const& e) {
		clog << "Couldn't dump scene: " << e.what() << '\n';
	}
}

void dumpKernel() {
	try {
		ofstream o(conf.path("text") + "Kernel.txt");
		o.exceptions(o.failbit | o.badbit | o.eofbit);
		addBOM(o);

		kernel const k(conf.path("kernel"));
		byteVec2d const text = k.getText();

		for (byteVec const& t : text)
			o << ffString::toString(t) << '\n';
				
	} catch (runErr const& e) {
		clog << "Couldn't dump kernel: " << e.what() << '\n';
	}
}

void dumpKernel2() {
	try {
		ofstream o(conf.path("text") + "Kernel2.txt");
		o.exceptions(o.failbit | o.badbit | o.eofbit);
		addBOM(o);

		vector<byteVec2d> const text(kernel2::toText(conf.path("kernel2")));

		for (u16 i = 0; i < text.size(); ++i) {
			o << kernel2::secNames[i] << "\n\n";
			
			for (byteVec const& t : text[i])
				o << ffString::toKernel2String(t) + '\n';
				
			o << delimiter << delimiter << delimiter << '\n';
		}
	} catch (runErr const& e) {
		clog << "Couldn't dump kernel2: " << e.what() << '\n';
	}
}

void dumpExe() {
	try {
		ofstream o(conf.path("text") + "Exe.txt");
		o.exceptions(o.failbit | o.badbit | o.eofbit);
		addBOM(o);

		ff7exe f(conf.path("exe"));

		sVec const text = f.getText();
		
		for (string const& t : text)
			o << t << '\n';
			
	} catch (runErr const& e) {
		clog << "Couldn't dump exe: " << e.what() << '\n';
	}
}

void encodeFlevel() {
	try {
		tmpFile ltemp;
		
		ilgp lin(conf.path("flevel"));
		olgp lout(ltemp, lin.size());
		
		cout << "    / " << lin.size();

		for (u16 i = 0; i < lin.size(); ++i) {
		
			byteVec file(lin[i]);
			string const name = lin.getName(i);
			
			cout << '\r' << i + 1;
			
			try {
			
				if (tutorial::isTut(name))
					encodeTutorial(name, file);
					
				if (field::isField(name)) {
					byteVec2d f(field::toField(lzs::decompress(file)));
					vecPatch(f[0]);
					
					script::section0 s(f[0]);
					if (conf["script_strip"]) script::removeUnused r(s);
					if (conf["window_patch"]) script::windowPatch w(s);
					if (conf["text_strip"]) script::textStrip t(s);
					
					readText(s, name);
					
					f[0] = s.save();
					file = lzs::compress(field::toVec(f));
				}
			} catch (runErr const& e) {
				clog << "Error in " << name << ": " << e.what() << '\n';
			}
			lout.write(file, name);
		}
		lout.save().close();
		lin.close();
		copyFile(ltemp, conf.path("flevel"));
		
	} catch (std::ios_base::failure const& e) {
		clog << "flevel read/write failure: " << e.what() << '\n';
	}
}

void readText(script::section0& s, string const& name) {

	char fid[5]{};
	sprintf(fid, "%04d", field_ids.at(name));
	ifstream in(conf.path("text") + fid + "_" + name + ".txt");
	
	if (!in.is_open() && spacing.size()) {
		script::autosize a(s, spacing);
		return;
	}
	removeBOM(in);

	sVec text(1);
	std::map<u8, script::setWin::window> windows;
	u16 id = 0;
	string line;
	
	while (getline(in, line)) {
	
		string& txt = text.back();
		
		if (line[0] == '#') {
		
			script::setWin::window& win = windows[id];
			std::istringstream winl(string(line.cbegin() + 1, line.cend()));
			winl.exceptions(winl.failbit | winl.badbit);
			string params;
			
			try {
			
				winl >> params;
				
				if (params.find_first_of("xycwhlt") != params.npos)
					win.p.push_back(script::setWin::params());
					
				script::setWin::params& wprms = win.p.back();
				
				for (char const c : params)
					switch (c) {
					case 'x':
						winl >> wprms.x;
						break;
					case 'y':
						winl >> wprms.y;
						break;
					case 'c':
						wprms.center = true;
						break;
					case 'w':
						winl >> wprms.w;
						break;
					case 'h':
						winl >> wprms.h;
						break;
					case 'e':
						winl >> wprms.e;
						break;
					case 'l':
						winl >> wprms.sx;
						break;
					case 't':
						winl >> wprms.sh;
						break;
					/*case 'o':
						winl >> win.first >> win.last;
						break;*/
					case 'n':
					case 'm':
					case 'i':
						break;
					default:
						throw std::ios_base::failure("unknown param");
					}
			} catch (std::ios_base::failure const& e) {
                          std::clog 	<< "Error reading params in " << name
							<< ".txt entry " << id << '\n';
			}
			
		} else if (line.find(delimiter+delimiter) != line.npos) {
		
			if (txt.size()) txt.erase(txt.rfind('\n'));
			
			text.push_back(string());
			++id;
			
		} else
			txt += line + '\n';
	}
	text.pop_back();
	
	if (text.size() != s.getText().size())
          throw std::runtime_error("Incorrect number of entries");

	for (u16 i = 0; i < text.size(); ++i)
		if (text[i].size() || !windows[i].p.empty()) {
			try {
			
				s.setText(i, ffString::toFFString(text[i]));
				
			} catch (runErr const& e) {
          std::clog << name << ": Error in entry n " << i << ": " << e.what() << '\n';
			}
		}
		
	if (spacing.size()) script::autosize a(s, spacing);
	
	script::setWin set(s, windows);
}

void encodeTutorial(string const& name, byteVec& buf) {
	try {
		
		string filename;

		if (name == "mds7_w.tut")
			filename = "0149_mds7_w2.tut.txt";
		else if (name == "mds7pb.tut")
			filename = "0154_mds7pb_1.tut.txt";
		else
			filename = "0378_junpb_2.tut.txt";

		ifstream in(conf.path("text") + filename);
		
		if (!in.is_open()) return;
		
		removeBOM(in);

		vector<sVec> out(1);
		string line;
		
		while (getline(in, line)) {
		
			if (line.find(delimiter) != line.npos) {
				out.push_back(sVec());
				continue;
			}
			out.back().push_back(line);
		}
		out.pop_back();
		buf = tutorial::toTutorial(out);
		
	} catch (runErr const& e) {
		clog << "Couldn't encode tutorial: " << e.what() << '\n';
	}
}

void encodeWorld() {
	try {
		ifstream in(conf.path("text") + "World.txt");
		ilgp lin(conf.path("world"));
		
		world::event globe(lin["wm0.ev"]);
		world::event glacier(lin["wm3.ev"]);
		byteVec2d text(world::toText(lin["mes"]));
		
		if (in.is_open()) {
		
			removeBOM(in);
			vector<world::event::window> windows;
			sVec newText(1);
			u16 id = 0;
			string line;
			
			while (getline(in, line)) {
			
				string& txt = newText.back();
				if (line[0] == '#') {
				
					std::istringstream winl(string(line.cbegin() + 1, line.cend()));
					winl.exceptions(winl.failbit | winl.badbit);
					string params;
					
					try {
					
						winl >> params;
						windows.push_back(world::event::window());
						world::event::window& win = windows.back();
						win.id = id;
						
						for (char const c : params)
							switch (c) {
							case 'x':
								winl >> win.x;
								break;
							case 'y':
								winl >> win.y;
								break;
							case 'w':
								winl >> win.w;
								break;
							case 'h':
								winl >> win.h;
								break;
							case 'o':
								winl >> win.first >> win.last;
								break;
              case 'c':
                win.center = true;
                break;
							default:
								throw std::ios_base::failure("unknown param");
							}
							
					} catch (std::ios_base::failure const& e) {
						clog 	<< "Error reading params in World.txt entry " << id << '\n';
					}
					
				} else if (line.find(delimiter) != line.npos) {
					if (txt.size()) txt.erase(txt.rfind('\n'));
					newText.push_back(string());
					++id;
					
				} else
					txt += line + '\n';
			}
			newText.pop_back();
			
			if (text.size() != newText.size())
				throw runErr("Incorrect number of text entries for world text");
				
			for (u8 i = 0; i < newText.size(); ++i)
				if (newText[i].size())
					text[i] = ffString::toFFString(newText[i]);

			if (spacing.size()) {
				globe.autosize(text, spacing);
				glacier.autosize(text, spacing);
			}
			globe.setWindows(windows);
			glacier.setWindows(windows);
			
		} else {
			if (spacing.size()) {
				globe.autosize(text, spacing);
				glacier.autosize(text, spacing);
			}
		}

		tmpFile ltemp;
		olgp lout(ltemp, lin.size());
		
		for (u16 i = 0; i < lin.size(); ++i) {
		
			string const name(lin.getName(i));
			
			if (name == "mes")
				lout.write(world::toMes(text), name);
			else if (name == "wm0.ev")
				lout.write(globe.save(), name);
			else if (name == "wm3.ev")
				lout.write(glacier.save(), name);
			else
				lout.write(lin[i], name);
		}
		
		lout.save().close();
		lin.close();
		copyFile(ltemp, conf.path("world"));
		
	} catch (runErr const& e) {
		clog << "Couldn't encode world_us.lgp: " << e.what() << '\n';
	}
}

void encodeScene() {
	try {
	
		ifstream in(conf.path("text") + "Scene.txt");
		if (!in.is_open()) return;
		removeBOM(in);

		vector<byteVec2d> textIn;
		textIn.push_back(byteVec2d());
		string line;
		
		while (getline(in, line)) {
			for (u8 lineno = 0; lineno < 38; ++lineno) {
				if (lineno < 2 || lineno == 5)
					getline(in, line);
					
				else {
					textIn.back().push_back(ffString::toSceneVec(line));
					getline(in, line);
				}
			}
			if (line.find(delimiter) == line.npos) {
			
				getline(in, line);
				
				while (line.find(delimiter) == line.npos) {
					textIn.back().push_back(ffString::toSceneVec(line));
					getline(in, line);
				}
			}
			textIn.push_back(byteVec2d());
		}
		in.close();
		textIn.resize(256);

		byteVec2d s(scene::toScene(conf.path("scene")));

		for (u16 i = 0; i < s.size(); ++i) {
		
			scene::sceneFile f(s[i]);
			byteVec2d text = f.getText();
			
			if (text.size() != textIn[i].size())
				throw runErr("Incorrect number of entries");
				
			for (u8 j = 0; j < text.size(); ++j)
				if (textIn[i][j].size())
					text[j] = textIn[i][j];

			f.setText(text);
			s[i] = f.save();
		}
		tmpFile stemp;
		ofstream out(stemp, out.out | out.binary);
		write(out, scene::toVec(s));
		out.close();
		copyFile(stemp, conf.path("scene"));

		kernel kern(conf.path("kernel"));
		kern.updateBattleTbl(s);

		tmpFile ktemp;
		out.open(ktemp, out.out | out.binary);
		write(out, kern.save());
		out.close();
		copyFile(ktemp, conf.path("kernel"));
		
	} catch (runErr const& e) {
		clog << "Couldn't encode scene: " << e.what() << '\n';
	}
}

void encodeKernel() {
	try {
		ifstream in(conf.path("text") + "Kernel.txt");
		if (!in.is_open()) return;
		removeBOM(in);

		byteVec2d text;
		string line;
		
		while (getline(in, line)) {			
			text.push_back(ffString::toFFString(line));
		}
		
		if (text.size() != 9) throw runErr("Incorrect number of entries");
		
		kernel k(conf.path("kernel"));
		k.setText(text);
		tmpFile ktemp;
		ofstream o(ktemp, o.out | o.binary);
		write(o, k.save());
		o.close();
		copyFile(ktemp, conf.path("kernel"));
		
	} catch (runErr const& e) {
		clog << "Couldn't encode kernel: " << e.what() << '\n';
	}
}

void encodeKernel2() {
	try {
		ifstream in(conf.path("text") + "Kernel2.txt");
		
		if (!in.is_open()) return;
		
		removeBOM(in);

		vector<byteVec2d> text = kernel2::toText(conf.path("kernel2"));
		u8 sec = 0;
		u16 l = 0;
		string line;
		
		while (getline(in, line)) {
			if (line[0] == '#') {
				getline(in, line);
				getline(in, line);
			}
			if (line.find(delimiter) != line.npos) {
				++sec;
				l = 0;
				continue;
			}
			if (line.size())
				text.at(sec).at(l) = ffString::toKernel2Vec(line);
			++l;
		}
		
		tmpFile ktemp;
		ofstream o(ktemp, o.out | o.binary);
		write(o, kernel2::toKernel2(text));
		o.close();
		copyFile(ktemp, conf.path("kernel2"));
		
		// update item ordering table
		sVec items;
		
		for (auto pos = text.cbegin() + 10; pos != text.cbegin() + 14; ++pos)
			for (byteVec const& b : *pos)
				items.push_back(ffString::toKernel2String(b));
		
		if ((items.size()) != 320) throw runErr("Incorrect item table length");
		
		try {
			ff7exe f(conf.path("exe"));
			f.updateItemOrder(items);
		} catch (...) {
			clog << "Could not update item order table in ff7.exe\n";
		}
		
	} catch (runErr const& e) {
		clog << "Couldn't encode kernel2: " << e.what() << '\n';
	}
}

void encodeExe() {
	try {
		ifstream in(conf.path("text") + "Exe.txt");
		
		if (!in.is_open()) return;
		
		removeBOM(in);

		sVec text;
		string line;
		
		while (getline(in, line))
			text.push_back(line);
			
		ff7exe f(conf.path("exe"));
		f.setText(text);		
		
	} catch (runErr const& e) {
		clog << "Couldn't encode exe: " << e.what() << '\n';
	}
}

void vecPatch (byteVec& vec) {

	DWORD hashVal = 0;
	HashData(vec.data(), vec.size(),
		static_cast<LPBYTE>(static_cast<void*>(&hashVal)), sizeof hashVal);	
		
	switch (hashVal) {
	
	// Spanish version
	// Fix AKAO pointers
	case loslake2:
		vec[0x68] += 1;
		break;
	case blue_1:
		vec[0x48] += 1;
		break;
	// text bug
	case shpin_22:
		vec[0x336] = 0xFF;
		break;
		
	// Italian Version
	// Fix tutorial offset
	case junpb_tut:
		vec[0x10] = 0x3A;
		break;

	default: break;
	}
}

inline void insert(byteVec& s, u32 const pos, byteVec&& ins) {	
	u8 op = ins[0];
	
	if (script::isJmp(op))
		setJmptmp(ins[0]);
	
	vec16 labels = script::getLabels(s);
	s.insert(s.begin() + pos, ins.cbegin(), ins.cend());		
	
	for (u16& label : labels)
		if (label >= pos)
			label += ins.size();
			
	script::setLabels(s, labels);
	
	s[pos] = op;
}

inline void erase(byteVec& s, u32 const pos, script::opcodes const op) {	
	if (script::isJmp(op))
		setJmptmp(s[pos]);
		
	vec16 labels = script::getLabels(s);
	s.erase(s.begin() + pos, s.begin() + pos + script::opSizes[op]);	
	
	for (u16& label : labels)
		if (label >= pos)
			label -= script::opSizes[op];
			
	script::setLabels(s, labels);
}

inline void setJmptmp(u8& op) {
	/* If jump, change op to non-jump of same size
		to avoid interfering with jump recalculations */
	switch (script::opSizes[op]) {
	case 2:
		op = script::retto;
		break;
	case 3:
		op = script::req;
		break;
	case 4:
		op = script::ptura;
		break;
	case 6:
		op = script::sptye;
		break;
	case 7:
		op = script::nfade;
		break;
	case 8:
		op = script::blink;
		break;
	case 9:
		op = script::bgmovie;			
		break;
	}
}

byteVec getSpacingTable() {
	byteVec spacing;

	try {
		if (ts::conf["font_spacing"])
			spacing = conf.getSpacing();
		else {
			windowBin const w(conf.path("window"));
			spacing = w.getSpacingTable();
		}
			
		spacing.push_back(0);
		
		if (ts::conf["padding"])
			spacing.push_back(ts::conf.padding);
		else
			spacing.push_back(0x10);

		try {
			ff7exe const e(conf.path("exe"));
			if (!ts::conf["max"])
				spacing[256] = e.maxSp;			
			if (!ts::conf["choice"])
				spacing[0xE0] = e.choiceSp;
			if (!ts::conf["tab"])
				spacing[0xE1] = e.tabSp;
		} catch (...) {
			clog << "FF7.exe not found, using spacing values in config file.\n";
		}

		if (ts::conf["max"])
			spacing[256] = ts::conf.max;
		if (ts::conf["choice"])
			spacing[0xE0] = ts::conf.choice;
		if (ts::conf["tab"])
			spacing[0xE1] = ts::conf.tab;
		
		spacing[0xE2] = spacing.at(0x00) + spacing.at(0x0C);
		spacing[0xE3] = spacing.at(0x0E) + spacing.at(0x02);
		spacing[0xE4] = spacing.at(0xA9) + spacing.at(0x02);
		
		for (u8 i = 0; i < conf.names().size(); ++i)
			spacing[0xEA + i] =
				ffString::width(ffString::toFFString(conf.names()[i]), spacing) - spacing.back();

		auto const max = max_element(spacing.cbegin() + 0xEA, spacing.cbegin() + 0xF3);
		
		for (u8 i = 0; i < 3; ++i)
			spacing[0xF3 + i] = *max; // party spacing

	} catch (runErr const& e) {
		clog << "Error creating spacing table\n";
	}

	return spacing;
}



std::string const getFFPath() {

  HKEY key;
  std::array<unsigned char, 260> val{};
  DWORD sz = MAX_PATH;

  static const std::array<std::string, 3> regStrings{
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 39140",
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{141B8BA9-BFFD-4635-AF64-078E31010EC3}_is1",
    "SOFTWARE\\Square Soft, Inc.\\Final Fantasy VII"
  };

  for (auto& node : regStrings)
    if (RegOpenKeyEx(
      reinterpret_cast<HKEY>(0x80000002), // HKLM
      node.c_str(), 0, KEY_QUERY_VALUE, &key
    ) == ERROR_SUCCESS) {
      std::clog << "Registry values found:" << node << "\n";

      if (!RegQueryValueEx(key, "AppPath", nullptr, nullptr, val.data(), &sz))
        return reinterpret_cast<char const*>(val.data());
    }
  std::clog << "No FFVII installation found\n"; 
  return ".\\";
}



inline void copyFile(string const& from, string const& to) {
	if (!CopyFile(from.c_str(), to.c_str(), false)) {
		throw runErr("Couldn't copy file " + from + " to " + to);
	}
}

inline string const getTempFile() {
	vector<TCHAR> tmpPath(MAX_PATH);
	vector<TCHAR> tmpFile(MAX_PATH);
	
	if (!GetTempPath(MAX_PATH, tmpPath.data())
	|| !GetTempFileName(tmpPath.data(), "ts", 0, tmpFile.data()))
		throw runErr("Cannot get temp filename");
		
	return tmpFile.data();
}

inline void makeDir(string const& dir) {
	if (!CreateDirectory(dir.c_str(), nullptr))
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			throw runErr("Cannot create directory " + dir);
}

}

