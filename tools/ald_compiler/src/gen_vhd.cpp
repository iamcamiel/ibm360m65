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
#include <regex>
#include <set>

static bool diagnostic_cpu() { return sections.count("KW") && sections.count("RX") && sections.count("CA") && sections.count("WA"); }

static int numtemps = 0;
static int issuedtemps = 0;
static std::ostream* logic_stream = nullptr;
static std::map<std::string, std::pair<std::string, std::string>> logic_bodies;
static std::map<std::string, std::set<std::string>> noclock_signals;

static std::ostream& vhd_stream(const std::string& sec) {
	return logic_stream ? *logic_stream : sections[sec].vhd_file;
}

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
		vhd_stream(sec) << "-- " << rest << "\n";
	}
	else if ((rest = getrest(lines[0].line, "#")) != "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after comment.", lines);
		vhd_stream(sec) << "-- " << rest << "\n";
	}
	else if (lines[0].line == "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after blank line.", lines);
		vhd_stream(sec) << "\n";
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
						vhd_stream(sec) << "      " + put_signal(sec, output, i) + " <= ";
					continue;
				}
				last_ident = prev_ident;
				while (l.ident <= last_ident) {
					if (stack[last_ident] == "ORNOT" || stack[last_ident] == "ANDNOT") {
						vhd_stream(sec) << "))";
					} else if (stack[last_ident] != "" && stack[last_ident] != "TEMP") {
						vhd_stream(sec) << ")";
					}
					stack[last_ident] = "";
					last_ident--;
				}
				if (l.ident <= prev_ident) {
					if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
						vhd_stream(sec) << " and ";
					else if (stack[last_ident] == "ANDNOT")
						vhd_stream(sec) << ") and not (";
					else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
						vhd_stream(sec) << " or ";
					else if (stack[last_ident] == "ORNOT")
						vhd_stream(sec) << ") or not (";
					else if (stack[last_ident] == "XOR")
						vhd_stream(sec) << " xor ";
					else if (is_known_word(stack[last_ident]))
						error_exit(sec, "syntax error in logic expression; no second term expected", lines);
					else
						vhd_stream(sec) << " , ";
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
							vhd_stream(sec) << put_signal(sec, l.signal);
						}
						else {
							vhd_stream(sec) << put_signal(sec, l.signal, 0);
							for (int ii = 1; ii < l.signal.second.length; ii++) {
								if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
									vhd_stream(sec) << " and ";
								else if (stack[last_ident] == "ANDNOT")
									vhd_stream(sec) << ") and not (";
								else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
									vhd_stream(sec) << " or ";
								else if (stack[last_ident] == "ORNOT")
									vhd_stream(sec) << ") or not (";
								else if (stack[last_ident] == "XOR")
									vhd_stream(sec) << " xor ";
								else
									error_exit(sec, "syntax error in logic expression; no second term expected", lines);
								vhd_stream(sec) << put_signal(sec, l.signal, ii);
							}
						}
					}
					else {
						vhd_stream(sec) << put_signal(sec, l.signal, i);
					}
				}
				else {
					stack[l.ident] = l.line;
					if (l.line == "AND" || l.line == "OR" || l.line == "XOR")
						vhd_stream(sec) << "(";
					else if (l.line == "ANDNOT" || l.line == "ORNOT")
						vhd_stream(sec) << "( not (";
					else if (l.line == "NAND" || l.line == "NOR")
						vhd_stream(sec) << " not (";
					else if (l.line == "NOT")
						// Parenthesize the operand: consecutive unary NOTs are invalid VHDL.
						vhd_stream(sec) << " not (";
					else if (l.line == "0")
						vhd_stream(sec) << "('0'";
					else if (l.line == "1")
						vhd_stream(sec) << "('1'";
					else if (l.line == "TD" || l.line == "TD10NS" || l.line == "INT")
						vhd_stream(sec) << "(";
					else {
						if (l.line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
							error_exit(sec, "Illegal function name <" + l.line + ">.", lines);
						vhd_stream(sec) << l.line << "(";
					}
				}
				prev_ident = l.ident;
			}
			last_ident = prev_ident;
			while (0 <= last_ident) {
				if (stack[last_ident] == "ORNOT" || stack[last_ident] == "ANDNOT") {
					vhd_stream(sec) << "))";
				} else if (stack[last_ident] != "" && stack[last_ident] != "TEMP") {
					vhd_stream(sec) << ")";
				}
				stack[last_ident] = "";
				last_ident--;
			}
			vhd_stream(sec) << ";\n";
		}
	}
	if (numtemps > issuedtemps) {
		issuedtemps++;
		process(sec, temporaries[issuedtemps - 1]);
	}
}

