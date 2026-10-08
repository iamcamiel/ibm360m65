// ALD Compiler
// Copyright(C) 2024 Camiel Vanderhoeven
//
// This program is free software : you can redistribute it and /or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.If not, see < http://www.gnu.org/licenses/>.

#include "ald_compiler.h"

static int numtemps = 0;
static int issuedtemps = 0;

static std::string put_signal_p(std::string sec, SIG1 s) {
	std::ostringstream oss;
	SIGINFO2 si;
	if (s.second.prefix == "")
		s.second.prefix = sec;
	if (s.second.prefix == sec)
		si.external_ref = false;
	else
		si.external_ref = true;
	if (sections.count(s.second.prefix)) {
		if (sections[s.second.prefix].signals.count(s.first)) {
			si = sections[s.second.prefix].signals[s.first];
		}
	}
	if (s.second.prefix!=sec && s.second.prefix != "*")
		oss << s.second.prefix << "_";

	oss << safename_signal(sec, s.first, true);

	return oss.str();
}

static std::string put_signal(std::string sec, SIG1 s) {
	std::ostringstream oss;
	oss << put_signal_p(sec, s);
	if (s.second.dimensioned) {
		if (s.second.length > 1) {
			if (s.second.order < 0)
				oss << "(" << s.second.high << " downto " << s.second.low << ")";
			else
				oss << "(" << s.second.low << " to " << s.second.high << ")";
		}
		else
			oss << "(" << s.second.low << ")";
	}
	return oss.str();
}

static std::string put_signal(std::string sec, SIG1 s, int i) {
	std::ostringstream oss;
	oss << put_signal_p(sec, s);

	if (s.second.dimensioned) {
		if (s.second.length > 1) {
			if (s.second.order < 0)
				oss << "(" << s.second.high - i << ")";
			else
				oss << "(" << s.second.low + i << ")";
		}
		else
			oss << "(" << s.second.low << ")";
	}
	return oss.str();
}

