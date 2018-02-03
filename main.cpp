#include <windows.h>
#include <sstream>
#include <vector>
#include <math.h>
#include <SFML/Graphics.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/range/join.hpp>

using namespace std;

inline int round(double x) {
	return floor(x + 0.5);
}

const int GRID_CELL_WIDTH = 16;

const int MODE_NULL = 0;
const int MODE_BUILD = 1;

const int BUILDINGTYPE_NEXUS = 0;
const int BUILDINGTYPE_NODE = 1;
const int BUILDINGTYPE_GENERATOR = 2;

const int NEXUS_MAXHEALTH = 2000;
const int NEXUS_MASSCOST = 100000;
const float NEXUS_BUILD_MASSDRAW = 10;
const float NEXUS_BUILD_ENERGYDRAW = 20;
const float NEXUS_ENERGY_PROVIDED = 20;

const int NODE_MAXHEALTH = 100;
const float NODE_MASSCOST = 2000;
const float NODE_BUILD_MASSDRAW = 10;
const float NODE_BUILD_ENERGYDRAW = 3;

const int GENERATOR_MAXHEALTH = 300;
const int GENERATOR_MASSCOST = 10000;
const float GENERATOR_BUILD_MASSDRAW = 10;
const float GENERATOR_BUILD_ENERGYDRAW = 10;
const float GENERATOR_ENERGY_PROVIDED = 10;

const int NODE_CONNECTION_MAXLENGTH = 300;

float getMagnitude(sf::Vector2f v) {
	return sqrt((v.x*v.x) + (v.y*v.y));
}
float getMagnitude(sf::Vector2i v) {
	return getMagnitude(sf::Vector2f(v));
}

sf::Font font;

class Grid {
	int cellWidth;
public:
	void setup(int _cellWidth) {
		cellWidth = _cellWidth;
	}
	sf::Vector2i getClosestGridPoint(sf::Vector2f realPos) {
		sf::Vector2f divided = realPos/(float)cellWidth;
		return sf::Vector2i(round(divided.x), round(divided.y));
	}
	sf::Vector2i getClosestGridPoint(sf::Vector2i realPos) {
		return getClosestGridPoint(sf::Vector2f(realPos));
	}
	sf::Vector2f getRealPos(sf::Vector2i gridPoint) {
		return sf::Vector2f(gridPoint*cellWidth);
	}
} grid;

sf::Vector2f toDrawPos(sf::Vector2f realPos) {
	return realPos + sf::Vector2f(0.375,0.375);
}

struct Resources {
	float mass;
	float energy;
	Resources(float _mass, float _energy) : mass(_mass), energy(_energy) {}
};

class Player;

