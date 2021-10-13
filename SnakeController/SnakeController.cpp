#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition = std::make_pair(foodX, foodY);

        istr >> d;
        switch (d) {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}

bool Controller::checkBiteSelf(std::list<Segment> &segments, const Segment& head) {
    for (auto segment : segments) {
        if (segment.x == head.x and segment.y == head.y) {
            return true;
        }
    }

    return false;
}

bool Controller::checkHitWall(const Segment& head, const std::pair<int, int>& mapDimension) const{
    if(head.x < 0 or head.y < 0){
        return true;
    }
    if(head.x >= mapDimension.first or head.y >= mapDimension.second){
        return true;
    }

    return false;
}

void Controller::shiftSnake(std::list<Segment>& segments, IPort& displayPort){
    for (auto &segment : segments) {
        if (not --segment.ttl) {
            DisplayInd l_evt{segment.x, segment.y, Cell_FREE};
            displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
        }
    }
}

bool Controller::checkGetFood(const Segment& head, const std::pair<int, int>& foodPosition){
    return std::make_pair(head.x, head.y) == foodPosition;
}

Segment Controller::getNewHead(const Segment& head) {
    auto dirLeft{m_currentDirection & Direction_LEFT};
    auto dirDown{m_currentDirection & Direction_DOWN};

    return {
        head.x + (dirLeft ? dirDown ? 1 : -1 : 0),
        head.y + (not dirLeft ? dirDown ? 1 : -1 : 0),
        head.ttl
    };
}

void Controller::displayNewHead(const Segment& head, IPort& displayPort){
    DisplayInd placeNewHead{ head.x, head.y, Cell_SNAKE };
    displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));
}

void Controller::removeOldElements(std::list<Segment>& segments){
    segments.erase(
            std::remove_if(
                    segments.begin(),
                    segments.end(),
                    [](auto const& segment){ return segment.ttl <= 0; }),
            segments.end());
}

void Controller::displayFood(const std::pair<int, int>& foodPosition, IPort& displayPort, Cell cell){
    DisplayInd clearOldFood{
        m_foodPosition.first,
        m_foodPosition.second,
        cell
    };

    displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));
}

void Controller::receive(std::unique_ptr<Event> e)
{
    try {
        auto const& timerEvent = *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        Segment newHead = getNewHead(m_segments.front());

        bool lost = checkBiteSelf(m_segments, newHead)
                || checkHitWall(newHead, m_mapDimension);

        if(lost){
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            return;
        }

        if (checkGetFood(newHead, m_foodPosition)) {
            m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
            m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        }
        else {
            shiftSnake(m_segments, m_displayPort);
        }

        m_segments.push_front(newHead);
        displayNewHead(newHead, m_displayPort);
        removeOldElements(m_segments);

    } catch (std::bad_cast&) {
        try {
            auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;

            if ((m_currentDirection & Direction_LEFT) != (direction & Direction_LEFT)) {
                m_currentDirection = direction;
            }
        } catch (std::bad_cast&) {
            try {
                auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);

                bool requestedFoodCollidedWithSnake = false;
                for (auto const& segment : m_segments) {
                    if (segment.x == receivedFood.x and segment.y == receivedFood.y) {
                        requestedFoodCollidedWithSnake = true;
                        break;
                    }
                }

                if (requestedFoodCollidedWithSnake) {
                    m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                } else {
                    displayFood(m_foodPosition, m_displayPort, Cell_FREE);
                    //displayFood({receivedFood.x, receivedFood.y}, m_displayPort, Cell_FOOD);

                    DisplayInd placeNewFood{
                        receivedFood.x,
                        receivedFood.y,
                        Cell_FOOD
                    };
                    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
                }

                m_foodPosition = std::make_pair(receivedFood.x, receivedFood.y);

            } catch (std::bad_cast&) {
                try {
                    auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);

                    bool requestedFoodCollidedWithSnake = false;
                    for (auto const& segment : m_segments) {
                        if (segment.x == requestedFood.x and segment.y == requestedFood.y) {
                            requestedFoodCollidedWithSnake = true;
                            break;
                        }
                    }

                    if (requestedFoodCollidedWithSnake) {
                        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                    } else {
                        DisplayInd placeNewFood{
                            requestedFood.x,
                            requestedFood.y,
                            Cell_FOOD
                        };

                        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
                    }

                    m_foodPosition = std::make_pair(requestedFood.x, requestedFood.y);
                } catch (std::bad_cast&) {
                    throw UnexpectedEventException();
                }
            }
        }
    }
}

} // namespace Snake