static void process(std::string sec, std::vector<LINE> lines) {
	bool in_temp = false;
	int temp_ident = 0;
	std::string rest;
	int sig_kind = 1;
	int all_temps = 0;
	int cur_temps = 0;

	if ((rest = getrest(lines[0].line, "SECTION")) != "")
		return;
	if ((rest = getrest(lines[0].line, ";")) != "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after comment.", lines);
		sections[sec].vhd_file << "-- " << rest << "\n";
	}
	else if ((rest = getrest(lines[0].line, "#")) != "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after comment.", lines);
		sections[sec].vhd_file << "-- " << rest << "\n";
	}
	else if (lines[0].line == "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after blank line.", lines);
		sections[sec].vhd_file << "\n";
	}
	else {
		if (lines.size() == 1) error_exit(sec, "Multi-line statement expected.", lines);
		if (lines[0].line == "DECODE") {
			if (lines.size() < 3 || lines.size() > 4) error_exit(sec, "Invalid number of arguments to decoder.", lines);
			SIG1 put = get_signal(sec, lines[1].line,true);

			if (sections[sec].signals.count(put.first) && sections[sec].signals[put.first].eliminated)
				return;
			SIG1 pos = get_signal(sec, lines[2].line,false);
			SIG1 gate = std::make_pair(std::string(""), SIGINFO1());
			if (lines.size() > 3) {
				gate = get_signal(sec, lines[3].line,false);
				if (gate.second.length != 1) error_exit(sec, "Gate dimension error.", lines);
			}
			if (put.second.length != 1 << pos.second.length) error_exit(sec, "Decoder dimension error.", lines);
			if (pos.second.order != 1 || put.second.order != 1) error_exit(sec, "Decoder order error.", lines);
			if (put.second.low != 0) error_exit(sec, "Decoder offset error.", lines);
			for (int i = 0; i < put.second.length; i++) {
				std::vector<LINE> lvec;
				LINE ll;
				ll.ident = 0;
				ll.line = lines[1].line.substr(0, lines[1].line.find_first_of("[")) + "[" + std::to_string(i) + "]";
				ll.is_signal = true;
				ll.signal = get_signal(sec, ll.line,true);
				lvec.push_back(ll);
				ll.ident = 1;
				ll.line = "AND";
				ll.is_signal = false;
				lvec.push_back(ll);
				ll.ident = 2;
				ll.is_signal = true;
				if (gate.first != "") {
					ll.line = lines[3].line;
					ll.signal = get_signal(sec, ll.line,false);
					lvec.push_back(ll);
				}
				for (int j = 0; j < pos.second.length; j++) {
					if ((i >> j) & 1) {
						ll.ident = 2;
					}
					else {
						ll.ident = 2;
						ll.line = "NOT";
						ll.is_signal = false;
						lvec.push_back(ll);
						ll.ident = 3;
					}
					ll.is_signal = true;
					ll.line = lines[2].line.substr(0, lines[2].line.find_first_of("[")) + "[" + std::to_string(pos.second.high - j) + "](" + pos.second.prefix + ")";
					ll.signal = get_signal(sec, ll.line,false);
					lvec.push_back(ll);
				}
				process(sec, lvec);
			}
			return;
		}
		SIG1 output;
		if (lines[0].line == "SPECIAL")
			return;
		output = get_signal(sec, lines[0].line, true);
		if (output.second.prefix != sec) error_exit(sec, "Can't have a section name in an output signal name.", lines);
		if (sections[sec].signals.count(output.first)) {
			if (sections[sec].signals[output.first].eliminated || sections[sec].signals[output.first].aliased)
				return;
			SIGINFO1 existing = sections[sec].signals[output.first].basics;
			if (existing.dimensioned != output.second.dimensioned)
				error_exit(sec, "Signal <" + output.first + "> appears both dimensioned and undimensioned.", lines);
			if (existing.dimensioned) {
				if (existing.order != 0) {
					if (output.second.order == 0)
						output.second.order = existing.order;
					if (output.second.order != existing.order)
						error_exit(sec, "Signal <" + output.first + "> appears with different orderings.", lines);
				}
				if (existing.low < output.second.low) output.second.low = existing.low;
				if (existing.high > output.second.high) output.second.high = existing.high;
				output.second.length = output.second.high - output.second.low + 1;
				sections[sec].signals[output.first].basics = output.second;
				output = get_signal(sec, lines[0].line,true);
			}
		}
		else {
			sections[sec].signals[output.first];
			sections[sec].signals[output.first].basics = output.second;
		}
		for (LINE l : lines) {
			if (l.is_signal) {
				if (l.signal.second.length != output.second.length && l.signal.second.length != 1 && output.second.length != 1)
					error_exit(sec, "dimension mismatch in <" + l.line + ">.", lines);
			}
		}
		std::map<int, std::string> stack;
		for (int i = 0; i < output.second.length; i++) {
			int last_ident = 0;
			int prev_ident = 0;
			cur_temps = 0;
			for (LINE l : lines) {
				if (in_temp) {
					if (l.ident > temp_ident)
continue;
					else
					in_temp = false;
				}
				if (l.ident == 0) {
					if (l.is_signal)
						sections[sec].vhd_file << "      " + put_signal(sec, output, i) + " <= ";
					continue;
				}
				last_ident = prev_ident;
				while (l.ident <= last_ident) {
					if (stack[last_ident] == "ORNOT" || stack[last_ident] == "ANDNOT") {
						sections[sec].vhd_file << "))";
					} else if (stack[last_ident] != "NOT" && stack[last_ident] != "" && stack[last_ident] != "TEMP") {
						sections[sec].vhd_file << ")";
					}
					stack[last_ident] = "";
					last_ident--;
				}
				if (l.ident <= prev_ident) {
					if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
						sections[sec].vhd_file << " and ";
					else if (stack[last_ident] == "ANDNOT")
						sections[sec].vhd_file << ") and not (";
					else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
						sections[sec].vhd_file << " or ";
					else if (stack[last_ident] == "ORNOT")
						sections[sec].vhd_file << ") or not (";
					else if (stack[last_ident] == "XOR")
						sections[sec].vhd_file << " xor ";
					else if (is_known_word(stack[last_ident]))
						error_exit(sec, "syntax error in logic expression; no second term expected", lines);
					else
						sections[sec].vhd_file << " , ";
				}
				if (l.is_signal) {
					if (l.signal.first.substr(1) == "temp") {
						stack[l.ident] = "TEMP";
						in_temp = true;
						temp_ident = l.ident;
						if (i == 0) {
							numtemps++;
							all_temps++;
						}
						cur_temps++;
						l.line = l.line.substr(0, 5) + std::to_string(numtemps - all_temps + cur_temps) + l.line.substr(5);
						l.signal.first += std::to_string(numtemps - all_temps + cur_temps);
					}
					else {
						l.signal = get_final_signal(sec, l.line);
					}
					if (l.signal.second.length > 1 && output.second.length == 1) {
						if (!is_known_word(stack[last_ident])) {
							sections[sec].vhd_file << put_signal(sec, l.signal);
						}
						else {
							sections[sec].vhd_file << put_signal(sec, l.signal, 0);
							for (int ii = 1; ii < l.signal.second.length; ii++) {
								if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
									sections[sec].vhd_file << " and ";
								else if (stack[last_ident] == "ANDNOT")
									sections[sec].vhd_file << ") and not (";
								else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
									sections[sec].vhd_file << " or ";
								else if (stack[last_ident] == "ORNOT")
									sections[sec].vhd_file << ") or not (";
								else if (stack[last_ident] == "XOR")
									sections[sec].vhd_file << " xor ";
								else
									error_exit(sec, "syntax error in logic expression; no second term expected", lines);
								sections[sec].vhd_file << put_signal(sec, l.signal, ii);
							}
						}
					}
					else {
						sections[sec].vhd_file << put_signal(sec, l.signal, i);
					}
				}
				else {
					stack[l.ident] = l.line;
					if (l.line == "AND" || l.line == "OR" || l.line == "XOR")
						sections[sec].vhd_file << "(";
					else if (l.line == "ANDNOT" || l.line == "ORNOT")
						sections[sec].vhd_file << "( not (";
					else if (l.line == "NAND" || l.line == "NOR")
						sections[sec].vhd_file << " not (";
					else if (l.line == "NOT")
						sections[sec].vhd_file << " not ";
					else if (l.line == "0")
						sections[sec].vhd_file << "('0'";
					else if (l.line == "1")
						sections[sec].vhd_file << "('1'";
					else if (l.line == "TD" || l.line == "TD10NS" || l.line == "INT")
						sections[sec].vhd_file << "(";
					else {
						if (l.line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
							error_exit(sec, "Illegal function name <" + l.line + ">.", lines);
						sections[sec].vhd_file << l.line << "(";
					}
				}
				prev_ident = l.ident;
			}
			last_ident = prev_ident;
			while (0 <= last_ident) {
				if (stack[last_ident] == "ORNOT" || stack[last_ident] == "ANDNOT") {
					sections[sec].vhd_file << "))";
				} else if (stack[last_ident] != "NOT" && stack[last_ident] != "" && stack[last_ident] != "TEMP") {
					sections[sec].vhd_file << ")";
				}
				stack[last_ident] = "";
				last_ident--;
			}
			sections[sec].vhd_file << ";\n";
		}
	}
	if (numtemps > issuedtemps) {
		issuedtemps++;
		process(sec, temporaries[issuedtemps - 1]);
	}
}