class Building {
protected:
	boost::weak_ptr<Player> owner;
	float health;
	sf::Vector2i gridPoint;
	int width;
	float massBuilt;
	bool active;
	bool ghost;
	bool built;
public:
	Building(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, int _width, bool _ghost) {
		owner = _owner;
		gridPoint = _gridPoint;
		width = _width;
		ghost = _ghost;
		massBuilt = 0;
		health = 0;
		active = false;
		built = false;
	}
	void setOwner(boost::weak_ptr<Player> _owner) {
		owner = _owner;
	}
	boost::weak_ptr<Player> getOwner() {
		return owner;
	}
	void magicallyComplete() {
		massBuilt = getBuildMassTarget();
		health = getMaxHealth();
		active = true;
		built = true;
	}
	virtual int getMaxHealth() {return 0;}
	virtual Resources getBuildResourceDraw() {return Resources(0,0);}
	virtual int getBuildMassTarget() {return 0;}
	virtual float getEnergyDraw() {return 0;}
	virtual float supplyEnergy(float supplyRatio) {return 0;}
	Resources build(float buildAmount) {
		Resources requestedResourceDraw = getBuildResourceDraw();
		float massBuiltThisFrame = requestedResourceDraw.mass * buildAmount;
		float energySpentOnBuildThisFrame = requestedResourceDraw.energy * buildAmount;
		massBuilt += massBuiltThisFrame;
		health += (massBuiltThisFrame / getBuildMassTarget()) * getMaxHealth();
		if (massBuilt >= getBuildMassTarget()) {
			massBuilt = getBuildMassTarget();
			built = true;
			activate();
		}
		return Resources(massBuiltThisFrame, energySpentOnBuildThisFrame);
	}
	void unGhost() {
		ghost = false;
	}
	bool isGhost() {
		return ghost;
	}
	bool isBuilt() {
		return built;
	}
	void activate() {
		active = true;
	}
	bool isActive() {
		return active;
	}
	void setMassBuilt(float _massBuilt) {
		massBuilt = _massBuilt;
	}
	void setGridPoint(sf::Vector2i newGridPoint) {
		gridPoint = newGridPoint;
	}
	sf::Vector2i getGridPoint() {
		return gridPoint;
	}
	sf::Vector2f getCenterPos() {
		sf::Vector2i bottomRightGridPoint(gridPoint.x + width, gridPoint.y + width);
		return grid.getRealPos(gridPoint + bottomRightGridPoint) / 2.f;
	}
	sf::Vector2f getPos() {
		return getCenterPos();
	}
	virtual void go() {}
	void drawBackground(sf::RenderWindow *window, sf::Color color) {
		sf::Vertex backgroundQuad[] = {
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint)), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x+width, gridPoint.y))), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x+width, gridPoint.y+width))), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x, gridPoint.y+width))), color)
		};
		window->draw(backgroundQuad, 4, sf::Quads);
	}
	virtual void drawDesign(sf::RenderWindow *window) {}
	void drawOutline(sf::RenderWindow *window, sf::Color color) {
		sf::Vertex outline[] = {
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint)), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x+width, gridPoint.y))), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x+width, gridPoint.y+width))), color),
			sf::Vertex(toDrawPos(grid.getRealPos(sf::Vector2i(gridPoint.x, gridPoint.y+width))), color),
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint)), color)
		};
		window->draw(outline, 5, sf::LinesStrip);
	}
	void drawHealthBar(sf::RenderWindow *window) {
		float healthFraction = health / getMaxHealth();
		sf::Color color = sf::Color((1-healthFraction)*255, healthFraction*255, 0);
		sf::Vertex healthBar[] = {
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint) + sf::Vector2f(1, 1)), color),
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint) + sf::Vector2f(width*healthFraction*GRID_CELL_WIDTH, 1)), color),
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint) + sf::Vector2f(width*healthFraction*GRID_CELL_WIDTH, 4)), color),
			sf::Vertex(toDrawPos(grid.getRealPos(gridPoint) + sf::Vector2f(1, 4)), color)
		};
		window->draw(healthBar, 4, sf::Quads);
	}
	void draw(sf::RenderWindow *window, sf::Color outlineColor) {
		if (!isGhost()) {
			drawBackground(window, sf::Color(50,50,50));
		}
		drawOutline(window, outlineColor);
		drawDesign(window); // defined in daughter classes
		drawHealthBar(window);
	}
	void draw(sf::RenderWindow *window) {
		draw(window, sf::Color(150,150,255));
	}
	void drawGhost(sf::RenderWindow *window) {
		draw(window, sf::Color(150,150,150,255));
	}
};

vector<boost::shared_ptr<Building>> buildings;
boost::shared_ptr<Building> cursorBuilding;

template <class BuildingClass>
vector<boost::shared_ptr<BuildingClass>> findNearbyBuildings(vector<boost::shared_ptr<Building>> *buildingVector, sf::Vector2f pos, int maxRange, bool mustBeActive) {
	vector<boost::shared_ptr<BuildingClass>> nearbyBuildings;
	for (int i=0; i<buildingVector->size(); i++) {
		if (mustBeActive && !((*buildingVector)[i]->isActive()))
			continue;
		boost::shared_ptr<BuildingClass> specifiedBuilding = boost::dynamic_pointer_cast<BuildingClass, Building>((*buildingVector)[i]);
		if (getMagnitude((*buildingVector)[i]->getPos() - pos) < maxRange && (specifiedBuilding)) {
			nearbyBuildings.push_back(specifiedBuilding);
		}
	}
	return nearbyBuildings;
}

class Network;

class EnergyProviderBaseClass : public virtual Building {
public:
	EnergyProviderBaseClass(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, int _width, bool _ghost)
		: Building(_owner, _gridPoint, _width, _ghost) {}
	virtual float getEnergyProvided() {return 0;}
};