static int special = 0;

// The C++ model uses two simultaneous NOCLOCK evaluations per CLOCK update.
// Store the first NOCLOCK evaluation with CLOCK state on the core edge, then
// evaluate the second from that global snapshot. Cross-section first-pass
// ports are essential: a second evaluation must never read another section's
// already-settled output, which would turn a finite pass into a logic loop.
static void prepare_logic_bodies() {
	if (!logic_bodies.empty()) return;
	const std::regex target(R"((\w+)(?:\(\d+\))?\s*<=)");
	for (auto& section : sections) {
		std::ostringstream first, clock;
		logic_stream = &first;
		process_lines(section.first, process, 1);
		logic_stream = &clock;
		process_lines(section.first, process, 2);
		logic_stream = nullptr;
		logic_bodies[section.first] = {first.str(), clock.str()};
		std::string body = first.str();
		for (std::sregex_iterator i(body.begin(), body.end(), target), end; i != end; ++i)
			noclock_signals[section.first].insert((*i)[1]);
		body = clock.str();
		for (std::sregex_iterator i(body.begin(), body.end(), target), end; i != end; ++i)
			if (noclock_signals[section.first].count((*i)[1]))
				error_exit(section.first, "A signal assigned in both CLOCK and NOCLOCK cannot share a snapshot bank", i->str());
	}
}

static std::string signal_type(const SIGINFO1& signal) {
	if (!signal.dimensioned) return "STD_LOGIC";
	return "STD_LOGIC_VECTOR (" + std::to_string(signal.order == -1 ? signal.high : signal.low)
		+ (signal.order == -1 ? " downto " : " to ")
		+ std::to_string(signal.order == -1 ? signal.low : signal.high) + ")";
}

static std::string initial_value(const std::string& name, const SIGINFO1& signal) {
	char bit = name[0] == '-' ? '1' : '0';
	if (!signal.dimensioned) return std::string("'") + bit + "'";
	return "\"" + std::string(signal.length, bit) + "\"";
}

static std::map<std::string, std::string> second_pass_inputs(const std::string& sec) {
	std::map<std::string, std::string> inputs;
	for (const auto& name : noclock_signals[sec]) inputs[name] = name + "_first";
	for (auto& owner : sections[sec].foreign_signals) {
		for (auto& signal : owner.second) {
			std::string name = safename_signal(sec, signal.first, true);
			if (owner.first == "*") inputs[name] = name + "_sampled";
			else if (noclock_signals[owner.first].count(name))
				inputs[owner.first + "_" + name] = owner.first + "_" + name + "_first";
		}
	}
	return inputs;
}

static std::string replace_inputs(const std::string& expression,
	const std::map<std::string, std::string>& inputs) {
	const std::regex identifier(R"(\b[A-Za-z][A-Za-z0-9_]*\b)");
	std::string output;
	size_t previous = 0;
	for (std::sregex_iterator i(expression.begin(), expression.end(), identifier), end; i != end; ++i) {
		output += expression.substr(previous, i->position() - previous);
		auto replacement = inputs.find(i->str());
		output += replacement == inputs.end() ? i->str() : replacement->second;
		previous = i->position() + i->length();
	}
	return output + expression.substr(previous);
}

