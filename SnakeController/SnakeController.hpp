#pragma once

#include <list>
#include <memory>
#include <stdexcept>

#include "IEventHandler.hpp"
#include "SnakeInterface.hpp"

class Event;
class IPort;

namespace Snake
{

struct Segment
{
    int x;
    int y;
    int ttl;
};

struct ConfigurationError : std::logic_error
{
    ConfigurationError();
};

struct UnexpectedEventException : std::runtime_error
{
    UnexpectedEventException();
};

class Controller : public IEventHandler
{
public:
    Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config);

    Controller(Controller const& p_rhs) = delete;
    Controller& operator=(Controller const& p_rhs) = delete;

    void receive(std::unique_ptr<Event> e) override;

private:
    IPort& m_displayPort;
    IPort& m_foodPort;
    IPort& m_scorePort;

    std::pair<int, int> m_mapDimension;
    std::pair<int, int> m_foodPosition;

    Direction m_currentDirection;
    std::list<Segment> m_segments;
    bool checkBiteSelf(std::list<Segment>& segments, const Segment& newHead);
    bool checkHitWall(const Segment& head, const std::pair<int, int>& mapDimension) const;
    bool checkGetFood(const Segment& head, const std::pair<int, int>& foodPosition);
    void shiftSnake(std::list<Segment>& segments, IPort& displayPort);
    Segment getNewHead(const Segment& head);
    void displayNewHead(const Segment& head, IPort& displayPort);
    void removeOldElements(std::list<Segment>& segments);
    void displayFood(const std::pair<int, int>& foodPosition, IPort& displayPort, Cell cell);
};

} // namespace Snake