class Generator : public EnergyProviderBaseClass {
public:
	Generator(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, bool _ghost)
		: EnergyProviderBaseClass(_owner, _gridPoint, 2, _ghost),
		  Building(_owner, _gridPoint, 2, _ghost) {}
	float getEnergyProvided() {
		return GENERATOR_ENERGY_PROVIDED;
	}
	int getMaxHealth() {
		return GENERATOR_MAXHEALTH;
	}
	Resources getBuildResourceDraw() {
		return Resources(GENERATOR_BUILD_MASSDRAW, GENERATOR_BUILD_ENERGYDRAW);
	}
	int getBuildMassTarget() {
		return GENERATOR_MASSCOST;
	}
	void drawDesign(sf::RenderWindow *window) {
		sf::Color arrowColor(255,255,0);
		sf::Vertex upArrow[] = {
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f(-3, 3)), arrowColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f(0, -3)), arrowColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f(3, 3)), arrowColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f(0, -3)), arrowColor),
		};
		window->draw(upArrow, 4, sf::Lines);
	}
};

class NodeBaseClass : public virtual Building {
protected:
	unsigned int distanceScore;
public:
	//boost::weak_ptr<Network> network;
	vector<boost::weak_ptr<Building>> connectedBuildings;
	NodeBaseClass(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, int _width, bool _ghost)
		: Building(_owner, _gridPoint, _width, _ghost) {}
	void setDistanceScore(unsigned int _score) {
		distanceScore = _score;
	}
	unsigned int getDistanceScore() {
		return distanceScore;
	}
	virtual void go() {
		Building::go();
	}
	void drawConnections(sf::RenderWindow *window, sf::Color color) {
		for (int i=0; i<connectedBuildings.size(); i++) {
			if (boost::shared_ptr<Building> connectedBuilding = connectedBuildings[i].lock()) {
				sf::Vertex line[] = {
					sf::Vertex(toDrawPos(getCenterPos())),
					sf::Vertex(toDrawPos(connectedBuilding->getCenterPos()))
				};
				window->draw(line, 2, sf::Lines);
			}
		}
	}
};

class Node : public NodeBaseClass {
public:
	Node(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, bool _ghost)
		: NodeBaseClass(_owner, _gridPoint, 1, _ghost),
		  Building(_owner, _gridPoint, 1, _ghost) {}
	int getMaxHealth() {
		return NODE_MAXHEALTH;
	}
	int getBuildMassTarget() {
		return NODE_MASSCOST;
	}
	Resources getBuildResourceDraw() {
		return Resources(NODE_BUILD_MASSDRAW, NODE_BUILD_ENERGYDRAW);
	}
	virtual void go() {
		NodeBaseClass::go();
	}
	void drawDesign(sf::RenderWindow *window) {
		if (isActive()) {
			sf::Text text;
			text.setFont(font);
			text.setCharacterSize(12);
			text.setColor(sf::Color::Yellow);
			text.setPosition(toDrawPos(getCenterPos()).x, toDrawPos(getCenterPos()).y-30);

			stringstream s;
			s << getDistanceScore();

			text.setString(s.str());
			window->draw(text);
		}
	}
};

class Nexus : public NodeBaseClass, public EnergyProviderBaseClass {
	int minerals;
public:
	Nexus(boost::weak_ptr<Player> _owner, sf::Vector2i _gridPoint, bool _ghost)
		: NodeBaseClass(_owner, _gridPoint, 3, _ghost),
		  EnergyProviderBaseClass(_owner, _gridPoint, 3, _ghost),
		  Building(_owner, _gridPoint, 3, _ghost) {}
	int getMaxHealth() {
		return NEXUS_MAXHEALTH;
	}
	int getMassBuildTarget() {
		return NEXUS_MASSCOST;
	}
	Resources getBuildResourceDraw() {
		return Resources(NEXUS_BUILD_MASSDRAW, NEXUS_BUILD_ENERGYDRAW);
	}
	float getEnergyProvided() {
		return NEXUS_ENERGY_PROVIDED;
	}
	void go() {
		NodeBaseClass::go();
	}
	void drawDesign(sf::RenderWindow *window) {
		sf::Color diamondColor(255,0,255);
		sf::Vertex diamond[] = {
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f( 0,-6)), diamondColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f( 3, 0)), diamondColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f( 0, 6)), diamondColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f(-3, 0)), diamondColor),
			sf::Vertex(toDrawPos(getCenterPos() + sf::Vector2f( 0,-6)), diamondColor)
		};
		window->draw(diamond, 5, sf::LinesStrip);
	}
};

