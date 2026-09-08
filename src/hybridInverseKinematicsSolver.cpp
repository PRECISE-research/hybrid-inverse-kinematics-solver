#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <iterator>
#include <chrono>
#include <iomanip>
#include <cstdlib>

#include <OpenSim.h>
#include <rapidxml.hpp>

struct InverseKinematicsConfigurationFile {
	std::string modelFilePath;

	std::string markersFilePath = "none";
	OpenSim::Set<OpenSim::MarkerWeight> markerWeights;

	std::string orientationsFilePath = "none";
	OpenSim::Set<OpenSim::OrientationWeight> orientationsWeights;
	std::vector<double> orientationRotations = { 0., 0., 0. };  // degrees

	std::string coordinatesFilePath = "none";
	std::map<std::string, double> coordinatesWeights;
	bool inDegrees = true;

	std::string resultsOutputPath;

	bool useVisualizer = false;
	double constraintWeight = SimTK::Infinity;
	double solverAccuracy = 1.e-5;

};

// Warn about elements we do not recognise, so an obsolete tag (applyRotation) or a
// typo is reported instead of being silently ignored.
static void warnUnknownChildren(rapidxml::xml_node<>* parent, const std::string& blockName,
	const std::set<std::string>& known) {
	for (auto child = parent->first_node(); child != NULL; child = child->next_sibling()) {
		if (known.find(child->name()) == known.end())
			std::cout << "Warning: unrecognised element '" << child->name() << "' in "
				<< blockName << " of cfg file, ignored" << std::endl;
	}
}

// Read one "<xWeight name=\"foo\">1.0</xWeight>" entry. Returns false (and warns)
// rather than dereferencing a missing attribute or leaving the value uninitialised.
static bool readWeightNode(rapidxml::xml_node<>* node, const std::string& blockName,
	std::string& nameOut, double& valueOut) {
	auto attr = node->first_attribute("name");
	if (attr == nullptr) {
		std::cout << "Warning: a " << blockName << "->weights entry has no 'name' attribute, skipping" << std::endl;
		return false;
	}
	nameOut = attr->value();
	std::istringstream strm(node->value());
	if (!(strm >> valueOut)) {
		std::cout << "Warning: " << blockName << "->weights entry '" << nameOut
			<< "' has a non-numeric value, skipping" << std::endl;
		return false;
	}
	return true;
}

