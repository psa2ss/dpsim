/** CIM Test
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * DPsim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <iostream>
#include <list>

#include "DPsim.h"

using namespace DPsim;
using namespace CPS;
using namespace CPS::Components;
using namespace CPS::Components::DP;

int main(int argc, char *argv[]) {

	// Specify CIM files
#ifdef _WIN32
	String path("..\\..\\..\\..\\dpsim\\Examples\\CIM\\WSCC-09_Neplan_RX\\");
#elif defined(__linux__) || defined(__APPLE__)
	String path("../Examples/CIM/WSCC-09_Neplan_RX/");
#endif

	std::list<String> filenames = {
		path + "WSCC-09_Neplan_RX_DI.xml",
		path + "WSCC-09_Neplan_RX_EQ.xml",
		path + "WSCC-09_Neplan_RX_SV.xml",
		path + "WSCC-09_Neplan_RX_TP.xml"
	};

	String simName = "Shmem_WSCC-9bus_Ctrl";
	
	CIM::Reader reader(simName, Logger::Level::INFO, Logger::Level::INFO);
	SystemTopology sys = reader.loadCIM(50, filenames);

	// Extend system with controllable load
	auto load = PQLoadCS::make("load_cs", Node::List{sys.mNodes[3]}, 0, 0, 230000, Logger::Level::INFO);
	sys.mComponents.push_back(load);

	// Controllers and filter
	std::vector<Real> coefficients = { -0.0024229,-0.0020832,0.0067703,0.016732,
	0.011117,-0.0062311,-0.0084016,0.0092568, 0.012983,-0.010121,-0.018274,0.011432,
	0.026176,-0.012489,-0.037997,0.013389,0.058155,-0.014048,-0.10272,0.014462,0.31717,
	0.48539, 0.31717,0.014462,-0.10272,-0.014048,0.058155,0.013389,-0.037997,-0.012489,
	0.026176,0.011432,-0.018274,-0.010121, 0.012983,0.0092568,-0.0084016,-0.0062311,
	0.011117,0.016732,0.0067703,-0.0020832,-0.0024229 };

	auto filtP = FIRFilter::make("filter_p", coefficients, Logger::Level::INFO);
	filtP->setPriority(1);
	filtP->initialize(0.);
	filtP->setConnection(load->findAttribute<Real>("active_power"));
	filtP->findAttribute<Real>("input")->set(0.);
	
	RealTimeSimulation sim(simName, sys, 0.001, 20,
		Solver::Domain::DP, Solver::Type::MNA, Logger::Level::INFO, true);

	// Create shmem interface
	Interface::Config conf;
	conf.samplelen = 64;
	conf.queuelen = 1024;
	conf.polling = false;
	String in  = "/dpsim10";
	String out = "/dpsim01";
	Interface intf(out, in, &conf);

	// Register exportable node voltages
	Int i = 0, o = 0;
	for (auto n : sys.mNodes) {
		auto v = n->findAttribute<Complex>("voltage");

		std::function<Real()> getMag = [v](){ return std::abs(v->get()); };
		std::function<Real()> getPhas = [v](){ return std::arg(v->get()); };

		intf.addExport(v, 1.0, o++, o++);
		intf.addExport(getMag, o++);
		intf.addExport(getPhas, o++);
	}

	// Register controllable load
	//intf.addImport(load->findAttribute<Real>("active_power"), 1.0, 0);
	intf.addImport(filtP->findAttribute<Real>("input"), 1.0, 0);

	sim.addInterface(&intf);
	sim.run();

	return 0;
}