class Network {
	boost::weak_ptr<Player> owner;
	vector<boost::shared_ptr<Building>> connectedBuildings;
	boost::shared_ptr<NodeBaseClass> networkCenter;
	vector<boost::shared_ptr<NodeBaseClass>> activeNodes;
	bool connectedToNexus;
public:
	float energyAvailable, energySpent, massAvailable, massSpent, energyProfit;
	Network(boost::weak_ptr<Player> _owner, boost::shared_ptr<NodeBaseClass> _networkCenter) {
		owner = _owner;
		networkCenter = _networkCenter;

		assert(networkCenter->isActive());
		activeNodes.push_back(networkCenter);
		connectedBuildings.push_back(networkCenter);

		networkCenter->setDistanceScore(0);
		if (boost::dynamic_pointer_cast<Nexus, NodeBaseClass>(networkCenter)) {
			connectedToNexus = true;
		}
		else connectedToNexus = false;

		energyAvailable = energySpent = massAvailable = massSpent = energyProfit = 0;
	}
	void go();
};

class Player {
public:
	vector<boost::shared_ptr<Building>> ownedBuildings;
	vector<boost::shared_ptr<Building>> ghostBuildings;
	vector<boost::shared_ptr<Network>> networks;
};

vector<boost::shared_ptr<Player>> players;

vector<boost::shared_ptr<NodeBaseClass>> getActiveNodesWithinRange(boost::shared_ptr<Player> player, sf::Vector2f pos) {
	return findNearbyBuildings<NodeBaseClass>(&(player->ownedBuildings), pos, NODE_CONNECTION_MAXLENGTH, true);
}

void registerNewGhostBuilding(boost::shared_ptr<Player> player, boost::shared_ptr<Building> ghostBuilding) {
	//find nearby active nodes and connect them to the ghostBuilding
	vector<boost::shared_ptr<NodeBaseClass>> nodes = getActiveNodesWithinRange(player, ghostBuilding->getPos());
	for (int i=0; i<nodes.size(); i++) {
		nodes[i]->connectedBuildings.push_back(ghostBuilding);
	}
}