static void write_noclock_logic(const std::string& sec, bool second) {
	const std::regex assignment(R"(^(\s*)(\w+)(\(\d+\))?\s*<=\s*(.*);$)");
	const auto inputs = second_pass_inputs(sec);
	std::istringstream body(logic_bodies[sec].first);
	std::string line;
	while (std::getline(body, line)) {
		std::smatch match;
		if (!std::regex_match(line, match, assignment)) {
			sections[sec].vhd_file << line << "\n";
			continue;
		}
		std::string name = match[2], index = match[3], expression = match[4];
		if (!second) {
			sections[sec].vhd_file << match[1] << name << "_first" << index << " <= " << expression << ";\n";
		} else {
			// Output bits reset synchronously through ald_settle_active. External
			// inputs are sampled on the same edge, so halt freezes this pass too.
			sections[sec].vhd_file << "  " << name << index << " <= "
				<< replace_inputs(expression, inputs) << " when ald_settle_active = '1' else "
				<< (name.substr(0, 2) == "M_" ? "'1'" : "'0'") << ";\n";
		}
	}
}

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
	prepare_logic_bodies();

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

	for (auto& sig : sections[sec].signals) {
		std::string name = safename_signal(sec, sig.first, true);
		if (noclock_signals[sec].count(name) && (sig.second.external_ref || sig.second.basics.external))
			sections[sec].vhd_file << ";\n    " << name << "_first : buffer " << signal_type(sig.second.basics);
	}
	for (auto& owner : sections[sec].foreign_signals) {
		if (owner.first == "*") continue;
		for (auto& signal : owner.second) {
			std::string name = safename_signal(sec, signal.first, true);
			if (noclock_signals[owner.first].count(name))
				sections[sec].vhd_file << ";\n    " << owner.first << "_" << name << "_first : in "
					<< signal_type(sections[owner.first].signals[signal.first].basics);
		}
	}
	if (sec == "KW") sections[sec].vhd_file << ";\n    diagnostic_checks_o : out STD_LOGIC_VECTOR(31 downto 0) := (others => '0')";
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
	sections[sec].vhd_file << "  signal ald_settle_active : STD_LOGIC := '0';\n";
	for (auto& sig : sections[sec].signals) {
		std::string name = safename_signal(sec, sig.first, true);
		if (noclock_signals[sec].count(name) && !sig.second.external_ref && !sig.second.basics.external)
			sections[sec].vhd_file << "  signal " << name << "_first : " << signal_type(sig.second.basics)
				<< " := " << initial_value(sig.first, sig.second.basics) << ";\n";
	}
	for (auto& signal : sections[sec].foreign_signals["*"]) {
		auto& info = ext_signals[signal.first].basics;
		sections[sec].vhd_file << "  signal " << safename_signal(sec, signal.first, true)
			<< "_sampled : " << signal_type(info) << " := " << initial_value(signal.first, info) << ";\n";
	}
	sections[sec].vhd_file << "begin\n";
	if (sec == "KW") sections[sec].vhd_file << R"DIAG(  diagnostic_checks_o(0) <= not M_ind_check_reg_1_summary;
  diagnostic_checks_o(16) <= not M_ind_check_reg_1_summary_first;
  diagnostic_checks_o(1) <= not M_ind_check_reg_2_summary;
  diagnostic_checks_o(17) <= not M_ind_check_reg_2_summary_first;
  diagnostic_checks_o(2) <= not M_check_reg_1_error;
  diagnostic_checks_o(18) <= not M_check_reg_1_error_first;
  diagnostic_checks_o(3) <= not M_check_reg_2_error;
  diagnostic_checks_o(19) <= not M_check_reg_2_error_first;
  diagnostic_checks_o(4) <= P_delayed_error_log_req;
  diagnostic_checks_o(20) <= P_delayed_error_log_req_first;
  diagnostic_checks_o(5) <= P_storage_error;
  diagnostic_checks_o(21) <= P_storage_error_first;
  diagnostic_checks_o(6) <= not M_error_log_required;
  diagnostic_checks_o(22) <= not M_error_log_required_first;
  diagnostic_checks_o(7) <= not M_disable_check;
  diagnostic_checks_o(23) <= not M_disable_check_first;
  diagnostic_checks_o(8) <= not M_hard_stop_condition_kw071;
  diagnostic_checks_o(24) <= not M_hard_stop_condition_kw071_first;
  diagnostic_checks_o(9) <= not M_hard_stop_condition_kw081;
  diagnostic_checks_o(25) <= not M_hard_stop_condition_kw081_first;
  diagnostic_checks_o(10) <= P_check_reset_pb;
  diagnostic_checks_o(26) <= P_check_reset_pb_first;
  diagnostic_checks_o(11) <= P_lamp_test_and_allow_ind_pb;
  diagnostic_checks_o(27) <= P_lamp_test_and_allow_ind_pb_first;
  diagnostic_checks_o(15 downto 12) <= (others=>'0');
  diagnostic_checks_o(31 downto 28) <= (others=>'0');
)DIAG";


	sections[sec].vhd_file << "  process(clk)\n";
	sections[sec].vhd_file << "  begin\n";
	sections[sec].vhd_file << "    if (rising_edge(clk)) then\n";
	sections[sec].vhd_file << "      if (rst = '1') then\n";
	sections[sec].vhd_file << "        ald_settle_active <= '0';\n";
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
		if (noclock_signals[sec].count(sn)) sn += "_first";
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
	sections[sec].vhd_file << "        ald_settle_active <= '1';\n";
	for (auto& signal : sections[sec].foreign_signals["*"]) {
		std::string name = safename_signal(sec, signal.first, true);
		sections[sec].vhd_file << "        " << name << "_sampled <= " << name << ";\n";
	}
	sections[sec].vhd_file << "        -- ALD_NOCLOCK_FIRST_BEGIN\n";
	write_noclock_logic(sec, false);
	sections[sec].vhd_file << "        -- ALD_NOCLOCK_FIRST_END\n";
	sections[sec].vhd_file << "        -- ALD_TIMING_BEGIN\n";
	sections[sec].vhd_file << logic_bodies[sec].second;
	sections[sec].vhd_file << "        -- ALD_TIMING_END\n";
	sections[sec].vhd_file << "      end if;\n";
	sections[sec].vhd_file << "    end if;\n";
	sections[sec].vhd_file << "  end process;\n\n";
	sections[sec].vhd_file << "  -- ALD_NOCLOCK_SECOND_BEGIN\n";
	write_noclock_logic(sec, true);
	sections[sec].vhd_file << "  -- ALD_NOCLOCK_SECOND_END\n\n";
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
	if (diagnostic_cpu()) vhd_file << ";\n    diagnostic_state_o : out STD_LOGIC_VECTOR(767 downto 0) := (others=>'0')";
	vhd_file << "\n  );\n";
	vhd_file << "end ald;\n\n";

	vhd_file << "architecture Behavioral of ald is\n";
	if (diagnostic_cpu()) vhd_file << "  signal diagnostic_checks : std_logic_vector(31 downto 0);\n";
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
	for (auto& sec : sections) {
		for (auto& sig : sec.second.signals) {
			std::string name = safename_signal(sec.first, sig.first, true);
			if (noclock_signals[sec.first].count(name) && (sig.second.external_ref || sig.second.basics.external))
				vhd_file << "  signal " << sec.first << "_" << name << "_first : "
					<< signal_type(sig.second.basics) << " := " << initial_value(sig.first, sig.second.basics) << ";\n";
		}
	}
	vhd_file << "begin\n";
	if (diagnostic_cpu()) vhd_file << R"DIAG(  -- Diagnostic schema1: observation only, sampled before a core edge.
  diagnostic_state_o(11 downto 0) <= RX_P_rosar;
  diagnostic_state_o(12) <= diagnostic_checks(0);
  diagnostic_state_o(13) <= diagnostic_checks(1);
  diagnostic_state_o(14) <= diagnostic_checks(16);
  diagnostic_state_o(15) <= diagnostic_checks(17);
  diagnostic_state_o(16) <= rst;
  diagnostic_state_o(17) <= P_reg_se_size(0);
  diagnostic_state_o(18) <= not RW_M_wait_state;
  diagnostic_state_o(19) <= hlt;
  diagnostic_state_o(20) <= KM_P_clock_p0;
  diagnostic_state_o(21) <= KM_P_clock_p2;
  diagnostic_state_o(22) <= '0';
  diagnostic_state_o(23) <= '0';
  diagnostic_state_o(24) <= '0';
  diagnostic_state_o(25) <= '0';
  diagnostic_state_o(26) <= '0';
  diagnostic_state_o(27) <= '0';
  diagnostic_state_o(28) <= '0';
  diagnostic_state_o(29) <= '0';
  diagnostic_state_o(30) <= '0';
  diagnostic_state_o(31) <= '0';
  diagnostic_state_o(63 downto 32) <= x"00" & CA_P_ic;
  diagnostic_state_o(95 downto 64) <= diagnostic_checks;
  diagnostic_state_o(96) <= MC_P_accept;
  diagnostic_state_o(97) <= MC_P_sample_sdbi;
  diagnostic_state_o(98) <= not MC_M_select_se_1_even;
  diagnostic_state_o(99) <= MC_P_store_to_se;
  diagnostic_state_o(100) <= MC_P_cancel;
  diagnostic_state_o(101) <= MC_P_set_key;
  diagnostic_state_o(102) <= MC_P_insert_key;
  diagnostic_state_o(103) <= MC_P_test_and_set;
  diagnostic_state_o(104) <= MC_P_protection_check;
  diagnostic_state_o(105) <= not MC_M_ind_cpu_storage_data_check;
  diagnostic_state_o(106) <= not MC_M_invalid_address;
  diagnostic_state_o(107) <= not MC_M_timeout_pulse_not_stor;
  diagnostic_state_o(108) <= not MC_M_bcu_stop_cl_on_cpu_stg_check;
  diagnostic_state_o(109) <= not MC_M_stop_clock_tgr_aw4;
  diagnostic_state_o(110) <= not MC_M_bcu_hold_on_cpu_clock;
  diagnostic_state_o(111) <= MC_P_error_reset;
  diagnostic_state_o(112) <= '0';
  diagnostic_state_o(113) <= '0';
  diagnostic_state_o(114) <= '0';
  diagnostic_state_o(115) <= '0';
  diagnostic_state_o(116) <= '0';
  diagnostic_state_o(117) <= '0';
  diagnostic_state_o(118) <= '0';
  diagnostic_state_o(119) <= '0';
  diagnostic_state_o(120) <= '0';
  diagnostic_state_o(121) <= '0';
  diagnostic_state_o(122) <= '0';
  diagnostic_state_o(123) <= '0';
  diagnostic_state_o(124) <= '0';
  diagnostic_state_o(125) <= '0';
  diagnostic_state_o(126) <= '0';
  diagnostic_state_o(127) <= '0';
  diagnostic_state_o(128) <= not AP_M_ind_full_sum_error_04M07;
  diagnostic_state_o(137) <= not AP_M_ind_half_sum_error_04M07;
  diagnostic_state_o(129) <= not AP_M_ind_full_sum_error_08M15;
  diagnostic_state_o(138) <= not AP_M_ind_half_sum_error_08M15;
  diagnostic_state_o(130) <= not AP_M_ind_full_sum_error_16M23;
  diagnostic_state_o(139) <= not AP_M_ind_half_sum_error_16M23;
  diagnostic_state_o(131) <= not AP_M_ind_full_sum_error_24M31;
  diagnostic_state_o(140) <= not AP_M_ind_half_sum_error_24M31;
  diagnostic_state_o(132) <= not AP_M_ind_full_sum_error_32M39;
  diagnostic_state_o(141) <= not AP_M_ind_half_sum_error_32M39;
  diagnostic_state_o(133) <= not AP_M_ind_full_sum_error_40M47;
  diagnostic_state_o(142) <= not AP_M_ind_half_sum_error_40M47;
  diagnostic_state_o(134) <= not AP_M_ind_full_sum_error_48M55;
  diagnostic_state_o(143) <= not AP_M_ind_half_sum_error_48M55;
  diagnostic_state_o(135) <= not AP_M_ind_full_sum_error_56M63;
  diagnostic_state_o(144) <= not AP_M_ind_half_sum_error_56M63;
  diagnostic_state_o(136) <= not AP_M_ind_full_sum_error_64M67;
  diagnostic_state_o(145) <= not AP_M_ind_half_sum_error_64M67;
  diagnostic_state_o(146) <= '0';
  diagnostic_state_o(147) <= '0';
  diagnostic_state_o(148) <= '0';
  diagnostic_state_o(149) <= '0';
  diagnostic_state_o(150) <= '0';
  diagnostic_state_o(151) <= '0';
  diagnostic_state_o(152) <= '0';
  diagnostic_state_o(153) <= '0';
  diagnostic_state_o(154) <= '0';
  diagnostic_state_o(155) <= '0';
  diagnostic_state_o(156) <= '0';
  diagnostic_state_o(157) <= '0';
  diagnostic_state_o(158) <= '0';
  diagnostic_state_o(159) <= '0';
  diagnostic_state_o(191 downto 160) <= x"00" & MA_P_sab;
  diagnostic_state_o(223 downto 192) <= MB_P_sdbi(0 to 31);
  diagnostic_state_o(255 downto 224) <= MB_P_sdbi(32 to 63);
  diagnostic_state_o(287 downto 256) <= x"000000" & MB_P_mark;
  diagnostic_state_o(319 downto 288) <= not BF_M_local_store_bus;
  diagnostic_state_o(320) <= not BF_M_local_store_bus_parity_check;
  diagnostic_state_o(321) <= not MB_M_sdbi_parity_check;
  diagnostic_state_o(322) <= MB_P_sdbo_parity_error;
  diagnostic_state_o(323) <= not MB_M_ind_sdbo_pty_check;
  diagnostic_state_o(324) <= '0';
  diagnostic_state_o(325) <= '0';
  diagnostic_state_o(326) <= '0';
  diagnostic_state_o(327) <= '0';
  diagnostic_state_o(328) <= '0';
  diagnostic_state_o(329) <= '0';
  diagnostic_state_o(330) <= '0';
  diagnostic_state_o(331) <= '0';
  diagnostic_state_o(332) <= '0';
  diagnostic_state_o(333) <= '0';
  diagnostic_state_o(334) <= '0';
  diagnostic_state_o(335) <= '0';
  diagnostic_state_o(336) <= '0';
  diagnostic_state_o(337) <= '0';
  diagnostic_state_o(338) <= '0';
  diagnostic_state_o(339) <= '0';
  diagnostic_state_o(340) <= '0';
  diagnostic_state_o(341) <= '0';
  diagnostic_state_o(342) <= '0';
  diagnostic_state_o(343) <= '0';
  diagnostic_state_o(344) <= '0';
  diagnostic_state_o(345) <= '0';
  diagnostic_state_o(346) <= '0';
  diagnostic_state_o(347) <= '0';
  diagnostic_state_o(348) <= '0';
  diagnostic_state_o(349) <= '0';
  diagnostic_state_o(350) <= '0';
  diagnostic_state_o(351) <= '0';
  diagnostic_state_o(352) <= AS_P_saddl_parity;
  diagnostic_state_o(353) <= not AS_M_sadder_full_sum_error;
  diagnostic_state_o(354) <= not AS_M_sadder_half_sum_error;
  diagnostic_state_o(355) <= AS_P_clock_p0;
  diagnostic_state_o(356) <= AS_P_clock_p2;
  diagnostic_state_o(357) <= AS_P_clock_p4;
  diagnostic_state_o(367 downto 360) <= AS_P_saddl;
  diagnostic_state_o(358) <= '0';
  diagnostic_state_o(359) <= '0';
  diagnostic_state_o(368) <= '0';
  diagnostic_state_o(369) <= '0';
  diagnostic_state_o(370) <= '0';
  diagnostic_state_o(371) <= '0';
  diagnostic_state_o(372) <= '0';
  diagnostic_state_o(373) <= '0';
  diagnostic_state_o(374) <= '0';
  diagnostic_state_o(375) <= '0';
  diagnostic_state_o(376) <= '0';
  diagnostic_state_o(377) <= '0';
  diagnostic_state_o(378) <= '0';
  diagnostic_state_o(379) <= '0';
  diagnostic_state_o(380) <= '0';
  diagnostic_state_o(381) <= '0';
  diagnostic_state_o(382) <= '0';
  diagnostic_state_o(383) <= '0';
  diagnostic_state_o(415 downto 384) <= x"00" & RD_P_d_bit;
  diagnostic_state_o(447 downto 416) <= RS_RT_P_st_bit(0 to 31);
  diagnostic_state_o(479 downto 448) <= RS_RT_P_st_bit(32 to 63);
  diagnostic_state_o(480) <= KU_P_soros_O_sync_lth;
  diagnostic_state_o(481) <= KU_P_soros_and_nt_ros_P_flt;
  diagnostic_state_o(482) <= KU_P_ce_logout_P_mc_intrpt;
  diagnostic_state_o(483) <= KU_P_log_out;
  diagnostic_state_o(484) <= KU_P_scan_force_rosar(7);
  diagnostic_state_o(485) <= not KU_M_flt_scan_block;
  diagnostic_state_o(486) <= not KU_M_log_outPscan_bypass;
  diagnostic_state_o(487) <= not KU_M_inhibit_free_running_osc;
  diagnostic_state_o(488) <= not KU_M_inhibit_ros_error_sample;
  diagnostic_state_o(500 downto 496) <= KU_P_address_sequence;
  diagnostic_state_o(489) <= '0';
  diagnostic_state_o(490) <= '0';
  diagnostic_state_o(491) <= '0';
  diagnostic_state_o(492) <= '0';
  diagnostic_state_o(493) <= '0';
  diagnostic_state_o(494) <= '0';
  diagnostic_state_o(495) <= '0';
  diagnostic_state_o(501) <= '0';
  diagnostic_state_o(502) <= '0';
  diagnostic_state_o(503) <= '0';
  diagnostic_state_o(504) <= '0';
  diagnostic_state_o(505) <= '0';
  diagnostic_state_o(506) <= '0';
  diagnostic_state_o(507) <= '0';
  diagnostic_state_o(508) <= '0';
  diagnostic_state_o(509) <= '0';
  diagnostic_state_o(510) <= '0';
  diagnostic_state_o(511) <= '0';
  diagnostic_state_o(543 downto 512) <= P_reg_se_resp;
  diagnostic_state_o(575 downto 544) <= P_reg_se_rdata_hi;
  diagnostic_state_o(607 downto 576) <= P_reg_se_rdata_lo;
  diagnostic_state_o(639 downto 608) <= P_reg_io_resp;
  diagnostic_state_o(671 downto 640) <= P_reg_io_int;
  diagnostic_state_o(672) <= AR_P_rosdr_bits_69M73_odd;
  diagnostic_state_o(673) <= not AP_M_ros_parity_ck_69M99;
  diagnostic_state_o(674) <= not AP_M_ind_half_sum_error_trig;
  diagnostic_state_o(675) <= '0';
  diagnostic_state_o(676) <= '0';
  diagnostic_state_o(677) <= '0';
  diagnostic_state_o(678) <= '0';
  diagnostic_state_o(679) <= '0';
  diagnostic_state_o(680) <= '0';
  diagnostic_state_o(681) <= '0';
  diagnostic_state_o(682) <= '0';
  diagnostic_state_o(683) <= '0';
  diagnostic_state_o(684) <= '0';
  diagnostic_state_o(685) <= '0';
  diagnostic_state_o(686) <= '0';
  diagnostic_state_o(687) <= '0';
  diagnostic_state_o(688) <= '0';
  diagnostic_state_o(689) <= '0';
  diagnostic_state_o(690) <= '0';
  diagnostic_state_o(691) <= '0';
  diagnostic_state_o(692) <= '0';
  diagnostic_state_o(693) <= '0';
  diagnostic_state_o(694) <= '0';
  diagnostic_state_o(695) <= '0';
  diagnostic_state_o(696) <= '0';
  diagnostic_state_o(697) <= '0';
  diagnostic_state_o(698) <= '0';
  diagnostic_state_o(699) <= '0';
  diagnostic_state_o(700) <= '0';
  diagnostic_state_o(701) <= '0';
  diagnostic_state_o(702) <= '0';
  diagnostic_state_o(703) <= '0';
  diagnostic_state_o(704) <= not AP_M_ind_full_sum_error_04M07_first;
  diagnostic_state_o(713) <= not AP_M_ind_half_sum_error_04M07_first;
  diagnostic_state_o(705) <= not AP_M_ind_full_sum_error_08M15_first;
  diagnostic_state_o(714) <= not AP_M_ind_half_sum_error_08M15_first;
  diagnostic_state_o(706) <= not AP_M_ind_full_sum_error_16M23_first;
  diagnostic_state_o(715) <= not AP_M_ind_half_sum_error_16M23_first;
  diagnostic_state_o(707) <= not AP_M_ind_full_sum_error_24M31_first;
  diagnostic_state_o(716) <= not AP_M_ind_half_sum_error_24M31_first;
  diagnostic_state_o(708) <= not AP_M_ind_full_sum_error_32M39_first;
  diagnostic_state_o(717) <= not AP_M_ind_half_sum_error_32M39_first;
  diagnostic_state_o(709) <= not AP_M_ind_full_sum_error_40M47_first;
  diagnostic_state_o(718) <= not AP_M_ind_half_sum_error_40M47_first;
  diagnostic_state_o(710) <= not AP_M_ind_full_sum_error_48M55_first;
  diagnostic_state_o(719) <= not AP_M_ind_half_sum_error_48M55_first;
  diagnostic_state_o(711) <= not AP_M_ind_full_sum_error_56M63_first;
  diagnostic_state_o(720) <= not AP_M_ind_half_sum_error_56M63_first;
  diagnostic_state_o(712) <= not AP_M_ind_full_sum_error_64M67_first;
  diagnostic_state_o(721) <= not AP_M_ind_half_sum_error_64M67_first;
  diagnostic_state_o(722) <= '0';
  diagnostic_state_o(723) <= '0';
  diagnostic_state_o(724) <= '0';
  diagnostic_state_o(725) <= '0';
  diagnostic_state_o(726) <= '0';
  diagnostic_state_o(727) <= '0';
  diagnostic_state_o(728) <= '0';
  diagnostic_state_o(729) <= '0';
  diagnostic_state_o(730) <= '0';
  diagnostic_state_o(731) <= '0';
  diagnostic_state_o(732) <= '0';
  diagnostic_state_o(733) <= '0';
  diagnostic_state_o(734) <= '0';
  diagnostic_state_o(735) <= '0';
  diagnostic_state_o(736) <= KW_P_stop_tgr;
  diagnostic_state_o(737) <= KW_P_hard_stop_inhibit_osc;
  diagnostic_state_o(738) <= not KW_M_inhibit_clock_ce_check;
  diagnostic_state_o(739) <= not KW_M_manual_trigger;
  diagnostic_state_o(740) <= not KW_M_disable_time_clock;
  diagnostic_state_o(741) <= KM_P_time_clock_step_trig;
  diagnostic_state_o(742) <= KM_P_timer_stepPstop_trigPintrpt;
  diagnostic_state_o(743) <= not KM_M_psa_lockout_set_ce_hardstop;
  diagnostic_state_o(744) <= '0';
  diagnostic_state_o(745) <= '0';
  diagnostic_state_o(746) <= '0';
  diagnostic_state_o(747) <= '0';
  diagnostic_state_o(748) <= '0';
  diagnostic_state_o(749) <= '0';
  diagnostic_state_o(750) <= '0';
  diagnostic_state_o(751) <= '0';
  diagnostic_state_o(752) <= '0';
  diagnostic_state_o(753) <= '0';
  diagnostic_state_o(754) <= '0';
  diagnostic_state_o(755) <= '0';
  diagnostic_state_o(756) <= '0';
  diagnostic_state_o(757) <= '0';
  diagnostic_state_o(758) <= '0';
  diagnostic_state_o(759) <= '0';
  diagnostic_state_o(760) <= '0';
  diagnostic_state_o(761) <= '0';
  diagnostic_state_o(762) <= '0';
  diagnostic_state_o(763) <= '0';
  diagnostic_state_o(764) <= '0';
  diagnostic_state_o(765) <= '0';
  diagnostic_state_o(766) <= '0';
  diagnostic_state_o(767) <= '0';
)DIAG";

	for (auto&sec: sections) {
		vhd_file << "  " << sec.first << " : entity work." << sec.first << " port map (\n";

		vhd_file << "    clk => clk,\n    rst => rst,\n    hlt => hlt";
		if (sec.first == "KW") vhd_file << ",\n    diagnostic_checks_o => " << (diagnostic_cpu() ? "diagnostic_checks" : "open");

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
		for (auto& sig : sec.second.signals) {
			std::string name = safename_signal(sec.first, sig.first, true);
			if (noclock_signals[sec.first].count(name) && (sig.second.external_ref || sig.second.basics.external))
				vhd_file << ",\n    " << name << "_first => " << sec.first << "_" << name << "_first";
		}
		for (auto& owner : sec.second.foreign_signals) {
			if (owner.first == "*") continue;
			for (auto& signal : owner.second) {
				std::string name = safename_signal(sec.first, signal.first, true);
				if (noclock_signals[owner.first].count(name))
					vhd_file << ",\n    " << owner.first << "_" << name << "_first => " << owner.first << "_" << name << "_first";
			}
		}
		vhd_file << "\n  );\n\n";
	}

	vhd_file << "end Behavioral;\n";

}