InverseKinematicsConfigurationFile readCfgFile(std::string path) {
	std::filesystem::path filePath(path);
	// TODO: this doesn't seem to work for everyone (i.e., Matt W)
	std::string parentFolder = filePath.parent_path().string();

	InverseKinematicsConfigurationFile cfgDetails;

	// Open the XML file
	std::ifstream file(path);
	if (!file) {
		std::cout << "Error: failed to open cfg file '" << path << "'" << std::endl;
		exit(EXIT_FAILURE);
	}

	// Read the XML file into a string. Read it whole rather than line by line:
	// concatenating getline() output drops the newlines, which silently glues a tag
	// split across lines onto the next one.
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string xmlContent = buffer.str();

	// Parse the XML content
	rapidxml::xml_document<> doc;
	try {
		doc.parse<0>(&xmlContent[0]);
	}
	catch (const rapidxml::parse_error& e) {
		std::cout << "Error: failed to parse cfg file '" << path << "': " << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}

	// Access the configuration values
	rapidxml::xml_node<>* rootNode = doc.first_node();
	if (rootNode == nullptr) {
		std::cout << "Error: cfg file '" << path << "' contains no XML root element" << std::endl;
		exit(EXIT_FAILURE);
	}

	warnUnknownChildren(rootNode, "the setup file", {
		"modelFilePath", "markers", "orientations", "coordinates",
		"resultsOutputPath", "useVisualizer", "constraintWeight", "solverAccuracy" });

	if (rootNode->first_node("modelFilePath")) {
		std::filesystem::path modelPath(rootNode->first_node("modelFilePath")->value());
		if (modelPath.is_relative())
			cfgDetails.modelFilePath = parentFolder + "/" + modelPath.string();
		else
			cfgDetails.modelFilePath = modelPath.string();
	}
	else {
		std::cout << "Error: no modelFilePath node available in cfg file" << std::endl;
		exit(EXIT_FAILURE);
	}

	if (rootNode->first_node("markers")) {
		// Try to get marker file path
		auto markersNode = rootNode->first_node("markers");
		warnUnknownChildren(markersNode, "markers", { "filePath", "weights" });
		if (markersNode->first_node("filePath")) {
			std::filesystem::path markersPath(markersNode->first_node("filePath")->value());
			if (markersPath.is_relative())
				cfgDetails.markersFilePath = parentFolder + "/" + markersPath.string();
			else
				cfgDetails.markersFilePath = markersPath.string();
		}
		else {
			std::cout << "Error: no markers->filePath node available in cfg file" << std::endl;
			exit(EXIT_FAILURE);
		}

		// Try to get marker weights
		if (markersNode->first_node("weights")) {
			auto weightsNode = markersNode->first_node("weights");
			auto node = weightsNode->first_node();
			while (node != NULL) {
				std::string weightName;
				double weightValue;
				if (readWeightNode(node, "markers", weightName, weightValue)) {
					OpenSim::MarkerWeight markerWeight(weightName, weightValue);
					cfgDetails.markerWeights.cloneAndAppend(markerWeight);
				}

				node = node->next_sibling();
			}
		}
		else {
			std::cout << "Warning: no markers->weights node available in cfg file, default weight of 1 will be used for all" << std::endl;
		}
	}
	else {
		std::cout << "Warning: no markers node available in cfg file, no markers will be tracked" << std::endl;
	}

	if (rootNode->first_node("orientations")) {
		// Try to get orientations file path
		auto orientationsNode = rootNode->first_node("orientations");
		warnUnknownChildren(orientationsNode, "orientations", { "filePath", "weights", "rotation" });
		if (orientationsNode->first_node("filePath")) {
			std::filesystem::path orientationsPath(orientationsNode->first_node("filePath")->value());
			if (orientationsPath.is_relative())
				cfgDetails.orientationsFilePath = parentFolder + "/" + orientationsPath.string();
			else
				cfgDetails.orientationsFilePath = orientationsPath.string();
		}
		else {
			std::cout << "Error: no orientations->filePath node available in cfg file" << std::endl;
			exit(EXIT_FAILURE);
		}

		// Try to get orientations weights
		if (orientationsNode->first_node("weights")) {
			auto weightsNode = orientationsNode->first_node("weights");
			auto node = weightsNode->first_node();
			while (node != NULL) {
				std::string weightName;
				double weightValue;
				if (readWeightNode(node, "orientations", weightName, weightValue)) {
					OpenSim::OrientationWeight markerWeight(weightName, weightValue);
					cfgDetails.orientationsWeights.cloneAndAppend(markerWeight);
				}

				node = node->next_sibling();
			}
		}
		else {
			std::cout << "Warning: no orientations->weights node available in cfg file, default weight of 1 will be used for all" << std::endl;
		}

		// Try to get the sensor-to-OpenSim rotation (degrees, space-fixed XYZ)
		if (orientationsNode->first_node("rotation")) {
			std::istringstream iss(orientationsNode->first_node("rotation")->value());
			auto rotation = std::vector<double>{ std::istream_iterator<double>(iss),
				std::istream_iterator<double>() };
			cfgDetails.orientationRotations = rotation;
			if (cfgDetails.orientationRotations.size() != 3) {
				std::cout << "Error: orientations->rotation expects 3 values, but " << cfgDetails.orientationRotations.size() << " were provided" << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}
	else {
		std::cout << "Warning: no orientations node available in cfg file, no orientations will be tracked" << std::endl;
	}

	if (rootNode->first_node("coordinates")) {
		// Try to get coordinates file path
		auto coordinatesNode = rootNode->first_node("coordinates");
		warnUnknownChildren(coordinatesNode, "coordinates", { "filePath", "weights", "inDegrees" });
		if (coordinatesNode->first_node("filePath")) {
			std::filesystem::path coordinatesPath(coordinatesNode->first_node("filePath")->value());
			if (coordinatesPath.is_relative())
				cfgDetails.coordinatesFilePath = parentFolder + "/" + coordinatesPath.string();
			else
				cfgDetails.coordinatesFilePath = coordinatesPath.string();
		}
		else {
			std::cout << "Error: no coordinates->filePath node available in cfg file" << std::endl;
			exit(EXIT_FAILURE);
		}

		// Try to get coordinates weights
		if (coordinatesNode->first_node("weights")) {
			auto weightsNode = coordinatesNode->first_node("weights");
			auto node = weightsNode->first_node();
			while (node != NULL) {
				std::string weightName;
				double weightValue;
				if (readWeightNode(node, "coordinates", weightName, weightValue)) {
					cfgDetails.coordinatesWeights.insert({ weightName, weightValue });
				}

				node = node->next_sibling();
			}
		}
		else {
			std::cout << "Warning: no coordinates->weights node available in cfg file, default weight of 1 will be used for all" << std::endl;
		}

		// Try to get whether the coordinates are in radians
		if (coordinatesNode->first_node("inDegrees")) {
			if (std::string(coordinatesNode->first_node("inDegrees")->value()) == "true")
				cfgDetails.inDegrees = true;
			else
				cfgDetails.inDegrees = false;
		}
	}
	else {
		std::cout << "Warning: no coordinates node available in cfg file, no coordinates will be tracked" << std::endl;
	}

	if (rootNode->first_node("resultsOutputPath")) {
		std::filesystem::path resultsPath(rootNode->first_node("resultsOutputPath")->value());
		if (resultsPath.is_relative())
			cfgDetails.resultsOutputPath = parentFolder + "/" + resultsPath.string();
		else
			cfgDetails.resultsOutputPath = resultsPath.string();
	}
	else {
		cfgDetails.resultsOutputPath = parentFolder + "/ik_results.sto";
		std::cout << "Warning: no resultsOutputPath node available in cfg file, using '" << cfgDetails.resultsOutputPath << "'" << std::endl;
	}

	if (rootNode->first_node("useVisualizer")) {
		if (std::string(rootNode->first_node("useVisualizer")->value()) == "true")
			cfgDetails.useVisualizer = true;
		else
			cfgDetails.useVisualizer = false;
	}

	if (rootNode->first_node("constraintWeight")) {
		std::istringstream myStrm(rootNode->first_node("constraintWeight")->value());
		myStrm >> cfgDetails.constraintWeight;
	}

	if (rootNode->first_node("solverAccuracy")) {
		std::istringstream myStrm(rootNode->first_node("solverAccuracy")->value());
		myStrm >> cfgDetails.solverAccuracy;
	}

	if (cfgDetails.markersFilePath == "none" && cfgDetails.orientationsFilePath == "none" && cfgDetails.coordinatesFilePath == "none") {
		std::cout << "Error: no markersFilePath, orientationsFilePath or coordinatesFilePath node available in cfg file, nothing to track" << std::endl;
		exit(EXIT_FAILURE);
	}

	return cfgDetails;
}

// `rotation` is a space-fixed XYZ sequence in DEGREES, matching OpenSim's own
// OpenSense setup files. SimTK::Rotation takes radians, so it is converted here.
OpenSim::TimeSeriesTable_<SimTK::Rotation> loadOrientationsData(std::string filePath, std::vector<double> rotation = { 0., 0., 0. }) {
	//To use quaternion data, have to load the quaternion.sto and convert it into rotations//
	//Load orienation data as a quat table 
	OpenSim::TimeSeriesTableQuaternion quatTable(filePath);
	//Have to rotate quaternions into opensim frame
	//Create rotation sequence
	const SimTK::Rotation sensorToOpensim = SimTK::Rotation(SimTK::BodyOrSpaceType::SpaceRotationSequence,
		SimTK::convertDegreesToRadians(rotation[0]), SimTK::XAxis,
		SimTK::convertDegreesToRadians(rotation[1]), SimTK::YAxis,
		SimTK::convertDegreesToRadians(rotation[2]), SimTK::ZAxis);
	// Rotate data so Yaxis is up (like OpenSim) 
	OpenSim::OpenSenseUtilities::rotateOrientationTable(quatTable, sensorToOpensim);
	//Have to convert Quaternion table to rotation data type
	OpenSim::TimeSeriesTable_<SimTK::Rotation> orientationsData = OpenSim::OpenSenseUtilities::convertQuaternionsToRotations(quatTable);

	return orientationsData;
}

// Print the time difference to the terminal in minutes and seconds
static void printComputationTime(std::chrono::high_resolution_clock::time_point startTime,
	std::chrono::high_resolution_clock::time_point endTime) {
	// Millisecond resolution: at second resolution every short trial reported
	// "0 second", which is both wrong-looking and useless for comparing runs.
	const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	const auto minutes = totalMs / 60000;
	const double seconds = (totalMs - minutes * 60000) / 1000.0;
	if (minutes > 0)
		std::cout << minutes << " minute" << (minutes == 1 ? "" : "s") << " ";
	std::cout << std::fixed << std::setprecision(2) << seconds
		<< " second" << (seconds == 1.0 ? "" : "s") << std::defaultfloat << std::endl;
}

void printHeader() {
	std::cout << "\n+-+-+-+-+-+-+\n"
		<< "|H|y|b|r|i|d|\n"
		<< "+-+-+-+-+-+-+-+\n"
		<< "|I|n|v|e|r|s|e|\n"
		<< "+-+-+-+-+-+-+-+-+-+-+\n"
		<< "|K|i|n|e|m|a|t|i|c|s|\n"
		<< "+-+-+-+-+-+-+-+-+-+-+\n"
		<< "|S|o|l|v|e|r|\n"
		<< "+-+-+-+-+-+-+" << std::endl;
	std::string progName = "hybrid-inverse-kinematics-solver";
	std::string version = std::string(PROJECT_MAJOR_VERSION) + "." + std::string(PROJECT_MINOR_VERSION) + "."
		+ std::string(PROJECT_PATCH_VERSION);
	std::cout << "\n" << progName << " version " << version << std::endl;
	std::cout << "Software developers: Matthew Hambly" << std::endl << std::endl;
}

void printUsage() {
	std::cout << "Option            Argument          Action / Notes\n";
	std::cout << "------            --------          --------------\n";
	std::cout << "-Help, -H                           Print the command-line options for "
		<< "hybrid-inverse-kinematics-solver" << ".\n";
	std::cout << "-Setup, -S        SetupFileName     Specify an xml setup file.\n";
}

int main(int argc, char** argv) {
	printHeader();
	////////////////////////// Get setup file from input //////////////////////////
	std::string option = "";
	std::string cfgFileName;
	if (argc < 2) {
		printUsage();
		return 0;
	}
	else {
		for (auto i(1); i <= (argc - 1); i++) {
			option = argv[i];
			if ((option == "-help") || (option == "-h") || (option == "-Help") || (option == "-H")
				|| (option == "-usage") || (option == "-u") || (option == "-Usage")
				|| (option == "-U")) {
				printUsage();
				return 0;
			}
			else if ((option == "-S") || (option == "-Setup")) {
				if (argv[i + 1] == 0) {
					std::cout << "No setup file specified!" << std::endl;
					printUsage();
					return -1;
				}
				cfgFileName = argv[i + 1];
				break;
			}
			else {
				std::cout << "Unrecognized option " << option << " on command line... Ignored"
					<< std::endl;
				// Add usage message here
				return -1;
			}
		}
	}

	// Diagnostic-only: match OpenSim::InverseKinematicsTool's per-frame assembler
	// diagnostics (normerr/maxerr/cost) when IK_DEBUG_LOG=1, for direct comparison
	// against `opensim-cmd --log=debug`. Does not affect normal runs.
	if (std::getenv("IK_DEBUG_LOG") != nullptr)
		OpenSim::Logger::setLevelString("debug");

	auto ikCfg = readCfgFile(cfgFileName);

	std::vector<double> timeSteps;

	// Load marker data if available
	OpenSim::MarkersReference mRefs;
	if (ikCfg.markersFilePath != "none") {
		// Load marker data from a .trc file
		try {
			// (matches OpenSim::InverseKinematicsTool's own marker-loading path exactly,
			// rather than reading the file ourselves)
			mRefs.initializeFromMarkersFile(ikCfg.markersFilePath, ikCfg.markerWeights);
			timeSteps = mRefs.getMarkerTable().getIndependentColumn();
		}
		catch (const std::exception& e) {
			std::cerr << "Error: failed to load marker data '" << ikCfg.markersFilePath << "': " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	// Load orientations data if available
	OpenSim::OrientationsReference oRefs;
	OpenSim::TimeSeriesTable_<SimTK::Rotation> orientationsData;
	if (ikCfg.orientationsFilePath != "none") {
		// Load orientations data
		try {
			orientationsData = loadOrientationsData(ikCfg.orientationsFilePath, ikCfg.orientationRotations);
			std::vector<std::string> orientationNames = orientationsData.getColumnLabels();
		
			if (timeSteps.empty()) {
				timeSteps = orientationsData.getIndependentColumn();
			}
			else {
				bool timeStepsMatch = true;
				// Check whether time steps match between markers and orientations
				if (timeSteps.size() != orientationsData.getIndependentColumn().size()) {
					timeStepsMatch = false;
				}
				else {
					for (auto i(0); i < timeSteps.size(); ++i) {
						if (std::abs(timeSteps[i] - orientationsData.getIndependentColumn()[i]) > 1e-6) {
							timeStepsMatch = false;
							break;
						}
					}
				}

				if (!timeStepsMatch) {
					std::cout << "Warning: time steps in marker data do not match time steps in orientations data, resampling orientations data to match marker data time steps." << std::endl;
					OpenSim::TimeSeriesTable_<SimTK::Rotation> orientationsDataNew;
					orientationsDataNew.setColumnLabels(orientationNames);
					for (auto time : timeSteps)
						orientationsDataNew.appendRow(time, orientationsData.getNearestRow(time));
					// Without this the resampled table was discarded and the solve ran
					// against the original, misaligned orientations.
					orientationsData = orientationsDataNew;
				}
			}
		
			oRefs = OpenSim::OrientationsReference(orientationsData, &ikCfg.orientationsWeights);
		}
		catch (const std::exception& e) {
			std::cerr << "Error: failed to load orientations data '" << ikCfg.orientationsFilePath << "': " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	// Load coordinates data if available
	SimTK::Array_<OpenSim::CoordinateReference> cRefs;
	OpenSim::TimeSeriesTable_<double> coordinatesData;
	if (ikCfg.coordinatesFilePath != "none") {
		try {
			for (const auto& coordinate : ikCfg.coordinatesWeights) {
				std::string name = coordinate.first;
				double weight = coordinate.second;
				OpenSim::CoordinateReference ref(name, OpenSim::Constant(0));
				ref.setWeight(weight);
				cRefs.push_back(ref);
			}

			// TODO: should check whether coordinates in the file match those listed in the setup xml and warn if not.

			coordinatesData = OpenSim::TimeSeriesTable_<double>(ikCfg.coordinatesFilePath);
		}
		catch (const std::exception& e) {
			std::cerr << "Error: failed to load coordinates data '" << ikCfg.coordinatesFilePath << "': " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
		
		if (timeSteps.empty()) {
			timeSteps = coordinatesData.getIndependentColumn();
		}
		else {
			// TODO: should check whether time steps match between markers and orientations.
			// This shouldn't matter for coordinates because we use getNearestRow in the loop, but it would could be good to check and warn if they are very different.
		}
	}

	// Load model
	OpenSim::Model model;
	try {
		model = OpenSim::Model(ikCfg.modelFilePath);
	}
	catch (const std::exception& e) {
		std::cerr << "Error: failed to load model '" << ikCfg.modelFilePath << "': " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	catch (...) {
		std::cerr << "Error: failed to load model '" << ikCfg.modelFilePath << "': unknown error" << std::endl;
		return EXIT_FAILURE;
	}
		
	model.setUseVisualizer(ikCfg.useVisualizer);
	SimTK::State state = model.initSystem();
	
	// Create IK solver
	OpenSim::InverseKinematicsSolver ikSolver(model, mRefs, oRefs, cRefs, ikCfg.constraintWeight);
	ikSolver.setAccuracy(ikCfg.solverAccuracy);

	// Setup the visualizer window
	if (ikCfg.useVisualizer) {
		SimTK::Visualizer viz = model.updVisualizer().updSimbodyVisualizer();
		viz.setBackgroundType(SimTK::Visualizer::BackgroundType::SolidColor);
		viz.setBackgroundColor(SimTK::Black);
		model.getVisualizer().show(state);
		model.updVisualizer().updSimbodyVisualizer().setMode(SimTK::Visualizer::Mode::RealTime);
		model.getVisualizer().getSimbodyVisualizer().setShowSimTime(true);
	}

	// Prepare table for results
	OpenSim::TimeSeriesTable results;
	const auto& coordSet = model.getCoordinateSet();
	std::vector<std::string> columnLabels;
	for (int i = 0; i < coordSet.getSize(); ++i)
		columnLabels.push_back(coordSet[i].getName());
	results.setColumnLabels(columnLabels);
	results.addTableMetaData<std::string>("inDegrees", "yes");

	// Prepare table for marker errors if tracking markers
	SimTK::Array_<std::string> markersInUse;
	for (auto m(0); m < mRefs.getNames().size(); ++m) {
		std::string name = mRefs.getNames()[m];
		if (model.getMarkerSet().contains(name))
			markersInUse.push_back(name);
		else
			std::cout << "  Warning: Marker '" << name << "' not found in model. Check your marker names to ensure they match the model." << std::endl;
	}
	OpenSim::TimeSeriesTable markerErrorsTable;
	markerErrorsTable.setColumnLabels(markersInUse);

	// Prepare table for orientation errors if tracking orientations
	OpenSim::TimeSeriesTable orientationsErrorsTable;

	std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();// Start timer
	for (auto t(0); t < timeSteps.size(); ++t) {
		// Update coordinates if provided
		if (coordinatesData.getIndependentColumn().size() > 0. && cRefs.size() > 0) {
			auto coords = coordinatesData.getNearestRow(timeSteps[t]);
			std::vector<std::string> coordNames = coordinatesData.getColumnLabels();
			for (auto i(0); i < coordNames.size(); i++) {
				std::string name = coordNames[i];
				// inDegrees applies to rotational coordinates only. Translational
				// coordinates are in metres and must not be scaled -- converting them
				// shrinks the prescribed value by 180/pi. This mirrors the motion-type
				// check already used when writing results out.
				int coordIndex = coordSet.getIndex(name);
				bool isRotational = (coordIndex >= 0) &&
					(coordSet[coordIndex].getMotionType() == OpenSim::Coordinate::MotionType::Rotational);

				double value;
				if (ikCfg.inDegrees && isRotational)
					value = SimTK::convertDegreesToRadians(coords[i]);
				else
					value = coords[i];

				// updateCoordinateReference's third parameter defaults to 1.0, so calling
				// it without a weight silently discards the weight set on the
				// CoordinateReference at construction, every frame. Pass it through.
				auto weightIt = ikCfg.coordinatesWeights.find(name);
				double coordWeight = (weightIt != ikCfg.coordinatesWeights.end())
					? weightIt->second : 1.0;
				ikSolver.updateCoordinateReference(name, value, coordWeight);
			}
		}

		// Run IK solver for this time step
		state.setTime(timeSteps[t]);
		try {
			if (t == 0) ikSolver.assemble(state);
			ikSolver.track(state);
			model.realizePosition(state);
		}
		catch (const std::exception& e) {
			std::cerr << "Error: solver failed at time " << timeSteps[t] << ": " << e.what() << std::endl;
			return EXIT_FAILURE;
		}

		// Diagnostic-only: print the Assembler's actual constraint-weight/error-norm
		// state after the first frame and exit, when IK_DIAG_ONLY=1. Does not affect
		// normal runs.
		if (t == 0 && std::getenv("IK_DIAG_ONLY") != nullptr) {
			const SimTK::Assembler& asmb = ikSolver.getAssembler();
			std::cout << "[IK_DIAG_ONLY] getSystemConstraintsWeight() = " << asmb.getSystemConstraintsWeight() << std::endl;
			std::cout << "[IK_DIAG_ONLY] calcCurrentErrorNorm()      = " << asmb.calcCurrentErrorNorm() << std::endl;
			std::cout << "[IK_DIAG_ONLY] getErrorToleranceInUse()    = " << asmb.getErrorToleranceInUse() << std::endl;
			std::cout << "[IK_DIAG_ONLY] getAccuracyInUse()          = " << asmb.getAccuracyInUse() << std::endl;
			std::cout << "[IK_DIAG_ONLY] ikCfg.constraintWeight      = " << ikCfg.constraintWeight << std::endl;
			return EXIT_SUCCESS;
		}

		// Compute marker errors if tracking markers
		if (ikCfg.markersFilePath != "none") {
			SimTK::Array_<SimTK::Real> markerErrors;
			ikSolver.computeCurrentMarkerErrors(markerErrors);
			markerErrorsTable.appendRow(timeSteps[t], markerErrors);
		}

		// Compute orientation errors if tracking orientations
		if (ikCfg.orientationsFilePath != "none") {
			SimTK::Array_<std::string> orientationsInUse;
			SimTK::Array_<SimTK::Real> orientationErrors;
			for (auto o(0); o < oRefs.getNames().size(); ++o) {
				std::string name = oRefs.getNames()[o];
				try {
					orientationErrors.push_back(ikSolver.computeCurrentOrientationError(name));
					orientationsInUse.push_back(name);
				}
				catch (...) {
					if (t == 0)
						std::cout << "  Warning: Orientation '" << name << "' not found in model. Check your orientation names to ensure they match the model." << std::endl;
				}
			}
			if (t == 0)
				orientationsErrorsTable.setColumnLabels(orientationsInUse);
			orientationsErrorsTable.appendRow(timeSteps[t], orientationErrors);
		}

		// Collect coordinate values for this time (rotational coordinates are
		// converted to degrees to match the inDegrees=yes table metadata above,
		// consistent with OpenSim's own .mot output convention)
		SimTK::RowVector row(coordSet.getSize());
		for (int i = 0; i < coordSet.getSize(); ++i) {
			double value = coordSet[i].getValue(state);
			if (coordSet[i].getMotionType() == OpenSim::Coordinate::MotionType::Rotational)
				value = SimTK::convertRadiansToDegrees(value);
			row[i] = value;
		}
		results.appendRow(timeSteps[t], row);

		// Visualize model
		if (ikCfg.useVisualizer)
			model.getVisualizer().show(state);
	}
	std::chrono::high_resolution_clock::time_point stopTime = std::chrono::high_resolution_clock::now();// Stop timer for evaluation
	std::cout << "  Time taken: ";
	printComputationTime(startTime, stopTime);

	// Write to .sto file
	std::cout << "Saving output to " << ikCfg.resultsOutputPath << std::endl;
	OpenSim::STOFileAdapter::write(results, ikCfg.resultsOutputPath);

	// Write marker and orientation errors to .sto files if tracking markers and orientations, respectively
	std::string baseOutputPath = std::filesystem::path(ikCfg.resultsOutputPath).parent_path().string();
	if (ikCfg.markersFilePath != "none")
		OpenSim::STOFileAdapter::write(markerErrorsTable, baseOutputPath + "/marker_errors.sto");
	if (ikCfg.orientationsFilePath != "none")
		OpenSim::STOFileAdapter::write(orientationsErrorsTable, baseOutputPath + "/orientation_errors.sto");

	return 0;	

}