void Network::go() {
	boost::shared_ptr<Player> networkOwner = owner.lock();

	float energyIncome = 0;
	for (int i=0; i<connectedBuildings.size(); i++) {
		if (boost::shared_ptr<EnergyProviderBaseClass> eProv = boost::dynamic_pointer_cast<EnergyProviderBaseClass, Building>(connectedBuildings[i])) {
			if (eProv->isActive()) {
				energyIncome += eProv->getEnergyProvided();
			}
		}
	}
		
	float energyAvailableFromStorage = 0;//determine energy available from batteries
		
	energyAvailable = energyIncome + energyAvailableFromStorage;
	massAvailable = 100;

	bool networkCanBuild = (connectedToNexus && massAvailable > 0);

	//Unghost any buildings attached to active nodes.
	for (int i=0; i<activeNodes.size(); i++) {
		for (int j=0; j<activeNodes[i]->connectedBuildings.size(); j++) {
			boost::shared_ptr<Building> possiblyGhostBuilding = activeNodes[i]->connectedBuildings[j].lock();
			if (!possiblyGhostBuilding) continue; // This indicates the node's connection weak_ptr points to a destroyed building

			if (possiblyGhostBuilding->isGhost()) {
				possiblyGhostBuilding->unGhost();

				networkOwner->ghostBuildings.erase(remove(networkOwner->ghostBuildings.begin(), networkOwner->ghostBuildings.end(), possiblyGhostBuilding), networkOwner->ghostBuildings.end());

				buildings.push_back(possiblyGhostBuilding);//add to global buildings list
				networkOwner->ownedBuildings.push_back(possiblyGhostBuilding);//add to player's buildings list
				connectedBuildings.push_back(possiblyGhostBuilding);//add to network's buildings list
			}
		}
	}

	float energyRequested = 0;
	float massRequested = 0;
	for (int i=0; i<connectedBuildings.size(); i++) {
		energyRequested += connectedBuildings[i]->getEnergyDraw();
		if (networkCanBuild && !connectedBuildings[i]->isBuilt()) {
			Resources r = connectedBuildings[i]->getBuildResourceDraw();
			energyRequested += r.energy;
			massRequested += r.mass;
		}
	}

	float energySatisfaction = energyRequested>0 ? min(1.0, energyAvailable/energyRequested) : 1.0;
	float massSatisfaction = (networkCanBuild && massRequested>0) ? min(1.0, massAvailable/massRequested) : 1.0;

	massSpent = 0;
	energySpent = 0;

	for (int i=0; i<connectedBuildings.size(); i++) {
		if (connectedBuildings[i]->isActive()) {
			energySpent += connectedBuildings[i]->supplyEnergy(energySatisfaction);
		}
		else if (networkCanBuild && !connectedBuildings[i]->isBuilt()) {
			Resources spent = connectedBuildings[i]->build(min(energySatisfaction, massSatisfaction));
			massSpent += spent.mass;
			energySpent += spent.energy;

			if (connectedBuildings[i]->isBuilt()) {
				//If the building was just built, activate and connect it if it's a node
				if (boost::shared_ptr<NodeBaseClass> node = boost::dynamic_pointer_cast<NodeBaseClass, Building>(connectedBuildings[i])) {
					activeNodes.push_back(node);
							
					//Add connections to nearby buildings and ghostBuildings
					vector<boost::shared_ptr<Building>> nearbyRealBuildings = findNearbyBuildings<Building>(&(networkOwner->ownedBuildings), node->getPos(), NODE_CONNECTION_MAXLENGTH, false);
					vector<boost::shared_ptr<Building>> nearbyGhostBuildings = findNearbyBuildings<Building>(&(networkOwner->ghostBuildings), node->getPos(), NODE_CONNECTION_MAXLENGTH, false);
					auto allNearbyBuildings = boost::join(nearbyRealBuildings, nearbyGhostBuildings);
						
					//look for the lowest nearby distanceScore to get local distanceScore
					unsigned int lowestDistanceScore = 65535; // Max value for unsigned int
					for (int j=0; j<allNearbyBuildings.size(); j++) {
						if (allNearbyBuildings[j].get() == node.get()) continue;

						node->connectedBuildings.push_back(boost::weak_ptr<Building>(allNearbyBuildings[j]));

						if (boost::shared_ptr<NodeBaseClass> otherNode = boost::dynamic_pointer_cast<NodeBaseClass, Building>(allNearbyBuildings[j])) {
								
							if (otherNode->getDistanceScore() < lowestDistanceScore) {
								lowestDistanceScore = otherNode->getDistanceScore();
							}
						}
					}
					node->setDistanceScore(lowestDistanceScore + 1);

					//Now iterate through all connected nodes to update their score if it's now higher than it should be
					vector<boost::shared_ptr<NodeBaseClass>> nodesUpdatedLastLoop;
					nodesUpdatedLastLoop.push_back(node);
					while (nodesUpdatedLastLoop.size() > 0) {
						vector<boost::shared_ptr<NodeBaseClass>> nodesUpdatedThisLoop;
						for (int j=0; j<nodesUpdatedLastLoop.size(); j++) {
							for (int k=0; k<nodesUpdatedLastLoop[j]->connectedBuildings.size(); k++) {

								//First make sure we haven't already updated this building
								bool inList = false;
								for (int l=0; l<nodesUpdatedThisLoop.size(); l++) {
									if (nodesUpdatedLastLoop[j]->connectedBuildings[k].lock().get() == nodesUpdatedThisLoop[l].get()) {
										inList = true;
										break;
									}
								}
								if (inList)
									continue;

								//If it's a node and the distance is > this node's distanceScore + 1, then recalculate distance score and add to nodesUpdatedThisLoop
								if (boost::shared_ptr<NodeBaseClass> connectedNode = boost::dynamic_pointer_cast<NodeBaseClass, Building>(nodesUpdatedLastLoop[j]->connectedBuildings[k].lock())) {
									if (connectedNode->getDistanceScore() > nodesUpdatedLastLoop[j]->getDistanceScore() + 1) {
										connectedNode->setDistanceScore(nodesUpdatedLastLoop[j]->getDistanceScore() + 1);
										nodesUpdatedThisLoop.push_back(connectedNode);
									}
								}
							}
						}
						nodesUpdatedLastLoop = nodesUpdatedThisLoop;
					}
				}
			}
		}
	}

	energyProfit = energyIncome - energySpent;
	//store or remove from storage
}