static int special = 0;

static void process_special(std::string sec, std::vector<LINE> lines) {

	if (lines[0].line != "SPECIAL")
		return;
	if (lines.size() == 1) 
		error_exit(sec, "Multi-line statement expected.", lines);
	SIG1 output;
	output.second.length = 1;
	sections[sec].vhd_file << "  SPEC" << ++special << " : entity work." << lines[1].line << " port map (clk, rst, hlt";
	for (unsigned int i = 2; i < lines.size(); i++) {
		LINE& l = lines[i];
		if (l.line == "IN") {
			continue;
		}
		if (l.line == "OUT") {
			continue;
		}
		if (l.line == "INOUT") {
			continue;
		}
		if (l.is_signal) {
			if (l.signal.first.substr(1) == "temp")
				error_exit(sec, "Can't have <temp> in a SPECIAL block.", lines);
			l.signal = get_final_signal(sec, l.line);
			sections[sec].vhd_file << ", " << put_signal(sec, l.signal);
			continue;
		}
		error_exit(sec, "Illegal line in SPECIAL block.", lines);
	}
	sections[sec].vhd_file << ");\n";
}

void write_vhd_file(std::string sec) {

	sections[sec].vhd_file << "library IEEE;\n";
	sections[sec].vhd_file << "use IEEE.STD_LOGIC_1164.ALL;\n";
	sections[sec].vhd_file << "use IEEE.NUMERIC_STD.ALL;\n\n";

	sections[sec].vhd_file << "entity " << sec << " is\n";
	sections[sec].vhd_file << "  port(\n";
	sections[sec].vhd_file << "    clk : in STD_LOGIC;\n";
	sections[sec].vhd_file << "    rst : in STD_LOGIC;\n";
	sections[sec].vhd_file << "    hlt : in STD_LOGIC";

	for (auto& b : sections[sec].foreign_signals["*"]) {
		auto& sig = ext_signals[b.first];
		std::string sn = safename_signal(sec, b.first, true);
		if (!sig.basics.dimensioned) {
			sections[sec].vhd_file << ";\n    " << sn << " : in STD_LOGIC";
		}
		else {
			if (sig.basics.order == -1)
				sections[sec].vhd_file << ";\n    " << sn << " : in STD_LOGIC_VECTOR (" << sig.basics.high << " downto " << sig.basics.low << ")";
			else
				sections[sec].vhd_file << ";\n    " << sn << " : in STD_LOGIC_VECTOR (" << sig.basics.low << " to " << sig.basics.high << ")";
		}
	}
	for (auto& sig : sections[sec].signals) {
		if (!sig.second.basics.external)
			continue;
		std::string sn = safename_signal(sec, sig.first, true);
		if (!sig.second.basics.dimensioned) {
			sections[sec].vhd_file << ";\n    " << sn << " : out STD_LOGIC";
		}
		else {
			if (sig.second.basics.order == -1)
				sections[sec].vhd_file << ";\n    " << sn << " : out STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ")";
			else
				sections[sec].vhd_file << ";\n    " << sn << " : out STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ")";
		}
	}

	for (auto& sig : sections[sec].signals) {
		if (collapse && sig.second.aliased)
			continue;
		if (eliminate && sig.second.eliminated)
			continue;
		if (sig.second.basics.external || !sig.second.external_ref)
			continue;
		std::string sn = safename_signal(sec, sig.first, true);
		if (!sig.second.basics.dimensioned) {
			sections[sec].vhd_file << ";\n    " << sn << " : buffer STD_LOGIC";
		}
		else {
			if (sig.second.basics.order == -1)
				sections[sec].vhd_file << ";\n    " << sn << " : buffer STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ")";
			else
				sections[sec].vhd_file << ";\n    " << sn << " : buffer STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ")";
		}
	}

	for (auto& a : sections[sec].foreign_signals) {
		for (auto& b : a.second) {
			if (sections.count(a.first)) {
				if (sections[a.first].signals.count(b.first)) {
					auto& sig = sections[a.first].signals[b.first];
					if (collapse && sig.aliased)
						continue;
					if (eliminate && sig.eliminated)
						continue;
					if (!sig.external_ref)
						continue;
					std::string sn = safename_signal(sec, b.first, true);
					if (!sig.basics.dimensioned) {
						sections[sec].vhd_file << ";\n    " << a.first << "_" << sn << " : in STD_LOGIC";
					}
					else {
						if (sig.basics.order == -1)
							sections[sec].vhd_file << ";\n    " << a.first << "_" << sn << " : in STD_LOGIC_VECTOR (" << sig.basics.high << " downto " << sig.basics.low << ")";
						else
							sections[sec].vhd_file << ";\n    " << a.first << "_" << sn << " : in STD_LOGIC_VECTOR (" << sig.basics.low << " to " << sig.basics.high << ")";
					}
				}
			}
		}
	}

	sections[sec].vhd_file << "\n  );\n";
	sections[sec].vhd_file << "end " << sec << ";\n\n";

	sections[sec].vhd_file << "architecture Behavioral of " << sec << " is\n";
	for (auto& sig : sections[sec].signals) {
		if (collapse && sig.second.aliased)
			continue;
		if (eliminate && sig.second.eliminated)
			continue;
		if (sig.second.external_ref)
			continue;
		std::string sn = safename_signal(sec, sig.first, true);

		if (!sig.second.basics.dimensioned) {
			sections[sec].vhd_file << "  signal " << sn << " : STD_LOGIC := '" << ((sig.first[0] == '-') ? "1" : "0") << "';\n";
		}
		else {
			if (sig.second.basics.order == -1)
				sections[sec].vhd_file << "  signal " << sn << " : STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ") := \"";
			else
				sections[sec].vhd_file << "  signal " << sn << " : STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ") := \"";
			for (int i = 0; i< sig.second.basics.length;i++)
				sections[sec].vhd_file << ((sig.first[0] == '-') ? "1" : "0");
			sections[sec].vhd_file << "\";\n";
		}
	}
	sections[sec].vhd_file << "begin\n";


	sections[sec].vhd_file << "  process(clk)\n";
	sections[sec].vhd_file << "  begin\n";
	sections[sec].vhd_file << "    if (rising_edge(clk)) then\n";
	sections[sec].vhd_file << "      if (rst = '1') then\n";
	for (auto& sig : sections[sec].signals) {
		if (collapse && sig.second.aliased)
			continue;
		if (eliminate && sig.second.eliminated)
			continue;
		if (sig.second.basics.special)
			continue;
//		if (sig.second.basics.external)
//			continue;
		std::string sn = safename_signal(sec, sig.first, true);
		if (!sig.second.basics.dimensioned) {
			sections[sec].vhd_file << "        " << sn << " <= \'" << ((sig.first[0] == '-') ? "1" : "0") << "\';\n";
		}
		else {
			sections[sec].vhd_file << "        " << sn << " <= \"";
			for (int i = 0; i < sig.second.basics.length; i++) sections[sec].vhd_file << ((sig.first[0] == '-') ? "1" : "0");
			sections[sec].vhd_file << "\";\n";
		}
	}
	sections[sec].vhd_file << "      elsif (hlt='0') then\n";
	process_lines(sec, process, 1);
	// Both ALD groups advance on each 10 ns core edge. Preserve the group
	// markers so the Boolean-expression audit can still check classification.
	sections[sec].vhd_file << "        -- ALD_TIMING_BEGIN\n";
	process_lines(sec, process, 2);
	sections[sec].vhd_file << "        -- ALD_TIMING_END\n";
	sections[sec].vhd_file << "      end if;\n";
	sections[sec].vhd_file << "    end if;\n";
	sections[sec].vhd_file << "  end process;\n\n";
	process_lines(sec, process_special);
	sections[sec].vhd_file << "end Behavioral;\n";

}

