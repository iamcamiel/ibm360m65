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
#include <functional>
#include <set>

void process_lines(std::string sec, void(*func)(std::string, std::vector<LINE>), int ck) {
	std::string line;
	std::vector<LINE> lines;
	int clk = 1;
	struct Block { int clock, line; std::vector<LINE> body; };
	std::vector<Block> blocks;
	int block_line = 1;

	sections[sec].ald_file.clear();
	sections[sec].ald_file.seekg(0);
	
	int lineno = 0;
	while (std::getline(sections[sec].ald_file, line))
	{
		lineno++;
		while (line.find("\t") != std::string::npos) {
			line.replace(line.find("\t"), 1, "    ");
		}
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		LINE l;
		if (line == "") {
			l.line = "";
			l.ident = 0;
		}
		else {
			l.line = line.substr(line.find_first_not_of(" "));
			l.ident = line.find_first_not_of(" ");
		}
		if (l.ident % 2) error_exit(sec, "identation must be in steps of 2.", lines);
		l.ident /= 2;
		l.is_signal = (l.line.substr(0, 1) == "-" || l.line.substr(0, 1) == "+");
		if (l.is_signal) {
			l.signal = get_signal(sec, l.line, false);
		}

		if (l.ident == 0) {
			if (lines.size()) {
				blocks.push_back({clk, block_line, lines});
				lines.clear();
				sections[sec].ald_line = lineno;
			}

			if (l.line == "CLOCK") {
				clk = 2;
				continue;
			}
			if (l.line == "NOCLOCK") {
				clk = 1;
				continue;
			}
			// An explicit combinational wire cone: retain its diagnostic output,
			// but substitute its expression into NOCLOCK consumers in this pass.
			if (l.line == "INLINE") {
				clk = 3;
				continue;
			}
		}
		if (lines.empty()) block_line = lineno;
		lines.push_back(l);
	}
	if (lines.size()) {
		blocks.push_back({clk, block_line, lines});
	}

	std::map<std::string, std::vector<LINE>> cones;
	for (const auto& block : blocks) {
		const auto& body = block.body;
		sections[sec].ald_line = block.line;
		if (block.clock != 3 || !body[0].is_signal) continue;
		if (body[0].signal.second.dimensioned || body[0].signal.second.prefix != sec)
			error_exit(sec, "INLINE requires a local scalar Boolean output.", body);
		for (unsigned int i = 1; i < body.size(); ++i)
			if (!body[i].is_signal && !is_known_word(body[i].line))
				error_exit(sec, "INLINE requires a pure Boolean expression.", body);
		if (!cones.emplace(body[0].signal.first, body).second)
			error_exit(sec, "Duplicate INLINE definition.", body);
	}
	std::function<std::vector<LINE>(const std::vector<LINE>&, std::set<std::string>)> expand;
	expand = [&](const std::vector<LINE>& body, std::set<std::string> active) {
		std::vector<LINE> result;
		for (const auto& input : body) {
			if (input.ident && input.is_signal && input.signal.second.prefix == sec && cones.count(input.signal.first)) {
				if (!active.insert(input.signal.first).second)
					error_exit(sec, "Recursive INLINE cone.", body);
				const auto& definition = cones.at(input.signal.first);
				auto nested = expand(std::vector<LINE>(definition.begin() + 1, definition.end()), active);
				for (auto item : nested) {
					item.ident += input.ident - 1;
					result.push_back(item);
				}
				active.erase(input.signal.first);
			} else result.push_back(input);
		}
		return result;
	};
	// Validate even when a cone is unused or only referenced by CLOCK logic.
	for (const auto& cone : cones)
		expand(std::vector<LINE>(cone.second.begin() + 1, cone.second.end()), {cone.first});
	for (const auto& block : blocks) {
		sections[sec].ald_line = block.line;
		if (!ck || block.clock == ck || (ck == 1 && block.clock == 3))
			func(sec, ck == 1 ? expand(block.body, {}) : block.body);
	}
}