void setup() {
	grid.setup(GRID_CELL_WIDTH);

	font.loadFromFile("tahoma.ttf");
}

int mode;
int buildType;

boost::shared_ptr<Player> selectedPlayer;

//sf::RenderWindow window(sf::VideoMode(1366, 768, 32), "Nodes", sf::Style::Fullscreen);
sf::RenderWindow window(sf::VideoMode(1920, 1080, 32), "Nodes", sf::Style::Fullscreen);

void start() {
	for (int i=0; i<9; i++) {
		players.push_back(boost::shared_ptr<Player>(new Player()));
	}

	selectedPlayer = players.front();

	boost::shared_ptr<Nexus> nexus = boost::shared_ptr<Nexus>(new Nexus(players[0], sf::Vector2i(15,15), false));
	nexus->magicallyComplete();

	buildings.push_back(nexus);
	selectedPlayer->ownedBuildings.push_back(nexus);

	players[0]->networks.push_back(boost::shared_ptr<Network>(new Network(players[0], nexus)));

	mode = MODE_NULL;
	buildType = BUILDINGTYPE_NEXUS;
}

void changeMode(int newMode) {
	if (newMode == MODE_BUILD) {
		window.setMouseCursorVisible(false);
		cursorBuilding = boost::shared_ptr<Building>(new Nexus(boost::shared_ptr<Player>(), sf::Vector2i(0,0), true));
	}
	else {
		window.setMouseCursorVisible(true);
	}
	mode = newMode;
}

void changeBuildType(int newBuildType) {
	if (newBuildType == BUILDINGTYPE_NEXUS) {
		cursorBuilding = boost::shared_ptr<Nexus>(new Nexus(boost::shared_ptr<Player>(), cursorBuilding->getGridPoint(), true));
	}
	else if (newBuildType == BUILDINGTYPE_NODE) {
		cursorBuilding = boost::shared_ptr<Node>(new Node(boost::shared_ptr<Player>(), cursorBuilding->getGridPoint(), true));
	}
	else if (newBuildType == BUILDINGTYPE_GENERATOR) {
		cursorBuilding = boost::shared_ptr<Generator>(new Generator(boost::shared_ptr<Player>(), cursorBuilding->getGridPoint(), true));
	}
	else {
		assert(false);
	}
	buildType = newBuildType;
}

void createNewCursorBuilding() {
	if (buildType == BUILDINGTYPE_NEXUS) {
		cursorBuilding = boost::shared_ptr<Nexus>(new Nexus(boost::shared_ptr<Player>(), sf::Vector2i(0,0), true));
	}
	else if (buildType == BUILDINGTYPE_NODE) {
		cursorBuilding = boost::shared_ptr<Node>(new Node(boost::shared_ptr<Player>(), sf::Vector2i(0,0), true));
	}
	else if (buildType == BUILDINGTYPE_GENERATOR) {
		cursorBuilding = boost::shared_ptr<Generator>(new Generator(boost::shared_ptr<Player>(), sf::Vector2i(0,0), true));
	}
	else {
		assert(false);
	}
}

void go() {
	if (mode == MODE_BUILD) {
		//update cursorBuilding's position
		sf::Vector2i gridPoint = grid.getClosestGridPoint(sf::Mouse::getPosition());
		cursorBuilding->setGridPoint(gridPoint);
	}

	for (int i=0; i<buildings.size(); i++) {
		buildings[i]->go();
	}
	for (int i=0; i<players.size(); i++) {
		for (int j=0; j<players[i]->networks.size(); j++) {
			players[i]->networks[j]->go();
		}
	}
}