void write_top_vhd_file(std::string outdir) {
	std::filesystem::path pp("ald.vhd");
	pp = outdir / pp.filename();
	std::ofstream vhd_file(pp);

	vhd_file << "library IEEE; \n";
	vhd_file << "use IEEE.STD_LOGIC_1164.ALL;\n";
	vhd_file << "use IEEE.NUMERIC_STD.ALL;\n\n";

	vhd_file << "entity ald is\n";
	vhd_file << "  port(\n";
	vhd_file << "    clk : in STD_LOGIC;\n";
	vhd_file << "    rst : in STD_LOGIC;\n";
	vhd_file << "    hlt : in STD_LOGIC";

	for (auto& sig : ext_signals) {
		std::string sn = safename_signal("", sig.first, true);
		if (!sig.second.basics.dimensioned) {
			vhd_file << ";\n    " << sn << " : in STD_LOGIC";
		}
		else {
			if (sig.second.basics.order == -1)
				vhd_file << ";\n    " << sn << " : in STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ")";
			else
				vhd_file << ";\n    " << sn << " : in STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ")";
		}
	}
	vhd_file << "\n";
	for (auto& sec : sections) {
		for (auto& sig : sec.second.signals) {
			if (sig.second.basics.external) {
				std::string sn = safename_signal("", sig.first, true);
				if (!sig.second.basics.dimensioned) {
					vhd_file << ";\n    " << sn << " : out STD_LOGIC";
				}
				else {
					if (sig.second.basics.order == -1)
						vhd_file << ";\n    " << sn << " : out STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ")";
					else
						vhd_file << ";\n    " << sn << " : out STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ")";
				}
			}
		}
	}
	vhd_file << "\n  );\n";
	vhd_file << "end ald;\n\n";

	vhd_file << "architecture Behavioral of ald is\n";
	for (auto& sec : sections) {
		for (auto& sig : sec.second.signals) {
			if (collapse && sig.second.aliased)
				continue;
			if (eliminate && sig.second.eliminated)
				continue;
			if (sig.second.basics.external || !sig.second.external_ref)
				continue;
			std::string sn = safename_signal(sec.first, sig.first, true);
			if (!sig.second.basics.dimensioned) {
				vhd_file << "  signal " << sec.first << "_" << sn << " : STD_LOGIC := '"<< ((sig.first[0]=='-')?"1":"0") <<"';\n";
			}
			else {
				if (sig.second.basics.order == -1)
					vhd_file << "  signal " << sec.first << "_" << sn << " : STD_LOGIC_VECTOR (" << sig.second.basics.high << " downto " << sig.second.basics.low << ") := \"";
				else
					vhd_file << "  signal " << sec.first << "_" << sn << " : STD_LOGIC_VECTOR (" << sig.second.basics.low << " to " << sig.second.basics.high << ") := \"";
				for (int i = 0; i < sig.second.basics.length; i++)
					vhd_file << ((sig.first[0] == '-') ? "1" : "0");
				vhd_file << "\";\n";
			}
		}
	}
	vhd_file << "begin\n";

	for (auto&sec: sections) {
		vhd_file << "  " << sec.first << " : entity work." << sec.first << " port map (\n";

		vhd_file << "    clk => clk,\n    rst => rst,\n    hlt => hlt";

		for (auto& b : sec.second.foreign_signals["*"]) {
			auto& sig = ext_signals[b.first];
			std::string sn = safename_signal(sec.first, b.first, true);
			if (!sig.basics.dimensioned) {
				vhd_file << ",\n    " << sn << " => " << sn;
			}
			else {
				if (sig.basics.order == -1)
					vhd_file << ",\n    " << sn << " => " << sn << "(" << sig.basics.high << " downto " << sig.basics.low << ")";
				else
					vhd_file << ",\n    " << sn << "=> " << sn << "(" << sig.basics.low << " to " << sig.basics.high << ")";
			}
		}
		for (auto& sig : sec.second.signals) {
			if (!sig.second.basics.external)
				continue;
			std::string sn = safename_signal(sec.first, sig.first, true);
			vhd_file << ",\n    " << sn << " => " << sn;
		}
		vhd_file << "\n";

		for (auto& sig : sec.second.signals) {
			if (collapse && sig.second.aliased)
				continue;
			if (eliminate && sig.second.eliminated)
				continue;
			if (sig.second.basics.external || !sig.second.external_ref)
				continue;
			std::string sn = safename_signal(sec.first, sig.first, true);
			vhd_file << ",\n    " << sn << " => " << sec.first << "_" << sn;
		}

		vhd_file << "\n";

		for (auto& a : sec.second.foreign_signals) {
			for (auto& b : a.second) {
				if (sections.count(a.first)) {
					if (sections[a.first].signals.count(b.first)) {
						auto& sig = sections[a.first].signals[b.first];
						if (collapse && sig.aliased)
							continue;
						if (eliminate && sig.eliminated)
							continue;
						if (!sig.external_ref)
							continue;
						std::string sn = safename_signal(sec.first, b.first, true);
						if (!sig.basics.dimensioned) {
							vhd_file << ",\n    " << a.first << "_" << sn << " => " << a.first << "_" << sn;
						}
						else {
							if (sig.basics.order == -1)
								vhd_file << ",\n    " << a.first << "_" << sn << " => " << a.first << "_" << sn << "(" << sig.basics.high << " downto " << sig.basics.low << ")";
							else
								vhd_file << ",\n    " << a.first << "_" << sn << " => " << a.first << "_" << sn << "(" << sig.basics.low << " to " << sig.basics.high << ")";
						}
					}
				}
			}
		}
		vhd_file << "\n  );\n\n";
	}

	vhd_file << "end Behavioral;\n";

}