void draw() {
	//draw connections
	for (int i=0; i<buildings.size(); i++) {
		if (boost::shared_ptr<NodeBaseClass> node = boost::dynamic_pointer_cast<NodeBaseClass, Building>(buildings[i])) {
			node->drawConnections(&window, sf::Color(100, 100, 255));
		}
	}
	for (int i=0; i<buildings.size(); i++) {
		buildings[i]->draw(&window);
	}
	for (int i=0; i<selectedPlayer->ghostBuildings.size(); i++) {
		selectedPlayer->ghostBuildings[i]->draw(&window, sf::Color(170,170,170));
	}

	if (mode == MODE_BUILD) {
		cursorBuilding->draw(&window, sf::Color(100,100,100));
	}

	//draw debug info
	sf::Text text;
	text.setFont(font);
	text.setCharacterSize(12);
	text.setColor(sf::Color::White);

	stringstream s;
	for (int i=0; i<selectedPlayer->networks.size(); i++) {
		s << "Network " << i << ":" << endl << endl;

		s << "Energy available: " << selectedPlayer->networks[i]->energyAvailable << endl;
		s << "Energy spent: " << selectedPlayer->networks[i]->energySpent << endl;
		s << "Energy profit: " << selectedPlayer->networks[i]->energyProfit << endl;

		s << "Mass available: " << selectedPlayer->networks[i]->massAvailable << endl;
		s << "Mass spent: " << selectedPlayer->networks[i]->massSpent << endl;

		s << endl << endl;
	}

	text.setString(s.str());
	text.setPosition(10,10);
	window.draw(text);
}

int main (int argc, char **argv) {
	setup();
	start();

    sf::Event e;

    while (window.isOpen()) {

        while (window.pollEvent(e)) {
            switch (e.type) {
                case sf::Event::Closed:
                    window.close();
                    break;
				case sf::Event::TextEntered:
					{
						if (e.text.unicode >= '0' && e.text.unicode <= '9') {
							int num = e.text.unicode - '0';
							
							selectedPlayer = players[num];
						}
					}
					break;
				case sf::Event::KeyPressed:
					{
						if (e.key.code == sf::Keyboard::Escape) {
							if (mode == MODE_NULL) {
								window.close();
							}
							else {
								changeMode(MODE_NULL);
							}
						}
						else if (e.key.code == sf::Keyboard::N) {
							changeMode(MODE_BUILD);
							changeBuildType(BUILDINGTYPE_NEXUS);
						}
						else if (e.key.code == sf::Keyboard::Q) {
							changeMode(MODE_BUILD);
							changeBuildType(BUILDINGTYPE_NODE);
						}
						else if (e.key.code == sf::Keyboard::W) {
							changeMode(MODE_BUILD);
							changeBuildType(BUILDINGTYPE_GENERATOR);
						}
					}
					break;
				case sf::Event::MouseButtonPressed:
					{
						if (e.mouseButton.button == sf::Mouse::Right) {
							changeMode(MODE_NULL);
						}
						else if (e.mouseButton.button == sf::Mouse::Left) {
							cursorBuilding->setOwner(selectedPlayer);

							if (boost::shared_ptr<Nexus> newNexus = boost::dynamic_pointer_cast<Nexus, Building>(cursorBuilding)) {
								//check if this player already has a nexus
								bool hasNexus = false;
								for (int i=0; i<selectedPlayer->ownedBuildings.size(); i++) {
									if (boost::dynamic_pointer_cast<Nexus, Building>(selectedPlayer->ownedBuildings[i])) {
										hasNexus = true;
										break;
									}
								}

								if (!hasNexus) {
									newNexus->unGhost();
									newNexus->magicallyComplete();

									buildings.push_back(newNexus);
									selectedPlayer->ownedBuildings.push_back(newNexus);

									selectedPlayer->networks.push_back(boost::shared_ptr<Network>(new Network(selectedPlayer, newNexus)));
								}
							}
							selectedPlayer->ghostBuildings.push_back(cursorBuilding);
							registerNewGhostBuilding(selectedPlayer, cursorBuilding);
							createNewCursorBuilding();
						}
					}
					break;
            }
        }

		go();

        window.clear();

		draw();

        window.display();
    }
    return 0;
}